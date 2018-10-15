/*
 * drivers/amlogic/spicc/spicc.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/of_irq.h>
#include "spicc.h"

/* #define CONFIG_SPICC_LOG */
#ifdef CONFIG_SPICC_LOG
struct my_log {
	struct timeval tv;
	unsigned int reg_val[13];
	unsigned int param[8];
	unsigned int param_count;
	const char *comment;
};
#endif

/**
 * struct spicc
 * @lock: spinlock for SPICC controller.
 * @msg_queue: link with the spi message list.
 * @wq: work queque
 * @work: work
 * @master: spi master alloc for this driver.
 * @spi: spi device on working.
 * @regs: the start register address of this SPICC controller.
 */
struct spicc {
	spinlock_t lock;
	struct list_head msg_queue;
	struct workqueue_struct *wq;
	struct work_struct work;
	struct spi_master *master;
	struct spi_device *spi;
	struct class cls;

	int device_id;
	struct reset_control *rst;
	struct clk *clk;
	struct clk *hclk;
	unsigned long clk_rate;
	void __iomem *regs;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pullup;
	struct pinctrl_state *pulldown;
	int bytes_per_word;
	int bit_offset;
	int mode;
	int speed;
	unsigned int dma_tx_threshold;
	unsigned int dma_rx_threshold;
	unsigned int dma_num_per_read_burst;
	unsigned int dma_num_per_write_burst;
	int irq;
	struct completion completion;
#define FLAG_DMA_EN 0
#define FLAG_DMA_AUTO_PARAM 1
#define FLAG_SSCTL 2
#define FLAG_ENHANCE 3
	unsigned int flags;
	u8 test_data;
	unsigned int delay_control;
	unsigned int cs_delay;
	unsigned int enhance_dlyctl;
	int remain;
	u8 *txp;
	u8 *rxp;
	int burst_len;
#ifdef CONFIG_SPICC_LOG
	struct my_log *log;
	int log_size;
	int log_count;
#endif
};

#ifdef CONFIG_SPICC_LOG
enum {
	PROBE_BEGIN,
	PROBE_END,
	REQUEST_IRQ,
	HAND_MSG_BEGIN,
	HAND_MSG_END,
	XFER_COMP_ISR,
	XFER_COMP_POLLING,
	DMA_BEGIN,
	DMA_END,
	PIO_BEGIN,
	PIO_END,
};

static const char * const log_comment[] = {
	"probe begin",
	"probe end",
	"request irq",
	"handle msg begin",
	"handle msg end",
	"xfer complete isr",
	"xfer complete polling",
	"dma begin",
	"dma end",
	"pio begin",
	"pio end"
};
static void spicc_log_init(struct spicc *spicc)
{
	spicc->log = 0;
	spicc->log_count = 0;
	if (spicc->log_size)
		spicc->log = kzalloc(spicc->log_size *
			sizeof(struct my_log), GFP_KERNEL);
	pr_info("spicc: log_size=%d, log=%p",
			spicc->log_size, spicc->log);
}

static void spicc_log_exit(struct spicc *spicc)
{
	spicc->log = 0;
	spicc->log_count = 0;
	spicc->log_size = 0;
	kfree(spicc->log);
}

static void spicc_log(
	struct spicc *spicc,
	unsigned int *param,
	int param_count,
	int comment_id)
{
	struct my_log *log;
	int i;

	if (IS_ERR_OR_NULL(spicc->log))
		return;
	log = &spicc->log[spicc->log_count];
	log->tv = ktime_to_timeval(ktime_get());
	for (i = 0; i < ARRAY_SIZE(log->reg_val); i++)
		log->reg_val[i] = readl(spicc->regs + ((i+2)<<2));

	if (param) {
		if (param_count > ARRAY_SIZE(log->param))
			param_count = ARRAY_SIZE(log->param);
		for (i = 0; i < param_count; i++)
			log->param[i] = param[i];
		log->param_count = param_count;
	}
	log->comment = log_comment[comment_id];

	if (++spicc->log_count > spicc->log_size)
		spicc->log_count = 0;
}

static void spicc_log_print(struct spicc *spicc)
{
	struct my_log *log;
	int i, j;
	unsigned int *p;

	if (IS_ERR_OR_NULL(spicc->log))
		return;
	pr_info("log total:%d\n", spicc->log_count);
	for (i = 0; i < spicc->log_count; i++) {
		log = &spicc->log[i];
		pr_info("[%6u.%6u]spicc-%d: %s\n",
				(unsigned int)log->tv.tv_sec,
				(unsigned int)log->tv.tv_usec,
				i, log->comment);
		p = log->reg_val;
		for (j = 0; j < ARRAY_SIZE(log->reg_val); j++)
			if (*p)
				pr_info("reg[%2d]=0x%x\n", j+2, *p++);
		p = log->param;
		for (j = 0; j < log->param_count; j++)
			pr_info("param[%d]=0x%x\n", j, *p++);
	}
}
#else
#define spicc_log_init(spicc)
#define spicc_log_exit(spicc)
#define spicc_log(spicc, param, param_count, comment_id)
#define spicc_log_print(spicc)
#endif

static void setb(void __iomem *mem_base,
				unsigned int bits_desc,
				unsigned int bits_val) {
	unsigned int mem_offset, val;
	unsigned int bits_offset, bits_mask;

	if (IS_ERR(mem_base))
		return;
	mem_offset = of_mem_offset(bits_desc);
	bits_offset = of_bits_offset(bits_desc);
	bits_mask = (1L<<of_bits_len(bits_desc))-1;
	val = readl(mem_base+mem_offset);
	val &= ~(bits_mask << bits_offset);
	val |= (bits_val & bits_mask) << bits_offset;
	writel(val, mem_base+mem_offset);
}

static unsigned int getb(void __iomem *mem_base,
				unsigned int bits_desc) {
	unsigned int mem_offset, val;
	unsigned int bits_offset, bits_mask;

	if (IS_ERR(mem_base))
		return -1;
	mem_offset = of_mem_offset(bits_desc);
	bits_offset = of_bits_offset(bits_desc);
	bits_mask = (1L<<of_bits_len(bits_desc))-1;
	val = readl(mem_base+mem_offset);
	return (val >> bits_offset) & bits_mask;
}

/* Note this is chip_select enable or disable */
void spicc_chip_select(struct spi_device *spi, bool select)
{
	struct spicc *spicc;
	unsigned int level;

	spicc = spi_master_get_devdata(spi->master);
	level = (spi->mode & SPI_CS_HIGH) ? select : !select;

	if (spi->mode & SPI_NO_CS)
		return;
	if (spi->cs_gpio >= 0) {
		gpio_direction_output(spi->cs_gpio, level);
	} else if (select) {
		setb(spicc->regs, CON_CHIP_SELECT, spi->chip_select);
		setb(spicc->regs, CON_SS_POL, level);
	}
}
EXPORT_SYMBOL(spicc_chip_select);

static inline bool spicc_get_flag(struct spicc *spicc, unsigned int flag)
{
	bool ret;

	ret = (spicc->flags >> flag) & 1;
	return ret;
}

static inline void spicc_set_flag(
		struct spicc *spicc,
		unsigned int flag,
		bool value)
{
	if (value)
		spicc->flags |= 1<<flag;
	else
		spicc->flags &= ~(1<<flag);
}

static inline void spicc_main_clk_ao(struct spicc *spicc, bool on)
{
	if (spicc_get_flag(spicc, FLAG_ENHANCE))
		setb(spicc->regs, MAIN_CLK_AO, on);
}

static inline void spicc_set_bit_width(struct spicc *spicc, u8 bw)
{
	setb(spicc->regs, CON_BITS_PER_WORD, bw-1);
	if (bw <= 8)
		spicc->bytes_per_word = 1;
	else if (bw <= 16)
		spicc->bytes_per_word = 2;
	else if (bw <= 32)
		spicc->bytes_per_word = 4;
	else
		spicc->bytes_per_word = 8;
	spicc->bit_offset = (spicc->bytes_per_word << 3) - bw;
}

static void spicc_set_mode(struct spicc *spicc, u8 mode)
{
	bool cpol = (mode & SPI_CPOL) ? 1:0;
	bool cpha = (mode & SPI_CPHA) ? 1:0;

	if (mode == spicc->mode)
		return;

	spicc_main_clk_ao(spicc, 1);
	spicc->mode = mode;
	if (!spicc_get_flag(spicc, FLAG_ENHANCE)) {
		if (cpol && !IS_ERR_OR_NULL(spicc->pullup))
			pinctrl_select_state(spicc->pinctrl, spicc->pullup);
		else if (!cpol && !IS_ERR_OR_NULL(spicc->pulldown))
			pinctrl_select_state(spicc->pinctrl, spicc->pulldown);
	}


	setb(spicc->regs, CON_CLK_PHA, cpha);
	setb(spicc->regs, CON_CLK_POL, cpol);
	setb(spicc->regs, LOOPBACK_EN, !!(mode & SPI_LOOP));
	spicc_main_clk_ao(spicc, 0);
}

static void spicc_set_clk(struct spicc *spicc, int speed)
{
	unsigned int sys_clk_rate;
	unsigned int div, mid_speed;

	if (!speed || (speed == spicc->speed))
		return;

	spicc->speed = speed;
	sys_clk_rate = clk_get_rate(spicc->clk);

	if (spicc_get_flag(spicc, FLAG_ENHANCE)) {
		div = sys_clk_rate/speed;
		if (div < 2)
			div = 2;
		div = (div >> 1) - 1;
		if (div > 0xff)
			div = 0xff;
		setb(spicc->regs, ENHANCE_CLK_DIV, div);
		setb(spicc->regs, ENHANCE_CLK_DIV_SELECT, 1);
	} else {
		/* speed = sys_clk_rate / 2^(conreg.data_rate_div+2) */
		mid_speed = (sys_clk_rate * 3) >> 4;
		for (div = 0; div < 7; div++) {
			if (speed >= mid_speed)
				break;
			mid_speed >>= 1;
		}
		setb(spicc->regs, CON_DATA_RATE_DIV, div);
	}
}

static inline void spicc_set_txfifo(struct spicc *spicc, u32 dat)
{
	writel(dat, spicc->regs + SPICC_REG_TXDATA);
}

static inline u32 spicc_get_rxfifo(struct spicc *spicc)
{
	return readl(spicc->regs + SPICC_REG_RXDATA);
}

static inline void spicc_reset_fifo(struct spicc *spicc)
{
	unsigned int dat;

	setb(spicc->regs, FIFO_RESET, 3);
	udelay(1);
	while (getb(spicc->regs, STA_RX_READY))
		dat = spicc_get_rxfifo(spicc);
}

static inline void spicc_enable(struct spicc *spicc, bool en)
{
	setb(spicc->regs, CON_ENABLE, en);
}

static void dma_one_burst(struct spicc *spicc)
{
	void __iomem *mem_base = spicc->regs;
	int bl, threshold = DMA_RX_FIFO_TH_MAX;

	setb(mem_base, STA_XFER_COM, 1);
	if (spicc->remain > 0) {
		bl = min_t(size_t, spicc->remain, BURST_LEN_MAX);
		if (bl != BURST_LEN_MAX) {
			bl = min_t(size_t, bl, DMA_RX_FIFO_TH_MAX);
			threshold =  bl;
		}
		setb(mem_base, CON_BURST_LEN, bl - 1);
		if (spicc_get_flag(spicc, FLAG_DMA_AUTO_PARAM)) {
			setb(mem_base, DMA_NUM_WR_BURST, threshold - 1);
			setb(mem_base, DMA_RX_FIFO_TH, threshold - 1);
		}
		spicc->remain -= bl;
		spicc->burst_len = bl;
		if (spicc->irq)
			enable_irq(spicc->irq);
		setb(mem_base, CON_XCH, 1);
	}
}

static inline u32 spicc_pull_data(struct spicc *spicc)
{
	int bytes = spicc->bytes_per_word;
	unsigned int dat = 0;
	int i;

	if (spicc->mode & SPI_LSB_FIRST)
		for (i = 0; i < bytes; i++) {
			dat <<= 8;
			dat += *spicc->txp++;
		}
	else
		for (i = 0; i < bytes; i++) {
			dat |= *spicc->txp << (i << 3);
			spicc->txp++;
		}
	dat >>= spicc->bit_offset;
	return dat;
}

static inline void spicc_push_data(struct spicc *spicc, u32 dat)
{
	int bytes = spicc->bytes_per_word;
	int i;

	dat <<= spicc->bit_offset;
	if (spicc->mode & SPI_LSB_FIRST)
		for (i = 0; i < bytes; i++)
			*spicc->rxp++ = dat >> ((bytes - i - 1) << 3);
	else
		for (i = 0; i < bytes; i++) {
			*spicc->rxp++ = dat & 0xff;
			dat >>= 8;
		}
}

static void pio_one_burst_recv(struct spicc *spicc)
{
	unsigned int dat;
	int i;

	for (i = 0; i < spicc->burst_len; i++) {
		dat = spicc_get_rxfifo(spicc);
		if (spicc->rxp)
			spicc_push_data(spicc, dat);
	}
}

static void pio_one_burst_send(struct spicc *spicc)
{
	void __iomem *mem_base = spicc->regs;
	unsigned int dat;
	int i;

	setb(mem_base, STA_XFER_COM, 1);
	if (spicc->remain > 0) {
		spicc->burst_len = min_t(size_t, spicc->remain,
				SPICC_FIFO_SIZE);
		for (i = 0; i < spicc->burst_len; i++) {
			dat = spicc->txp ? spicc_pull_data(spicc) : 0;
			spicc_set_txfifo(spicc, dat);
		}
		setb(mem_base, CON_BURST_LEN, spicc->burst_len - 1);
		spicc->remain -= spicc->burst_len;
		if (spicc->irq)
			enable_irq(spicc->irq);
		setb(mem_base, CON_XCH, 1);
	}
}

/**
 * Return: same with wait_for_completion_interruptible_timeout
 *   =0: timed out,
 *   >0: completed, number of usec left till timeout.
 */
static int spicc_wait_complete(struct spicc *spicc, int us)
{
	void __iomem *mem_base = spicc->regs;
	int i;

	for (i = 0; i < us; i++) {
		if (getb(mem_base, STA_XFER_COM)) {
			setb(spicc->regs, STA_XFER_COM, 1); /* set 1 to clear */
			break;
		}
		udelay(1);
	}
	return us - i;
}

static irqreturn_t spicc_xfer_complete_isr(int irq, void *dev_id)
{
	struct spicc *spicc = (struct spicc *)dev_id;
	unsigned long flags;

	spin_lock_irqsave(&spicc->lock, flags);
	disable_irq_nosync(spicc->irq);
	spicc_wait_complete(spicc, 100);
	spicc_log(spicc, &spicc->remain, 1, XFER_COMP_ISR);
	if (!spicc_get_flag(spicc, FLAG_DMA_EN))
		pio_one_burst_recv(spicc);
	if (!spicc->remain)
		complete(&spicc->completion);
	else if (spicc_get_flag(spicc, FLAG_DMA_EN))
		dma_one_burst(spicc);
	else
		pio_one_burst_send(spicc);
	spin_unlock_irqrestore(&spicc->lock, flags);
	return IRQ_HANDLED;
}

#define INVALID_DMA_ADDRESS 0xffffffff
/*
 * For DMA, tx_buf/tx_dma have the same relationship as rx_buf/rx_dma:
 *  - The buffer is either valid for CPU access, else NULL
 *  - If the buffer is valid, so is its DMA address
 *
 * This driver manages the dma address unless message->is_dma_mapped.
 */
static int spicc_dma_map(struct spicc *spicc, struct spi_transfer *t)
{
	struct device *dev = spicc->master->dev.parent;

	t->tx_dma = t->rx_dma = INVALID_DMA_ADDRESS;
	if (t->tx_buf) {
		t->tx_dma = dma_map_single(dev,
				(void *)t->tx_buf, t->len, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, t->tx_dma)) {
			dev_err(dev, "tx_dma map failed\n");
			return -ENOMEM;
		}
	}
	if (t->rx_buf) {
		t->rx_dma = dma_map_single(dev,
				t->rx_buf, t->len, DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, t->rx_dma)) {
			if (t->tx_buf) {
				dev_err(dev, "rx_dma map failed\n");
				dma_unmap_single(dev, t->tx_dma, t->len,
						DMA_TO_DEVICE);
			}
			return -ENOMEM;
		}
	}
	return 0;
}

static void spicc_dma_unmap(struct spicc *spicc, struct spi_transfer *t)
{
	struct device *dev = spicc->master->dev.parent;

	if (t->tx_dma != INVALID_DMA_ADDRESS)
		dma_unmap_single(dev, t->tx_dma, t->len, DMA_TO_DEVICE);
	if (t->rx_dma != INVALID_DMA_ADDRESS)
		dma_unmap_single(dev, t->rx_dma, t->len, DMA_FROM_DEVICE);
}

/**
 * Return:
 *   =0: xfer success,
 *   -ETIMEDOUT: timeout.
 */
static int spicc_dma_xfer(struct spicc *spicc, struct spi_transfer *t)
{
	void __iomem *mem_base = spicc->regs;
	int ret = 0;

	spicc_reset_fifo(spicc);
	setb(mem_base, CON_XCH, 0);
	setb(mem_base, WAIT_CYCLES, spicc->speed >> 25);
	spicc->remain = t->len / spicc->bytes_per_word;
	if (t->tx_dma != INVALID_DMA_ADDRESS)
		writel(t->tx_dma, mem_base + SPICC_REG_DRADDR);
	if (t->rx_dma != INVALID_DMA_ADDRESS)
		writel(t->rx_dma, mem_base + SPICC_REG_DWADDR);
	setb(mem_base, DMA_EN, 1);
	spicc_log(spicc, &spicc->remain, 1, DMA_BEGIN);
	if (spicc->irq) {
		setb(mem_base, INT_XFER_COM_EN, 1);
		dma_one_burst(spicc);
		ret = wait_for_completion_interruptible_timeout(
			&spicc->completion, msecs_to_jiffies(2000));
		setb(mem_base, INT_XFER_COM_EN, 0);
	} else {
		while (spicc->remain) {
			dma_one_burst(spicc);
			ret = spicc_wait_complete(spicc,
					spicc->burst_len << 13);
			spicc_log(spicc, &spicc->remain, 1, XFER_COMP_POLLING);
			if (!ret)
				break;
		}
	}
	setb(mem_base, DMA_EN, 0);
	ret = ret ? 0 : -ETIMEDOUT;
	spicc_log(spicc, &ret, 1, DMA_END);
	return ret;
}

/**
 * Return:
 *   =0: xfer success,
 *   -ETIMEDOUT: timeout.
 */
static int spicc_hw_xfer(struct spicc *spicc, u8 *txp, u8 *rxp, int len)
{
	void __iomem *mem_base = spicc->regs;
	int ret;

	spicc_reset_fifo(spicc);
	setb(mem_base, CON_XCH, 0);
	setb(mem_base, WAIT_CYCLES, 0);
	spicc->txp = txp;
	spicc->rxp = rxp;
	spicc->remain = len / spicc->bytes_per_word;
	spicc_log(spicc, &spicc->remain, 1, PIO_BEGIN);
	if (spicc->irq) {
		setb(mem_base, INT_XFER_COM_EN, 1);
		pio_one_burst_send(spicc);
		ret = wait_for_completion_interruptible_timeout(
			&spicc->completion, msecs_to_jiffies(2000));
		setb(mem_base, INT_XFER_COM_EN, 0);
	} else {
		while (spicc->remain) {
			pio_one_burst_send(spicc);
			ret = spicc_wait_complete(spicc,
					spicc->burst_len << 10);
			spicc_log(spicc, &spicc->remain, 1, XFER_COMP_POLLING);
			if (!ret)
				break;
			pio_one_burst_recv(spicc);
		}
	}
	ret = ret ? 0 : -ETIMEDOUT;
	spicc_log(spicc, &ret, 1, PIO_END);
	return ret;
}

static void spicc_hw_init(struct spicc *spicc)
{
	void __iomem *mem_base = spicc->regs;

	spicc->mode = -1;
	spicc->speed = -1;
	spicc_enable(spicc, 0);
	setb(mem_base, CLK_FREE_EN, 1);
	setb(mem_base, CON_MODE, 1); /* 0-slave, 1-master */
	setb(mem_base, CON_XCH, 0);
	setb(mem_base, CON_SMC, 0);
	setb(mem_base, CON_SS_CTL, spicc_get_flag(spicc, FLAG_SSCTL));
	setb(mem_base, CON_DRCTL, 0);
	spicc_enable(spicc, 1);
	setb(mem_base, DELAY_CONTROL, spicc->delay_control);
	writel((0x00<<26 |
					0x00<<20 |
					0x0<<19 |
					spicc->dma_num_per_write_burst<<15 |
					spicc->dma_num_per_read_burst<<11 |
					spicc->dma_rx_threshold<<6 |
					spicc->dma_tx_threshold<<1 |
					0x0<<0),
					mem_base + SPICC_REG_DMA);
	spicc_set_bit_width(spicc, SPICC_DEFAULT_BIT_WIDTH);
	spicc_set_clk(spicc, SPICC_DEFAULT_SPEED_HZ);
	if (spicc_get_flag(spicc, FLAG_ENHANCE)) {
		setb(spicc->regs, MOSI_OEN, 1);
		setb(spicc->regs, CLK_OEN, 1);
		setb(mem_base, CS_OEN, 1);
		setb(mem_base, CS_DELAY, spicc->cs_delay);
		setb(mem_base, CS_DELAY_EN, 1);
		writel(spicc->enhance_dlyctl,
			spicc->regs + SPICC_REG_ENHANCE_CNTL1);
	}
	spicc_set_mode(spicc, SPI_MODE_0);
	/* spicc_enable(spicc, 0); */
}


int dirspi_xfer(struct spi_device *spi, u8 *tx_buf, u8 *rx_buf, int len)
{
	struct spicc *spicc;

	spicc = spi_master_get_devdata(spi->master);
	return spicc_hw_xfer(spicc, tx_buf, rx_buf, len);
}
EXPORT_SYMBOL(dirspi_xfer);

int dirspi_write(struct spi_device *spi, u8 *buf, int len)
{
	struct spicc *spicc;

	spicc = spi_master_get_devdata(spi->master);
	return spicc_hw_xfer(spicc, buf, 0, len);
}
EXPORT_SYMBOL(dirspi_write);

int dirspi_read(struct spi_device *spi, u8 *buf, int len)
{
	struct spicc *spicc;

	spicc = spi_master_get_devdata(spi->master);
	return spicc_hw_xfer(spicc, 0, buf, len);
}
EXPORT_SYMBOL(dirspi_read);

void dirspi_start(struct spi_device *spi)
{
	spicc_chip_select(spi, 1);
}
EXPORT_SYMBOL(dirspi_start);

void dirspi_stop(struct spi_device *spi)
{
	spicc_chip_select(spi, 0);
}
EXPORT_SYMBOL(dirspi_stop);


/* setting clock and pinmux here */
static int spicc_setup(struct spi_device *spi)
{
	struct spicc *spicc;

	spicc = spi_master_get_devdata(spi->master);
	if (spi->cs_gpio >= 0)
		gpio_request(spi->cs_gpio, "spicc");
	dev_info(&spi->dev, "cs=%d cs_gpio=%d mode=0x%x speed=%d\n",
			spi->chip_select, spi->cs_gpio,
			spi->mode, spi->max_speed_hz);
	spicc_chip_select(spi, 0);
	spicc_set_bit_width(spicc,
			spi->bits_per_word ? : SPICC_DEFAULT_BIT_WIDTH);
	spicc_set_clk(spicc,
			spi->max_speed_hz ? : SPICC_DEFAULT_SPEED_HZ);
	spicc_set_mode(spicc, spi->mode);
	return 0;
}

static void spicc_cleanup(struct spi_device *spi)
{
	spicc_chip_select(spi, 0);
	if (spi->cs_gpio >= 0)
		gpio_free(spi->cs_gpio);
}

static void spicc_handle_one_msg(struct spicc *spicc, struct spi_message *m)
{
	struct spi_device *spi = m->spi;
	struct spi_transfer *t;
	int ret = 0;
	bool cs_changed = 0;

	spicc_log(spicc, 0, 0, HAND_MSG_BEGIN);
	/* spicc_enable(spicc, 1); */
	if (spi) {
		spicc_set_bit_width(spicc,
				spi->bits_per_word ? : SPICC_DEFAULT_BIT_WIDTH);
		spicc_set_clk(spicc,
				spi->max_speed_hz ? : SPICC_DEFAULT_SPEED_HZ);
		spicc_set_mode(spicc, spi->mode);
		spicc_chip_select(spi, 1);
	}
	list_for_each_entry(t, &m->transfers, transfer_list) {
		if (t->bits_per_word)
			spicc_set_bit_width(spicc, t->bits_per_word);
		if (t->speed_hz)
			spicc_set_clk(spicc, t->speed_hz);
		if (spi && cs_changed) {
			spicc_chip_select(spi, 1);
			cs_changed = 0;
		}
		if (t->delay_usecs >> 10)
			udelay(t->delay_usecs >> 10);

		spicc_set_flag(spicc, FLAG_DMA_EN, 0);
		if (t->bits_per_word == 64) {
			spicc_set_flag(spicc, FLAG_DMA_EN, 1);
			if (!m->is_dma_mapped)
				spicc_dma_map(spicc, t);
			ret = spicc_dma_xfer(spicc, t);
			if (!m->is_dma_mapped)
				spicc_dma_unmap(spicc, t);
		} else {
			ret = spicc_hw_xfer(spicc, (u8 *)t->tx_buf,
				(u8 *)t->rx_buf, t->len);
		}
		if (ret)
			goto spicc_handle_end;
		m->actual_length += t->len;

		/*
		 * to delay after this transfer before
		 * (optionally) changing the chipselect status.
		 */
		if (t->delay_usecs & 0x3ff)
			udelay(t->delay_usecs & 0x3ff);
		if (spi && t->cs_change) {
			spicc_chip_select(spi, 0);
			cs_changed = 1;
		}
	}

spicc_handle_end:
	if (spi)
		spicc_chip_select(spi, 0);
	/* spicc_enable(spicc, 0); */
	m->status = ret;
	if (m->context)
		m->complete(m->context);
	spicc_log(spicc, 0, 0, HAND_MSG_END);
}

static int spicc_transfer(struct spi_device *spi, struct spi_message *m)
{
	struct spicc *spicc = spi_master_get_devdata(spi->master);
	unsigned long flags;

	m->actual_length = 0;
	m->status = -EINPROGRESS;

	spin_lock_irqsave(&spicc->lock, flags);
	list_add_tail(&m->queue, &spicc->msg_queue);
	queue_work(spicc->wq, &spicc->work);
	spin_unlock_irqrestore(&spicc->lock, flags);

	return 0;
}

static void spicc_work(struct work_struct *work)
{
	struct spicc *spicc = container_of(work, struct spicc, work);
	struct spi_message *m;
	unsigned long flags;

	spin_lock_irqsave(&spicc->lock, flags);
	while (!list_empty(&spicc->msg_queue)) {
		m = container_of(spicc->msg_queue.next,
				struct spi_message, queue);
		list_del_init(&m->queue);
		spin_unlock_irqrestore(&spicc->lock, flags);
		spicc_handle_one_msg(spicc, m);
		spin_lock_irqsave(&spicc->lock, flags);
	}
	spin_unlock_irqrestore(&spicc->lock, flags);
}

static ssize_t show_setting(struct class *class,
	struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct spicc *spicc = container_of(class, struct spicc, cls);

	if (!strcmp(attr->attr.name, "test_data"))
		ret = sprintf(buf, "test_data=0x%x\n", spicc->test_data);
	else if (!strcmp(attr->attr.name, "help")) {
		pr_info("SPI device test help\n");
		pr_info("echo cs_gpio speed mode bits_per_word num");
		pr_info("[wbyte1 wbyte2...] >test\n");
	}
#ifdef CONFIG_SPICC_LOG
	else if (!strcmp(attr->attr.name, "log")) {
		spicc_log_print(spicc);
		spicc->log_count = 0;
	}
#endif
	return ret;
}


static ssize_t store_setting(
		struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	long value;
	struct spicc *spicc = container_of(class, struct spicc, cls);

	if (kstrtol(buf, 0, &value))
		return -EINVAL;
	if (!strcmp(attr->attr.name, "test_data"))
		spicc->test_data = (u8)(value & 0xff);
	return count;
}

static ssize_t store_test(
		struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct spicc *spicc = container_of(class, struct spicc, cls);
	struct device *dev = spicc->master->dev.parent;
	unsigned int cs_gpio, speed, mode, bits_per_word, num;
	u8 *tx_buf, *rx_buf;
	unsigned long value;
	char *kstr, *str_temp, *token;
	int i, ret;
	struct spi_transfer t;
	struct spi_message m;

	if (sscanf(buf, "%d%d%x%d%d", &cs_gpio, &speed,
			&mode, &bits_per_word, &num) != 5) {
		dev_err(dev, "error format\n");
		return count;
	}
	if (!speed || !num || !bits_per_word) {
		dev_err(dev, "error parameter\n");
		return count;
	}
	kstr = kstrdup(buf, GFP_KERNEL);
	tx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	rx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	if (IS_ERR_OR_NULL(kstr) ||
			IS_ERR_OR_NULL(tx_buf) ||
			IS_ERR_OR_NULL(rx_buf)) {
		dev_err(dev, "failed to alloc tx rx buffer\n");
		goto test_end;
	}

	str_temp = kstr;
	/* skip pass over "cs_gpio speed mode bits_per_word num" */
	for (i = 0; i < 5; i++)
		strsep(&str_temp, ", ");
	for (i = 0; i < num; i++) {
		token = strsep(&str_temp, ", ");
		if (!token || kstrtoul(token, 16, &value))
			break;
		tx_buf[i] = (u8)(value & 0xff);
	}
	for (; i < num; i++) {
		tx_buf[i] = spicc->test_data;
			spicc->test_data++;
	}

	spi_message_init(&m);
	m.spi = spi_alloc_device(spicc->master);
	if (cs_gpio < 1000)
		m.spi->cs_gpio = cs_gpio;
	else {
		m.spi->cs_gpio = -ENOENT;
		m.spi->chip_select = cs_gpio - 1000;
	}
	m.spi->max_speed_hz = speed;
	m.spi->mode = mode & 0xffff;
	m.spi->bits_per_word = bits_per_word;
	spicc_setup(m.spi);
	memset(&t, 0, sizeof(t));
	t.tx_buf = (void *)tx_buf;
	t.rx_buf = (void *)rx_buf;
	t.len = num;
	spi_message_add_tail(&t, &m);
	//spicc_handle_one_msg(spicc, &m);
	ret = spi_sync(m.spi, &m);
	spi_dev_put(m.spi);
	if (ret) {
		dev_err(dev, "transfer failed(%d)\n", ret);
		goto test_end;
	}

	if (mode & (SPI_LOOP | (1<<16))) {
		ret = 0;
		for (i = 0; i < num; i++) {
			if (tx_buf[i] != rx_buf[i]) {
				ret++;
				pr_info("[%d]: 0x%x, 0x%x\n",
					i, tx_buf[i], rx_buf[i]);
			}
		}
		dev_info(dev, "total %d, failed %d\n", num, ret);
	}
	dev_info(dev, "transfer ok\n");

test_end:
	kfree(kstr);
	kfree(tx_buf);
	kfree(rx_buf);
	return count;
}

static struct class_attribute spicc_class_attrs[] = {
	__ATTR(test, 0200, NULL, store_test),
	__ATTR(test_data, 0644, show_setting, store_setting),
	__ATTR(help, 0444, show_setting, NULL),
#ifdef CONFIG_SPICC_LOG
	__ATTR(log, 0444, show_setting, NULL),
#endif
	__ATTR_NULL
};


static int of_spicc_get_data(
		struct spicc *spicc,
		struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	int err;
	unsigned int value;

	err = of_property_read_u32(np, "device_id", &spicc->device_id);
	if (err) {
		dev_err(&pdev->dev, "get device_id fail\n");
		return err;
	}

	err = of_property_read_u32(np, "dma_auto_param", &value);
	spicc_set_flag(spicc, FLAG_DMA_AUTO_PARAM, err ? 0 : (!!value));
	dev_info(&pdev->dev, "dma_auto_param=%d\n",
		spicc_get_flag(spicc, FLAG_DMA_AUTO_PARAM));
	err = of_property_read_u32(np, "dma_tx_threshold", &value);
	spicc->dma_tx_threshold = err ? 3 : value;
	err = of_property_read_u32(np, "dma_rx_threshold", &value);
	spicc->dma_rx_threshold = err ? 3 : value;
	err = of_property_read_u32(np, "dma_num_per_read_burst", &value);
	spicc->dma_num_per_read_burst = err ? 3 : value;
	err = of_property_read_u32(np, "dma_num_per_write_burst", &value);
	spicc->dma_num_per_write_burst = err ? 3 : value;
	spicc->irq = irq_of_parse_and_map(np, 0);
	dev_info(&pdev->dev, "irq = 0x%x\n", spicc->irq);
	err = of_property_read_u32(np, "delay_control", &value);
	spicc->delay_control = err ? 0x15 : value;
	dev_info(&pdev->dev, "delay_control=%d\n", spicc->delay_control);
	err = of_property_read_u32(np, "enhance", &value);
	spicc_set_flag(spicc, FLAG_ENHANCE, err ? 0 : (!!value));
	dev_info(&pdev->dev, "enhance=%d\n",
			spicc_get_flag(spicc, FLAG_ENHANCE));
	err = of_property_read_u32(np, "cs_delay", &value);
	spicc->cs_delay = err ? 0 : value;
	err = of_property_read_u32(np, "ssctl", &value);
	spicc_set_flag(spicc, FLAG_SSCTL, err ? 0 : (!!value));
	err = of_property_read_u32(np, "clk_rate", &value);
	spicc->clk_rate = err ? 0 : value;
#ifdef CONFIG_SPICC_LOG
	err = of_property_read_u32(np, "log_size", &value);
	spicc->log_size = err ? 0 : value;
#endif

	spicc->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(spicc->pinctrl)) {
		dev_err(&pdev->dev, "get pinctrl fail\n");
		return PTR_ERR(spicc->pinctrl);
	}
	spicc->pullup = pinctrl_lookup_state(spicc->pinctrl, "default");
	if (!IS_ERR_OR_NULL(spicc->pullup))
		pinctrl_select_state(spicc->pinctrl, spicc->pullup);
	spicc->pullup = pinctrl_lookup_state(
			spicc->pinctrl, "spicc_pullup");
	spicc->pulldown = pinctrl_lookup_state(
			spicc->pinctrl, "spicc_pulldown");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	spicc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(spicc->regs)) {
		dev_err(&pdev->dev, "get resource fail\n");
		return PTR_ERR(spicc->regs);
	}

	spicc->rst = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR_OR_NULL(spicc->rst))
		dev_err(&pdev->dev, "get reset failed\n");
	else {
		reset_control_deassert(spicc->rst);
		dev_info(&pdev->dev, "get reset by default!\n");
	}

	spicc->clk = devm_clk_get(&pdev->dev, "spicc_clk");
	if (IS_ERR_OR_NULL(spicc->clk)) {
		dev_err(&pdev->dev, "get clk fail\n");
		return PTR_ERR(spicc->clk);
	}
	spicc->hclk = devm_clk_get(&pdev->dev, "cts_spicc_hclk");
	if (IS_ERR_OR_NULL(spicc->hclk))
		dev_err(&pdev->dev, "get cts_spicc_hclk failed\n");
	if (spicc->clk_rate) {
		clk_set_rate(spicc->clk, spicc->clk_rate);
		clk_prepare_enable(spicc->clk);
	}

	if (spicc_get_flag(spicc, FLAG_ENHANCE)) {
		err = of_property_read_u32(np, "enhance_dlyctl", &value);
		spicc->enhance_dlyctl = err ? 0 : value;
		dev_info(&pdev->dev, "enhance_dlyctl=0x%x\n",
			spicc->enhance_dlyctl);
	}
	return 0;
}

static int spicc_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct spicc *spicc;
	int i, ret;

	WARN_ON(!pdev->dev.of_node);
	master = spi_alloc_master(&pdev->dev, sizeof(*spicc));
	if (IS_ERR_OR_NULL(master)) {
		dev_err(&pdev->dev, "allocate spi master failed!\n");
		return -ENOMEM;
	}
	spicc = spi_master_get_devdata(master);
	spicc->master = master;
	dev_set_drvdata(&pdev->dev, spicc);
	ret = of_spicc_get_data(spicc, pdev);
	if (ret) {
		dev_err(&pdev->dev, "of error=%d\n", ret);
		goto err;
	}
	spicc_hw_init(spicc);
	spicc_log_init(spicc);
	spicc_log(spicc, 0, 0, PROBE_BEGIN);

	master->dev.of_node = pdev->dev.of_node;
	master->bus_num = spicc->device_id;
	master->setup = spicc_setup;
	master->transfer = spicc_transfer;
	master->cleanup = spicc_cleanup;
	/* DMA doesn't surpport LSB_FIRST.
	 * For PIO 16-bit and tx_buf={0x12,0x34}, byte order on mosi is:
	 *   MSB_FIRST: "0x34, 0x12"
	 *   LSB_FIRST: "0x12, 0x34"
	 * For PIO 16-bit rx and byte order on miso is "0x31, 0x32", spicc
	 * will fill rx buffer with "0x12,0x34" no matter MSB or LSB_FIRST.
	 */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH
			| SPI_LSB_FIRST | SPI_LOOP;
	ret = spi_register_master(master);
	if (ret < 0) {
		dev_err(&pdev->dev, "register spi master failed! (%d)\n", ret);
		goto err;
	}

	INIT_WORK(&spicc->work, spicc_work);
	spicc->wq = create_singlethread_workqueue(dev_name(master->dev.parent));
	if (spicc->wq == NULL) {
		ret = -EBUSY;
		goto err;
	}
	spin_lock_init(&spicc->lock);
	INIT_LIST_HEAD(&spicc->msg_queue);

	init_completion(&spicc->completion);
	if (spicc->irq) {
		if (request_irq(spicc->irq, spicc_xfer_complete_isr,
				IRQF_SHARED, "spicc", spicc)) {
			dev_err(&pdev->dev, "master %d request irq(%d) failed!\n",
					master->bus_num, spicc->irq);
			spicc->irq = 0;
		} else
			disable_irq_nosync(spicc->irq);
	}
	spicc_log(spicc, &spicc->irq, 1, REQUEST_IRQ);

	/*setup class*/
	spicc->cls.name = kzalloc(10, GFP_KERNEL);
	sprintf((char *)spicc->cls.name, "spicc%d", master->bus_num);
	spicc->cls.class_attrs = spicc_class_attrs;
	ret = class_register(&spicc->cls);
	if (ret < 0)
		dev_err(&pdev->dev, "register class failed! (%d)\n", ret);

	dev_info(&pdev->dev, "cs_num=%d\n", master->num_chipselect);
	if (master->cs_gpios) {
		for (i = 0; i < master->num_chipselect; i++)
			dev_info(&pdev->dev, "cs[%d]=%d\n", i,
					master->cs_gpios[i]);
	}
	spicc_log(spicc, 0, 0, PROBE_END);
	return ret;
err:
	spi_master_put(master);
	return ret;
}

static int spicc_remove(struct platform_device *pdev)
{
	struct spicc *spicc;

	spicc = (struct spicc *)dev_get_drvdata(&pdev->dev);
	spicc_log_exit(spicc);
	spi_unregister_master(spicc->master);
	destroy_workqueue(spicc->wq);
	if (spicc->pinctrl)
		devm_pinctrl_put(spicc->pinctrl);
	return 0;
}

static int spicc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct spicc *spicc;
	unsigned long flags;

	spicc = (struct spicc *)dev_get_drvdata(&pdev->dev);
	spin_lock_irqsave(&spicc->lock, flags);
	spin_unlock_irqrestore(&spicc->lock, flags);
	return 0;
}

static int spicc_resume(struct platform_device *pdev)
{
	struct spicc *spicc;
	unsigned long flags;

	spicc = (struct spicc *)dev_get_drvdata(&pdev->dev);
	spin_lock_irqsave(&spicc->lock, flags);
	spin_unlock_irqrestore(&spicc->lock, flags);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id spicc_of_match[] = {
	{	.compatible = "amlogic, spicc", },
	{},
};
#else
#define spicc_of_match NULL
#endif

static struct platform_driver spicc_driver = {
	.probe = spicc_probe,
	.remove = spicc_remove,
	.suspend = spicc_suspend,
	.resume = spicc_resume,
	.driver = {
			.name = "spicc",
			.of_match_table = spicc_of_match,
			.owner = THIS_MODULE,
		},
};

static int __init spicc_init(void)
{
	return platform_driver_register(&spicc_driver);
}

static void __exit spicc_exit(void)
{
	platform_driver_unregister(&spicc_driver);
}

subsys_initcall(spicc_init);
module_exit(spicc_exit);

MODULE_DESCRIPTION("Amlogic SPICC driver");
MODULE_LICENSE("GPL");
