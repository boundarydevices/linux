/*
 * Driver for Amlogic Meson SPI communication controller (SPICC)
 *
 * Copyright (C) BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/reset.h>
#include <linux/gpio.h>
#include <linux/dma-mapping.h>

/*
 * The Meson SPICC controller could support DMA based transfers, but is not
 * implemented by the vendor code, and while having the registers documentation
 * it has never worked on the GXL Hardware.
 * The PIO mode is the only mode implemented, and due to badly designed HW :
 * - all transfers are cutted in 16 words burst because the FIFO hangs on
 *   TX underflow, and there is no TX "Half-Empty" interrupt, so we go by
 *   FIFO max size chunk only
 * - CS management is dumb, and goes UP between every burst, so is really a
 *   "Data Valid" signal than a Chip Select, GPIO link should be used instead
 *   to have a CS go down over the full transfer
 */

/* Register Map */
#define SPICC_RXDATA	0x00

#define SPICC_TXDATA	0x04

#define SPICC_CONREG	0x08
#define SPICC_ENABLE		BIT(0)
#define SPICC_MODE_MASTER	BIT(1)
#define SPICC_XCH		BIT(2)
#define SPICC_SMC		BIT(3)
#define SPICC_POL		BIT(4)
#define SPICC_PHA		BIT(5)
#define SPICC_SSCTL		BIT(6)
#define SPICC_SSPOL		BIT(7)
#define SPICC_DRCTL_MASK	GENMASK(9, 8)
#define SPICC_DRCTL_IGNORE	0
#define SPICC_DRCTL_FALLING	1
#define SPICC_DRCTL_LOWLEVEL	2
#define SPICC_CS_MASK		GENMASK(13, 12)
#define SPICC_DATARATE_MASK	GENMASK(18, 16)
#define SPICC_FIX_FACTOR_MULT	1
#define SPICC_FIX_FACTOR_DIV	4
#define SPICC_BITLENGTH_MASK	GENMASK(24, 19)
#define SPICC_BURSTLENGTH_MASK	GENMASK(31, 25)

#define SPICC_INTREG	0x0c
#define SPICC_TE_EN	BIT(0) /* TX FIFO Empty Interrupt */
#define SPICC_TH_EN	BIT(1) /* TX FIFO Half-Full Interrupt */
#define SPICC_TF_EN	BIT(2) /* TX FIFO Full Interrupt */
#define SPICC_RR_EN	BIT(3) /* RX FIFO Ready Interrupt */
#define SPICC_RH_EN	BIT(4) /* RX FIFO Half-Full Interrupt */
#define SPICC_RF_EN	BIT(5) /* RX FIFO Full Interrupt */
#define SPICC_RO_EN	BIT(6) /* RX FIFO Overflow Interrupt */
#define SPICC_TC_EN	BIT(7) /* Transfert Complete Interrupt */

#define SPICC_DMAREG	0x10
#define SPICC_DMA_ENABLE		BIT(0)
/* When txfifo_count<threshold, request a read(dma->txfifo) burst */
#define SPICC_TXFIFO_THRESHOLD_MASK	GENMASK(5, 1)
#define SPICC_TXFIFO_THRESHOLD_DEFAULT	4
/* When rxfifo count>threshold, request a write(rxfifo->dma) burst */
#define SPICC_RXFIFO_THRESHOLD_MASK	GENMASK(10, 6)
#define SPICC_READ_BURST_MASK		GENMASK(14, 11)
#define SPICC_WRITE_BURST_MASK		GENMASK(18, 15)
#define SPICC_DMA_URGENT		BIT(19)
#define SPICC_DMA_THREADID_MASK		GENMASK(25, 20)
#define SPICC_DMA_BURSTNUM_MASK		GENMASK(31, 26)

#define SPICC_STATREG	0x14
#define SPICC_TE	BIT(0) /* TX FIFO Empty Interrupt */
#define SPICC_TH	BIT(1) /* TX FIFO Half-Full Interrupt */
#define SPICC_TF	BIT(2) /* TX FIFO Full Interrupt */
#define SPICC_RR	BIT(3) /* RX FIFO Ready Interrupt */
#define SPICC_RH	BIT(4) /* RX FIFO Half-Full Interrupt */
#define SPICC_RF	BIT(5) /* RX FIFO Full Interrupt */
#define SPICC_RO	BIT(6) /* RX FIFO Overflow Interrupt */
#define SPICC_TC	BIT(7) /* Transfert Complete Interrupt */

#define SPICC_PERIODREG	0x18
#define SPICC_PERIOD	GENMASK(14, 0)	/* Wait cycles */

#define SPICC_TESTREG	0x1c
#define SPICC_TXCNT_MASK	GENMASK(4, 0)	/* TX FIFO Counter */
#define SPICC_RXCNT_MASK	GENMASK(9, 5)	/* RX FIFO Counter */
#define SPICC_SMSTATUS_MASK	GENMASK(12, 10)	/* State Machine Status */
#define SPICC_LBC		BIT(14) /* Loop Back Control */
#define SPICC_SWAP		BIT(15) /* RX FIFO Data Swap */
#define SPICC_MO_DELAY_MASK	GENMASK(17, 16) /* Master Output Delay */
#define SPICC_MO_NO_DELAY	0
#define SPICC_MO_DELAY_1_CYCLE	1
#define SPICC_MO_DELAY_2_CYCLE	2
#define SPICC_MO_DELAY_3_CYCLE	3
#define SPICC_MI_DELAY_MASK	GENMASK(19, 18) /* Master Input Delay */
#define SPICC_MI_NO_DELAY	0
#define SPICC_MI_DELAY_1_CYCLE	1
#define SPICC_MI_DELAY_2_CYCLE	2
#define SPICC_MI_DELAY_3_CYCLE	3
#define SPICC_MI_CAP_DELAY_MASK	GENMASK(21, 20) /* Master Capture Delay */
#define SPICC_CAP_AHEAD_2_CYCLE	0
#define SPICC_CAP_AHEAD_1_CYCLE	1
#define SPICC_CAP_NO_DELAY	2
#define SPICC_CAP_DELAY_1_CYCLE	3
#define SPICC_FIFORST_MASK	GENMASK(23, 22) /* FIFO Softreset */

#define SPICC_DRADDR	0x20	/* Read Address of DMA */

#define SPICC_DWADDR	0x24	/* Write Address of DMA */

#define SPICC_ENH_CTL0	0x38	/* Enhanced Feature 0 */
#define SPICC_ENH_CS_PRE_DELAY_MASK	GENMASK(15, 0)
#define SPICC_ENH_DATARATE_MASK		GENMASK(23, 16)
#define SPICC_ENH_FIX_FACTOR_MULT	1
#define SPICC_ENH_FIX_FACTOR_DIV	2
#define SPICC_ENH_DATARATE_EN		BIT(24)
#define SPICC_ENH_MOSI_OEN		BIT(25)
#define SPICC_ENH_CLK_OEN		BIT(26)
#define SPICC_ENH_CS_OEN		BIT(27)
#define SPICC_ENH_CS_PRE_DELAY_EN	BIT(28)
#define SPICC_ENH_MAIN_CLK_AO		BIT(29)

#define SPICC_ENH_CTL1	0x3c	/* Enhanced Feature 1 */
#define SPICC_ENH_MI_CAP_DELAY_EN	BIT(0)
#define SPICC_ENH_MI_CAP_DELAY_MASK	GENMASK(9, 1)
#define SPICC_ENH_SI_CAP_DELAY_EN	BIT(14)		/* slave mode */
#define SPICC_ENH_DELAY_EN		BIT(15)
#define SPICC_ENH_SI_DELAY_EN		BIT(16)		/* slave mode */
#define SPICC_ENH_SI_DELAY_MASK		GENMASK(19, 17)	/* slave mode */
#define SPICC_ENH_MI_DELAY_EN		BIT(20)
#define SPICC_ENH_MI_DELAY_MASK		GENMASK(23, 21)
#define SPICC_ENH_MO_DELAY_EN		BIT(24)
#define SPICC_ENH_MO_DELAY_MASK		GENMASK(27, 25)
#define SPICC_ENH_MO_OEN_DELAY_EN	BIT(28)
#define SPICC_ENH_MO_OEN_DELAY_MASK	GENMASK(31, 29)

#define SPICC_ENH_CTL2	0x40	/* Enhanced Feature */
#define SPICC_ENH_TI_DELAY_MASK		GENMASK(14, 0)
#define SPICC_ENH_TI_DELAY_EN		BIT(15)
#define SPICC_ENH_TT_DELAY_MASK		GENMASK(30, 16)
#define SPICC_ENH_TT_DELAY_EN		BIT(31)

#define writel_bits_relaxed(mask, val, addr) \
	writel_relaxed((readl_relaxed(addr) & ~(mask)) | (val), addr)
#define readl_bits_relaxed(mask, addr) \
	((readl_relaxed(addr) & (mask)) >> (ffs(mask) - 1))
#define mask_width(mask) (fls(mask) + 1 - ffs(mask))

/* only for local test, must remove it before upstream pushing */
#define MESON_SPICC_TEST_ENTRY

/*
 * cs_pre_delay: delay time from SS falling edge to the first CLK edge.
 * mo_delay: MOSI output delay time.
 * mi_delay: MISO input delay time.
 * mi_capture_delay: MISO capture delay time.
 * tt_delay: trailing time from the last CLK edge to the SS rising edge.
 * ti_delay: idling time between transfers.
 */
struct meson_spicc_controller_data {
	unsigned int			cs_pre_delay;
	unsigned int			tt_delay;
	unsigned int			ti_delay;
};

struct meson_spicc_data {
	unsigned int			min_speed_hz;
	unsigned int			max_speed_hz;
	unsigned int			fifo_size;
	bool				dma_burst_triggered_by_ssctl;
	bool				has_oen;
	bool				has_enhance_clk_div;
	bool				has_cs_pre_delay;
	bool				has_enhance_io_delay;
	bool				has_comp_clk;
	bool				is_div_parent_comp_clk;
	bool				has_enhance_tt_ti_delay;
};

struct meson_spicc_device {
	struct spi_master		*master;
	struct platform_device		*pdev;
	void __iomem			*base;
	struct clk			*core;
	struct clk			*comp;
	struct clk			*clk;
	struct spi_message		*message;
	struct spi_transfer		*xfer;
	const struct meson_spicc_data	*data;
	u8				*tx_buf;
	u8				*rx_buf;
	unsigned int			bytes_per_word;
	unsigned int			speed_hz;
	unsigned long			tx_remain;
	unsigned long			txb_remain;
	unsigned long			rx_remain;
	unsigned long			rxb_remain;
	unsigned long			xfer_remain;
	bool				using_dma;
#ifdef MESON_SPICC_TEST_ENTRY
	struct				class cls;
	u8				test_data;
#endif
};

static void meson_spicc_oen_enable(struct meson_spicc_device *spicc)
{
	u32 conf;

	if (!spicc->data->has_oen)
		return;

	conf = readl_relaxed(spicc->base + SPICC_ENH_CTL0) |
		SPICC_ENH_MOSI_OEN | SPICC_ENH_CLK_OEN | SPICC_ENH_CS_OEN;

	writel_relaxed(conf, spicc->base + SPICC_ENH_CTL0);
}

static void meson_spicc_setup_controller_data(
		struct meson_spicc_device *spicc,
		struct spi_device *spi)
{
	struct meson_spicc_controller_data *cd;
	u32 conf;

	cd = (struct meson_spicc_controller_data *)spi->controller_data;
	if (!cd)
		return;

	/* setup cs preload delay for ss chip-select */
	if (spicc->data->has_cs_pre_delay && !gpio_is_valid(spi->cs_gpio)) {
		conf = readl_relaxed(spicc->base + SPICC_ENH_CTL0);
		conf &= ~(SPICC_ENH_CS_PRE_DELAY_MASK |
			  SPICC_ENH_CS_PRE_DELAY_EN);
		if (cd->cs_pre_delay) {
			conf |= SPICC_ENH_CS_PRE_DELAY_EN;
			conf |= FIELD_PREP(SPICC_ENH_CS_PRE_DELAY_MASK,
					   cd->cs_pre_delay);
		}
		writel_relaxed(conf, spicc->base + SPICC_ENH_CTL0);
	}

	if (spicc->data->has_enhance_tt_ti_delay) {
		conf = 0;
		if (cd->tt_delay) {
			conf |= SPICC_ENH_TT_DELAY_EN;
			conf |= FIELD_PREP(SPICC_ENH_TT_DELAY_MASK,
					   cd->tt_delay);
		}
		if (cd->ti_delay) {
			conf |= SPICC_ENH_TI_DELAY_EN;
			conf |= FIELD_PREP(SPICC_ENH_TI_DELAY_MASK,
					   cd->ti_delay);
		}
		writel_relaxed(conf, spicc->base + SPICC_ENH_CTL2);
	}
}

static void meson_spicc_auto_io_delay(struct meson_spicc_device *spicc)
{
	u32 div, hz;
	u32 mi_delay, cap_delay;
	u32 conf;

	if (spicc->data->has_enhance_clk_div) {
		div = readl_bits_relaxed(SPICC_ENH_DATARATE_MASK,
				spicc->base + SPICC_ENH_CTL0);
		div++;
		div <<= 1;
	} else {
		div = readl_bits_relaxed(SPICC_DATARATE_MASK,
				spicc->base + SPICC_CONREG);
		div += 2;
		div = 1 << div;
	}

	mi_delay = SPICC_MI_NO_DELAY;
	cap_delay = SPICC_CAP_AHEAD_2_CYCLE;
	hz = clk_get_rate(spicc->clk);

	if (spicc->message->spi->mode & SPI_LOOP)
		cap_delay = SPICC_CAP_AHEAD_1_CYCLE;
	else if (hz >= 100000000)
		cap_delay = SPICC_CAP_DELAY_1_CYCLE;
	else if (hz >= 80000000)
		cap_delay = SPICC_CAP_NO_DELAY;
	else if (hz >= 40000000)
		cap_delay = SPICC_CAP_AHEAD_1_CYCLE;
	else if (div >= 16)
		mi_delay = SPICC_MI_DELAY_3_CYCLE;
	else if (div >= 8)
		mi_delay = SPICC_MI_DELAY_2_CYCLE;
	else if (div >= 6)
		mi_delay = SPICC_MI_DELAY_1_CYCLE;

	conf = readl_relaxed(spicc->base + SPICC_TESTREG);
	conf &= ~(SPICC_MO_DELAY_MASK | SPICC_MI_DELAY_MASK
		  | SPICC_MI_CAP_DELAY_MASK);
	conf |= FIELD_PREP(SPICC_MI_DELAY_MASK, mi_delay);
	conf |= FIELD_PREP(SPICC_MI_CAP_DELAY_MASK, cap_delay);
	writel_relaxed(conf, spicc->base + SPICC_TESTREG);
}

static int meson_spicc_dma_map(struct meson_spicc_device *spicc,
			       struct spi_transfer *t)
{
	struct device *dev = spicc->master->dev.parent;

	t->tx_dma = dma_map_single(dev, (void *)t->tx_buf, t->len,
				   DMA_TO_DEVICE);
	if (dma_mapping_error(dev, t->tx_dma)) {
		dev_err(dev, "tx_dma map failed\n");
		return -ENOMEM;
	}

	t->rx_dma = dma_map_single(dev, t->rx_buf, t->len, DMA_FROM_DEVICE);
	if (dma_mapping_error(dev, t->rx_dma)) {
		dma_unmap_single(dev, t->tx_dma, t->len, DMA_TO_DEVICE);
		dev_err(dev, "rx_dma map failed\n");
		return -ENOMEM;
	}

	return 0;
}

static void meson_spicc_dma_unmap(struct meson_spicc_device *spicc,
				  struct spi_transfer *t)
{
	struct device *dev = spicc->master->dev.parent;

	dma_unmap_single(dev, t->tx_dma, t->len, DMA_TO_DEVICE);
	dma_unmap_single(dev, t->rx_dma, t->len, DMA_FROM_DEVICE);
}

static void meson_spicc_setup_dma_burst(struct meson_spicc_device *spicc)
{
	unsigned int burst_len, thres;
	unsigned int fifo_size = spicc->data->fifo_size;

	burst_len = min_t(unsigned int,
			  spicc->xfer_remain / spicc->bytes_per_word,
			  fifo_size << 3);

	thres = burst_len % fifo_size;
	if (thres == 0)
		thres = fifo_size;
	else if (burst_len != thres) {
		burst_len -= thres;
		thres = fifo_size;
	}

	/* Setup Xfer variables */
	spicc->xfer_remain -= burst_len * spicc->bytes_per_word;

	/* Setup burst length */
	writel_bits_relaxed(SPICC_BURSTLENGTH_MASK,
			FIELD_PREP(SPICC_BURSTLENGTH_MASK,
				burst_len - 1),
			spicc->base + SPICC_CONREG);

	writel_relaxed(SPICC_DMA_ENABLE
		| FIELD_PREP(SPICC_TXFIFO_THRESHOLD_MASK,
			     SPICC_TXFIFO_THRESHOLD_DEFAULT)
		| FIELD_PREP(SPICC_READ_BURST_MASK,
			     fifo_size - SPICC_TXFIFO_THRESHOLD_DEFAULT)
		| FIELD_PREP(SPICC_RXFIFO_THRESHOLD_MASK, thres - 1)
		| FIELD_PREP(SPICC_WRITE_BURST_MASK, thres - 1),
		spicc->base + SPICC_DMAREG);
}

static inline bool meson_spicc_txfull(struct meson_spicc_device *spicc)
{
	return !!FIELD_GET(SPICC_TF,
			   readl_relaxed(spicc->base + SPICC_STATREG));
}

static inline bool meson_spicc_rxready(struct meson_spicc_device *spicc)
{
	return FIELD_GET(SPICC_RH | SPICC_RR | SPICC_RF,
			 readl_relaxed(spicc->base + SPICC_STATREG));
}

static void meson_spicc_reset_fifo(struct meson_spicc_device *spicc)
{
	u32 data;

	if (spicc->data->has_oen)
		writel_bits_relaxed(SPICC_ENH_MAIN_CLK_AO,
				    SPICC_ENH_MAIN_CLK_AO,
				    spicc->base + SPICC_ENH_CTL0);

	writel_bits_relaxed(SPICC_FIFORST_MASK,
			FIELD_PREP(SPICC_FIFORST_MASK, 3),
			spicc->base + SPICC_TESTREG);

	while (meson_spicc_rxready(spicc))
		data = readl_relaxed(spicc->base + SPICC_RXDATA);

	if (spicc->data->has_oen)
		writel_bits_relaxed(SPICC_ENH_MAIN_CLK_AO, 0,
				    spicc->base + SPICC_ENH_CTL0);
}

static inline u32 meson_spicc_pull_data(struct meson_spicc_device *spicc)
{
	unsigned int bytes = spicc->bytes_per_word;
	unsigned int byte_shift = 0;
	u32 data = 0;
	u8 byte;

	while (bytes--) {
		byte = *spicc->tx_buf++;
		data |= (byte & 0xff) << byte_shift;
		byte_shift += 8;
	}

	spicc->tx_remain--;
	return data;
}

static inline void meson_spicc_push_data(struct meson_spicc_device *spicc,
					 u32 data)
{
	unsigned int bytes = spicc->bytes_per_word;
	unsigned int byte_shift = 0;
	u8 byte;

	while (bytes--) {
		byte = (data >> byte_shift) & 0xff;
		*spicc->rx_buf++ = byte;
		byte_shift += 8;
	}

	spicc->rx_remain--;
}

static inline void meson_spicc_rx(struct meson_spicc_device *spicc)
{
	/* Empty RX FIFO */
	while (spicc->rx_remain &&
	       meson_spicc_rxready(spicc))
		meson_spicc_push_data(spicc,
				readl_relaxed(spicc->base + SPICC_RXDATA));
}

static inline void meson_spicc_tx(struct meson_spicc_device *spicc)
{
	/* Fill Up TX FIFO */
	while (spicc->tx_remain &&
	       !meson_spicc_txfull(spicc))
		writel_relaxed(meson_spicc_pull_data(spicc),
			       spicc->base + SPICC_TXDATA);
}

static void meson_spicc_setup_pio_burst(struct meson_spicc_device *spicc)
{
	unsigned int burst_len;

	burst_len = min_t(unsigned int,
			  spicc->xfer_remain / spicc->bytes_per_word,
			  spicc->data->fifo_size);

	/* Setup Xfer variables */
	spicc->tx_remain = burst_len;
	spicc->rx_remain = burst_len;
	spicc->xfer_remain -= burst_len * spicc->bytes_per_word;

	/* Setup burst length */
	writel_bits_relaxed(SPICC_BURSTLENGTH_MASK,
			FIELD_PREP(SPICC_BURSTLENGTH_MASK,
				burst_len - 1),
			spicc->base + SPICC_CONREG);

	/* Fill TX FIFO */
	meson_spicc_tx(spicc);
}

static irqreturn_t meson_spicc_irq(int irq, void *data)
{
	struct meson_spicc_device *spicc = (void *) data;

	writel_bits_relaxed(SPICC_TC, SPICC_TC, spicc->base + SPICC_STATREG);

	if (!spicc->using_dma)
		/* Empty RX FIFO */
		meson_spicc_rx(spicc);

	if (!spicc->xfer_remain) {
		/* Disable all IRQs */
		writel(0, spicc->base + SPICC_INTREG);
		if (spicc->using_dma) {
			spicc->using_dma = 0;
			writel_bits_relaxed(SPICC_DMA_ENABLE, 0,
					    spicc->base + SPICC_DMAREG);
			if (!spicc->message->is_dma_mapped)
				meson_spicc_dma_unmap(spicc, spicc->xfer);
		}
		spi_finalize_current_transfer(spicc->master);

		return IRQ_HANDLED;
	}

	/* Setup burst */
	if (spicc->using_dma)
		meson_spicc_setup_dma_burst(spicc);
	else
		meson_spicc_setup_pio_burst(spicc);

	/* Start burst */
	writel_bits_relaxed(SPICC_XCH, SPICC_XCH, spicc->base + SPICC_CONREG);

	return IRQ_HANDLED;
}

static void meson_spicc_setup_xfer(struct meson_spicc_device *spicc,
				   struct spi_transfer *xfer)
{
	u32 conf, conf_orig;

	meson_spicc_reset_fifo(spicc);

	/* Read original configuration */
	conf = conf_orig = readl_relaxed(spicc->base + SPICC_CONREG);

	/* Setup word width */
	conf &= ~SPICC_BITLENGTH_MASK;
	conf |= FIELD_PREP(SPICC_BITLENGTH_MASK,
			   (spicc->bytes_per_word << 3) - 1);

	/* Ignore if unchanged */
	if (conf != conf_orig)
		writel_relaxed(conf, spicc->base + SPICC_CONREG);

	if (spicc->speed_hz != xfer->speed_hz) {
		spicc->speed_hz = xfer->speed_hz;
		clk_set_rate(spicc->clk, xfer->speed_hz);
	}
	meson_spicc_auto_io_delay(spicc);

	spicc->using_dma = 0;
	if ((xfer->bits_per_word == 64)
	    && (spicc->message->is_dma_mapped
		|| !meson_spicc_dma_map(spicc, xfer))) {
		spicc->using_dma = 1;
		writel_relaxed(xfer->tx_dma, spicc->base + SPICC_DRADDR);
		writel_relaxed(xfer->rx_dma, spicc->base + SPICC_DWADDR);
		writel_relaxed(xfer->speed_hz >> 25,
			       spicc->base + SPICC_PERIODREG);
	}
	writel_relaxed(0, spicc->base + SPICC_DMAREG);
}

static int meson_spicc_transfer_one(struct spi_master *master,
				    struct spi_device *spi,
				    struct spi_transfer *xfer)
{
	struct meson_spicc_device *spicc = spi_master_get_devdata(master);

	/* Store current transfer */
	spicc->xfer = xfer;

	/* Setup transfer parameters */
	spicc->tx_buf = (u8 *)xfer->tx_buf;
	spicc->rx_buf = (u8 *)xfer->rx_buf;
	spicc->xfer_remain = xfer->len;

	/* Pre-calculate word size */
	spicc->bytes_per_word =
	   DIV_ROUND_UP(spicc->xfer->bits_per_word, 8);

	if (xfer->len % spicc->bytes_per_word)
		return -EINVAL;

	/* Setup transfer parameters */
	meson_spicc_setup_xfer(spicc, xfer);

	/* Setup burst */
	if (spicc->using_dma)
		meson_spicc_setup_dma_burst(spicc);
	else
		meson_spicc_setup_pio_burst(spicc);

	/* Start burst */
	writel_bits_relaxed(SPICC_XCH, SPICC_XCH, spicc->base + SPICC_CONREG);

	/* Enable interrupts */
	writel_relaxed(SPICC_TC_EN, spicc->base + SPICC_INTREG);

	return 1;
}

static int meson_spicc_prepare_message(struct spi_master *master,
				       struct spi_message *message)
{
	struct meson_spicc_device *spicc = spi_master_get_devdata(master);
	struct spi_device *spi = message->spi;
	u32 conf = 0;

	/* Store current message */
	spicc->message = message;

	/* Enable Master */
	conf |= SPICC_ENABLE;
	conf |= SPICC_MODE_MASTER;

	if (spicc->data->dma_burst_triggered_by_ssctl)
		conf |= SPICC_SSCTL;

	/* SMC = 0 */

	/* Setup transfer mode */
	if (spi->mode & SPI_CPOL)
		conf |= SPICC_POL;
	else
		conf &= ~SPICC_POL;

	if (spi->mode & SPI_CPHA)
		conf |= SPICC_PHA;
	else
		conf &= ~SPICC_PHA;

	/* SSCTL = 0 */

	if (spi->mode & SPI_CS_HIGH)
		conf |= SPICC_SSPOL;
	else
		conf &= ~SPICC_SSPOL;

	if (spi->mode & SPI_READY)
		conf |= FIELD_PREP(SPICC_DRCTL_MASK, SPICC_DRCTL_LOWLEVEL);
	else
		conf |= FIELD_PREP(SPICC_DRCTL_MASK, SPICC_DRCTL_IGNORE);

	/* Select CS */
	conf |= FIELD_PREP(SPICC_CS_MASK, spi->chip_select);

	/* Default 8bit word */
	conf |= FIELD_PREP(SPICC_BITLENGTH_MASK, 8 - 1);

	writel_relaxed(conf, spicc->base + SPICC_CONREG);

	/* Setup no wait cycles by default */
	writel_relaxed(0, spicc->base + SPICC_PERIODREG);

	meson_spicc_oen_enable(spicc);

	conf = readl_relaxed(spicc->base + SPICC_TESTREG);
	conf &= ~SPICC_LBC;
	if (spi->mode & SPI_LOOP)
		conf |= SPICC_LBC;
	writel_relaxed(conf, spicc->base + SPICC_TESTREG);

	/* setup cs-preload, input/output/capture delay */
	meson_spicc_setup_controller_data(spicc, spi);

	return 0;
}

static int meson_spicc_unprepare_transfer(struct spi_master *master)
{
	struct meson_spicc_device *spicc = spi_master_get_devdata(master);

	/* Disable all IRQs */
	writel(0, spicc->base + SPICC_INTREG);

	device_reset_optional(&spicc->pdev->dev);

	return 0;
}

static int meson_spicc_setup(struct spi_device *spi)
{
	int ret = 0;

	if (!gpio_is_valid(spi->cs_gpio))
		return 0;

	if (!spi->controller_state) {
		ret = gpio_request(spi->cs_gpio, dev_name(&spi->dev));
		if (ret) {
			dev_err(&spi->dev, "failed to request cs gpio\n");
			return ret;
		}
		spi->controller_state = spi_master_get_devdata(spi->master);
	}

	ret = gpio_direction_output(spi->cs_gpio,
			!(spi->mode & SPI_CS_HIGH));

	return ret;
}

static void meson_spicc_cleanup(struct spi_device *spi)
{
	if (gpio_is_valid(spi->cs_gpio))
		gpio_free(spi->cs_gpio);

	spi->controller_state = NULL;
}

#ifdef MESON_SPICC_TEST_ENTRY
static struct meson_spicc_controller_data cd = {0};
static ssize_t store_setting(
		struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	if (!strcmp(attr->attr.name, "controller_data"))
		sscanf(buf, "%d%d%d", &cd.cs_pre_delay,
			   &cd.tt_delay, &cd.ti_delay);

	return count;
}

static ssize_t show_setting(
		struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	if (!strcmp(attr->attr.name, "controller_data")) {
		pr_info("cs_pre_delay = %d\n", cd.cs_pre_delay);
		pr_info("tt_delay = %d\n", cd.tt_delay);
		pr_info("ti_delay = %d\n", cd.ti_delay);
	}

	return 0;
}

#define TEST_PARAM_NUM 5
static ssize_t store_test(
		struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct meson_spicc_device *spicc = container_of(
		class, struct meson_spicc_device, cls);
	struct device *dev = spicc->master->dev.parent;
	unsigned int cs_gpio, speed, mode, bits_per_word, num;
	u8 *tx_buf, *rx_buf;
	unsigned long value;
	char *kstr, *str_temp, *token;
	int i, ret;
	struct spi_transfer t;
	struct spi_message m;

	if (sscanf(buf, "%d%d%x%d%d", &cs_gpio, &speed,
		   &mode, &bits_per_word, &num) != TEST_PARAM_NUM) {
		dev_err(dev, "error format\n");
		return count;
	}

	kstr = kstrdup(buf, GFP_KERNEL);
	tx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	rx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(kstr) ||
			IS_ERR(tx_buf) ||
			IS_ERR(rx_buf)) {
		dev_err(dev, "failed to alloc tx rx buffer\n");
		goto test_end;
	}

	str_temp = kstr;
	/* skip pass over "cs_gpio speed mode bits_per_word num" */
	for (i = 0; i < TEST_PARAM_NUM; i++)
		strsep(&str_temp, ", ");
	for (i = 0; i < num; i++) {
		token = strsep(&str_temp, ", ");
		if ((token == 0) || kstrtoul(token, 16, &value))
			break;
		tx_buf[i] = (u8)(value & 0xff);
	}
	for (; i < num; i++)
		tx_buf[i] = spicc->test_data++;

	spi_message_init(&m);
	m.spi = spi_alloc_device(spicc->master);
	m.spi->cs_gpio = (cs_gpio > 0) ? cs_gpio : -ENOENT;
	m.spi->max_speed_hz = speed;
	m.spi->mode = mode & 0xffff;
	m.spi->bits_per_word = bits_per_word;
	m.spi->controller_data = &cd;
	if (spi_setup(m.spi))
		goto test_end;

	memset(&t, 0, sizeof(t));
	t.tx_buf = (void *)tx_buf;
	t.rx_buf = (void *)rx_buf;
	t.len = num;
	spi_message_add_tail(&t, &m);
	ret = spi_sync(m.spi, &m);
	spi_dev_put(m.spi);
	if (!ret && (mode & (SPI_LOOP | (1<<16)))) {
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

test_end:
	dev_info(dev, "test end @%d\n", (u32)clk_get_rate(spicc->clk));
	kfree(kstr);
	kfree(tx_buf);
	kfree(rx_buf);
	return count;
}

static struct class_attribute spicc_class_attrs[] = {
	__ATTR(test, 0200, NULL, store_test),
	__ATTR(controller_data, 0644, show_setting, store_setting),
	__ATTR_NULL
};
#endif /* end MESON_SPICC_TEST_ENTRY */

/*
 * There are three function blocks driven by core/comp clk in spicc,
 * x-------------------------------------------------------x
 * |             | apb bus | delay control |  rate divider |
 * x-------------------------------------------------------x
 * |gxl/txl/txlx |  core   |     core      |    core       |
 * x-------------------------------------------------------x
 * |axg          |  core   |     comp      |    core       |
 * x-------------------------------------------------------x
 * |txhd/g12     |  core   |     comp      |    comp       |
 * x-------------------------------------------------------x
 *
 * The Clock Mux
 *            x-----------------x   x------------x    x------\
 *        |---| 0) fixed factor |---| 1) old div |----|      |
 *        |   x-----------------x   x------------x    |      |
 * src ---|                                           |5) mux|-- out
 *        |   x-----------------x   x------------x    |      |
 *        |---| 2) fixed factor |---| 3) new div |0---|      |
 *            x-----------------x   x------------x    x------/
 *
 * Clk path for GX series:
 *    src -> 0 -> 1 -> out
 *
 * Clk path for AXG series:
 *    src -> 0 -> 1 -> 5 -> out
 *    src -> 2 -> 3 -> 5 -> out
 */

/*
 * Register a group of clk, one is a clk-fixed-factor and the other
 * is a clk-divider whose parent is the clk-fixed-factor.
 */
static struct clk *meson_spicc_clk_register_ff_divider(
	      struct meson_spicc_device *spicc, bool is_enhance)
{
	struct device *dev = &spicc->pdev->dev;
	struct clk_init_data init;
	struct clk *clk;
	struct clk_fixed_factor *ff;
	struct clk_divider *div;
	const char *parent_names[1];
	char name[32];
	char *which;

	ff = devm_kzalloc(dev, sizeof(*ff), GFP_KERNEL);
	div = devm_kzalloc(dev, sizeof(*div), GFP_KERNEL);
	if (!ff || !div)
		return ERR_PTR(-ENOMEM);

	if (is_enhance) {
		which = "enh";
		ff->mult = SPICC_ENH_FIX_FACTOR_MULT;
		ff->div = SPICC_ENH_FIX_FACTOR_DIV;
		div->reg = spicc->base + SPICC_ENH_CTL0;
		div->shift = ffs(SPICC_ENH_DATARATE_MASK) - 1;
		div->width = mask_width(SPICC_ENH_DATARATE_MASK);
		div->flags = CLK_DIVIDER_ROUND_CLOSEST;
#ifdef CONFIG_AMLOGIC_MODIFY
		div->flags |= CLK_DIVIDER_PROHIBIT_ZERO;
#endif
	} else {
		which = "old";
		ff->mult = SPICC_FIX_FACTOR_MULT;
		ff->div = SPICC_FIX_FACTOR_DIV;
		div->reg = spicc->base + SPICC_CONREG;
		div->shift = ffs(SPICC_DATARATE_MASK) - 1;
		div->width = mask_width(SPICC_DATARATE_MASK);
		div->flags = CLK_DIVIDER_POWER_OF_TWO;
	}

	/* Get parent clk of the group */
	clk = spicc->data->is_div_parent_comp_clk ? spicc->comp : spicc->core;

	/* Register clk-fixed-factor */
	parent_names[0] = __clk_get_name(clk);
	snprintf(name, sizeof(name), "%s#_%sff", dev_name(dev), which);
	init.name = name;
	init.ops = &clk_fixed_factor_ops;
	init.flags = CLK_SET_RATE_PARENT;
	init.parent_names = parent_names;
	init.num_parents = 1;
	ff->hw.init = &init;
	clk = devm_clk_register(dev, &ff->hw);
	if (IS_ERR(clk))
		return clk;

	/* Register clk-divider, which parent the clk-fixed-factor */
	parent_names[0] = __clk_get_name(clk);
	snprintf(name, sizeof(name), "%s#_%sdiv", dev_name(dev), which);
	init.name = name;
	init.ops = &clk_divider_ops;
	init.flags = CLK_SET_RATE_PARENT;
	init.parent_names = parent_names;
	init.num_parents = 1;
	div->hw.init = &init;
	clk = devm_clk_register(dev, &div->hw);

	return clk;
}

static struct clk *meson_spicc_clk_register_mux(
		   struct meson_spicc_device *spicc,
		   const char * const *parent_names)
{
	struct device *dev = &spicc->pdev->dev;
	struct clk_init_data init;
	struct clk_mux *mux;
	struct clk *clk;
	char name[32];

	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	snprintf(name, sizeof(name), "%s#_sel", dev_name(dev));
	init.name = name;
	init.ops = &clk_mux_ops;
	init.parent_names = parent_names;
	init.num_parents = 2;
	init.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT;

	mux->mask = mask_width(SPICC_ENH_DATARATE_EN);
	mux->shift = ffs(SPICC_ENH_DATARATE_EN) - 1;
	mux->reg = spicc->base + SPICC_ENH_CTL0;
	mux->hw.init = &init;
	clk = devm_clk_register(dev, &mux->hw);

	return clk;
}

static int meson_spicc_clk_init(struct meson_spicc_device *spicc)
{
	struct device *dev = &spicc->pdev->dev;
	struct clk *clk;
	const char *parent_names[2];

	/* Get core clk */
	clk = devm_clk_get(dev, "core");
	if (WARN_ON(IS_ERR(clk)))
		return PTR_ERR(clk);
	clk_prepare_enable(clk);
	spicc->core = clk;

	/* Get composite clk */
	if (spicc->data->has_comp_clk) {
		clk = devm_clk_get(dev, "comp");
		if (WARN_ON(IS_ERR(clk)))
			return PTR_ERR(clk);
		clk_prepare_enable(clk);
		spicc->comp = clk;
	}

	/* Register old divider */
	clk = meson_spicc_clk_register_ff_divider(spicc, 0);
	if (IS_ERR(clk)) {
		dev_err(dev, "Register old divider failed\n");
		return PTR_ERR(clk);
	}

	/* use old divider as spi clk if there isn't enhance divider */
	if (spicc->data->has_enhance_clk_div == false) {
		spicc->clk = clk;
		clk_prepare_enable(spicc->clk);
		return 0;
	}

	/* Set old divider as the first parent of mux */
	parent_names[0] = __clk_get_name(clk);

	/* Register enhance divider */
	clk = meson_spicc_clk_register_ff_divider(spicc, 1);
	if (IS_ERR(clk)) {
		dev_err(dev, "Register enh divider failed\n");
		return PTR_ERR(clk);
	}

	/* Set enhance divider as the second parent of mux */
	parent_names[1] = __clk_get_name(clk);

	/* Register mux and use it as spi clk */
	spicc->clk = meson_spicc_clk_register_mux(spicc, parent_names);
	if (IS_ERR(spicc->clk)) {
		dev_err(dev, "Register mux failed\n");
		return PTR_ERR(spicc->clk);
	}

	/* Fix ehance divider as the parent of mux */
	clk_set_parent(spicc->clk, clk);
	clk_prepare_enable(spicc->clk);
	return 0;
}

static int meson_spicc_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct meson_spicc_device *spicc;
	struct resource *res;
	int ret, irq;

	master = spi_alloc_master(&pdev->dev, sizeof(*spicc));
	if (!master) {
		dev_err(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}
	spicc = spi_master_get_devdata(master);
	spicc->master = master;

	spicc->pdev = pdev;
	platform_set_drvdata(pdev, spicc);

	spicc->data = (const struct meson_spicc_data *)
		of_device_get_match_data(&pdev->dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	spicc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spicc->base)) {
		dev_err(&pdev->dev, "io resource mapping failed\n");
		ret = PTR_ERR(spicc->base);
		goto out_master;
	}

	/* Set master mode and enable controller */
	writel_relaxed(SPICC_ENABLE | SPICC_MODE_MASTER,
			spicc->base + SPICC_CONREG);

	/* Disable all IRQs */
	writel_relaxed(0, spicc->base + SPICC_INTREG);

	irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, irq, meson_spicc_irq,
			       0, dev_name(&pdev->dev), spicc);
	if (ret) {
		dev_err(&pdev->dev, "irq request failed\n");
		goto out_master;
	}

	ret = meson_spicc_clk_init(spicc);
	if (ret) {
		dev_err(&pdev->dev, "clock registration failed\n");
		goto out_master;
	}

	device_reset_optional(&pdev->dev);

	master->num_chipselect = 4;
	master->dev.of_node = pdev->dev.of_node;
	master->mode_bits = SPI_CPHA | SPI_CPOL | SPI_CS_HIGH | SPI_LOOP;
	master->flags = (SPI_MASTER_MUST_RX | SPI_MASTER_MUST_TX);
	/* Setup max/min rate according to the Meson datasheet */
	master->min_speed_hz = spicc->data->min_speed_hz;
	master->max_speed_hz = spicc->data->max_speed_hz;
	master->setup = meson_spicc_setup;
	master->cleanup = meson_spicc_cleanup;
	master->prepare_message = meson_spicc_prepare_message;
	master->unprepare_transfer_hardware = meson_spicc_unprepare_transfer;
	master->transfer_one = meson_spicc_transfer_one;

	ret = devm_spi_register_master(&pdev->dev, master);
	if (!ret) {
#ifdef MESON_SPICC_TEST_ENTRY
		spicc->cls.name = dev_name(&pdev->dev);
		spicc->cls.class_attrs = spicc_class_attrs;
		ret = class_register(&spicc->cls);
#endif /* end MESON_SPICC_TEST_ENTRY */
		return 0;
	}
	dev_err(&pdev->dev, "spi master registration failed\n");

out_master:
	spi_master_put(master);

	return ret;
}

static int meson_spicc_remove(struct platform_device *pdev)
{
	struct meson_spicc_device *spicc = platform_get_drvdata(pdev);

	/* Disable SPI */
	writel(0, spicc->base + SPICC_CONREG);

	clk_disable_unprepare(spicc->core);

	return 0;
}

static const struct meson_spicc_data meson_spicc_gx_data = {
	.min_speed_hz		= 325000,
	.max_speed_hz		= 4166667,
	.dma_burst_triggered_by_ssctl = true,
	.fifo_size		= 16,
};

static const struct meson_spicc_data meson_spicc_txlx_data = {
	.min_speed_hz		= 325000,
	.max_speed_hz		= 83333333,
	.dma_burst_triggered_by_ssctl = true,
	.fifo_size		= 16,
	.has_oen		= true,
	.has_enhance_clk_div	= true,
	.has_cs_pre_delay	= true,
};

static const struct meson_spicc_data meson_spicc_axg_data = {
	.min_speed_hz		= 325000,
	.max_speed_hz		= 83333333,
	.fifo_size		= 16,
	.has_oen		= true,
	.has_enhance_clk_div	= true,
	.has_cs_pre_delay	= true,
	.has_enhance_io_delay	= true,
	.has_comp_clk		= true,
};

static const struct meson_spicc_data meson_spicc_g12a_data = {
	.min_speed_hz		= 50000,
	.max_speed_hz		= 166666667,
	.fifo_size		= 15,
	.has_oen		= true,
	.has_enhance_clk_div	= true,
	.has_cs_pre_delay	= true,
	.has_enhance_io_delay	= true,
	.has_comp_clk		= true,
	.is_div_parent_comp_clk	= true,
	.has_enhance_tt_ti_delay = true,
};

static const struct of_device_id meson_spicc_of_match[] = {
	{
		.compatible	= "amlogic,meson-gx-spicc",
		.data		= &meson_spicc_gx_data,
	},
	{
		.compatible	= "amlogic,meson-txlx-spicc",
		.data		= &meson_spicc_txlx_data,
	},
	{
		.compatible = "amlogic,meson-axg-spicc",
		.data		= &meson_spicc_axg_data,
	},
	{
		.compatible = "amlogic,meson-g12a-spicc",
		.data		= &meson_spicc_g12a_data,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, meson_spicc_of_match);

static struct platform_driver meson_spicc_driver = {
	.probe   = meson_spicc_probe,
	.remove  = meson_spicc_remove,
	.driver  = {
		.name = "meson-spicc",
		.of_match_table = of_match_ptr(meson_spicc_of_match),
	},
};

module_platform_driver(meson_spicc_driver);

MODULE_DESCRIPTION("Meson SPI Communication Controller driver");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_LICENSE("GPL");
