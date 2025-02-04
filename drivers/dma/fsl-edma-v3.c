/*
 * drivers/dma/fsl-edma3-v3.c
 *
 * Copyright 2017-2018 NXP .
 *
 * Driver for the Freescale eDMA engine v3. This driver based on fsl-edma3.c
 * but changed to meet the IP change on i.MX8QM: every dma channel is specific
 * to hardware. For example, channel 14 for LPUART1 receive request and channel
 * 13 for transmit requesst. The eDMA block can be found on i.MX8QM
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_dma.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>

#include "virt-dma.h"

#define EDMA_MP_CH_MUX			0x200

#define EDMA_CH_CSR			0x00
#define EDMA_CH_ES			0x04
#define EDMA_CH_INT			0x08
#define EDMA_CH_SBR			0x0C
#define EDMA_CH_PRI			0x10
#define EDMA_CH_MUX			0x14
#define EDMA_TCD_SADDR			0x20
#define EDMA_TCD_SOFF			0x24
#define EDMA_TCD_ATTR			0x26
#define EDMA_TCD_NBYTES			0x28
#define EDMA_TCD_SLAST			0x2C
#define EDMA_TCD_DADDR			0x30
#define EDMA_TCD_DOFF			0x34
#define EDMA_TCD_CITER_ELINK		0x36
#define EDMA_TCD_CITER			0x36
#define EDMA_TCD_DLAST_SGA		0x38
#define EDMA_TCD_CSR			0x3C
#define EDMA_TCD_BITER_ELINK		0x3E
#define EDMA_TCD_BITER			0x3E

#define EDMA_V5_TCD_SADDR_L		0x20
#define EDMA_V5_TCD_SADDR_H		0x24
#define EDMA_V5_TCD_SOFF		0x28
#define EDMA_V5_TCD_ATTR		0x2A
#define EDMA_V5_TCD_NBYTES		0x2C
#define EDMA_V5_TCD_SLAST_L		0x30
#define EDMA_V5_TCD_SLAST_H		0x34
#define EDMA_V5_TCD_DADDR_L		0x38
#define EDMA_V5_TCD_DADDR_H		0x3C
#define EDMA_V5_TCD_DLAST_SGA_L		0x40
#define EDMA_V5_TCD_DLAST_SGA_H		0x44
#define EDMA_V5_TCD_DOFF		0x48
#define EDMA_V5_TCD_CITER_ELINK		0x4A
#define EDMA_V5_TCD_CITER		0x4A
#define EDMA_V5_TCD_CSR			0x4C
#define EDMA_V5_TCD_BITER_ELINK		0x4E
#define EDMA_V5_TCD_BITER		0x4E

#define EDMA_CH_SBR_RD		BIT(22)
#define EDMA_CH_SBR_WR		BIT(21)
#define EDMA_CH_CSR_ERQ		BIT(0)
#define EDMA_CH_CSR_EARQ	BIT(1)
#define EDMA_CH_CSR_EEI		BIT(2)
#define EDMA_CH_CSR_DONE	BIT(30)
#define EDMA_CH_CSR_ACTIVE	BIT(31)

#define EDMA_TCD_ATTR_DSIZE(x)		(((x) & 0x0007))
#define EDMA_TCD_ATTR_DMOD(x)		(((x) & 0x001F) << 3)
#define EDMA_TCD_ATTR_SSIZE(x)		(((x) & 0x0007) << 8)
#define EDMA_TCD_ATTR_SMOD(x)		(((x) & 0x001F) << 11)
#define EDMA_TCD_ATTR_SSIZE_8BIT	(0x0000)
#define EDMA_TCD_ATTR_SSIZE_16BIT	(0x0100)
#define EDMA_TCD_ATTR_SSIZE_32BIT	(0x0200)
#define EDMA_TCD_ATTR_SSIZE_64BIT	(0x0300)
#define EDMA_TCD_ATTR_SSIZE_16BYTE	(0x0400)
#define EDMA_TCD_ATTR_SSIZE_32BYTE	(0x0500)
#define EDMA_TCD_ATTR_SSIZE_64BYTE	(0x0600)
#define EDMA_TCD_ATTR_DSIZE_8BIT	(0x0000)
#define EDMA_TCD_ATTR_DSIZE_16BIT	(0x0001)
#define EDMA_TCD_ATTR_DSIZE_32BIT	(0x0002)
#define EDMA_TCD_ATTR_DSIZE_64BIT	(0x0003)
#define EDMA_TCD_ATTR_DSIZE_16BYTE	(0x0004)
#define EDMA_TCD_ATTR_DSIZE_32BYTE	(0x0005)
#define EDMA_TCD_ATTR_DSIZE_64BYTE	(0x0006)

#define EDMA_TCD_SOFF_SOFF(x)		(x)
#define EDMA_TCD_NBYTES_NBYTES(x)	(x)
#define EDMA_TCD_NBYTES_MLOFF_NBYTES(x)	((x) & GENMASK(9, 0))
#define EDMA_TCD_NBYTES_MLOFF(x)	(x << 10)
#define EDMA_TCD_NBYTES_DMLOE		(1 << 30)
#define EDMA_TCD_NBYTES_SMLOE		(1 << 31)
#define EDMA_TCD_SLAST_SLAST(x)		(x)
#define EDMA_TCD_DADDR_DADDR(x)		(x)
#define EDMA_TCD_CITER_CITER(x)		((x) & 0x7FFF)
#define EDMA_TCD_DOFF_DOFF(x)		(x)
#define EDMA_TCD_DLAST_SGA_DLAST_SGA(x)	(x)
#define EDMA_TCD_BITER_BITER(x)		((x) & 0x7FFF)

#define EDMA_TCD_CSR_START		BIT(0)
#define EDMA_TCD_CSR_INT_MAJOR		BIT(1)
#define EDMA_TCD_CSR_INT_HALF		BIT(2)
#define EDMA_TCD_CSR_D_REQ		BIT(3)
#define EDMA_TCD_CSR_E_SG		BIT(4)
#define EDMA_TCD_CSR_E_LINK		BIT(5)
#define EDMA_TCD_CSR_ACTIVE		BIT(6)
#define EDMA_TCD_CSR_DONE		BIT(7)

#define EDMA_CH_ERR_DBE                 BIT(0)
#define EDMA_CH_ERR_SBE                 BIT(1)
#define EDMA_CH_ERR_SGE                 BIT(2)
#define EDMA_CH_ERR_NCE                 BIT(3)
#define EDMA_CH_ERR_DOE                 BIT(4)
#define EDMA_CH_ERR_DAE                 BIT(5)
#define EDMA_CH_ERR_SOE                 BIT(6)
#define EDMA_CH_ERR_SAE                 BIT(7)
#define EDMA_CH_ERR_ECX                 BIT(8)
#define EDMA_CH_ERR_UCE                 BIT(9)
#define EDMA_CH_ERR                     BIT(31)

#define FSL_EDMA_BUSWIDTHS	(BIT(DMA_SLAVE_BUSWIDTH_1_BYTE) | \
				BIT(DMA_SLAVE_BUSWIDTH_2_BYTES) | \
				BIT(DMA_SLAVE_BUSWIDTH_4_BYTES) | \
				BIT(DMA_SLAVE_BUSWIDTH_8_BYTES) | \
				BIT(DMA_SLAVE_BUSWIDTH_16_BYTES))

#define EDMA_ADDR_WIDTH                 0x4

#define ARGS_RX				BIT(0)
#define ARGS_REMOTE			BIT(1)
#define ARGS_MULTI_FIFO			BIT(2)

/* channel name template define in dts */
#define CHAN_PREFIX			"edma0-chan"
#define CHAN_POSFIX			"-tx"
#define CLK_POSFIX			"-clk"

#define EDMA_MINOR_LOOP_TIMEOUT                500 /* us */

struct fsl_edma3_hw_tcd {
	__le32	saddr;
	__le16	soff;
	__le16	attr;
	__le32	nbytes;
	__le32	slast;
	__le32	daddr;
	__le16	doff;
	__le16	citer;
	__le32	dlast_sga;
	__le16	csr;
	__le16	biter;
};

struct fsl_edma3_hw_tcd_v5 {
	__le32	saddr_l;
	__le32	saddr_h;
	__le16	soff;
	__le16	attr;
	__le32	nbytes;
	__le32	slast_l;
	__le32	slast_h;
	__le32	daddr_l;
	__le32	daddr_h;
	__le32	dlast_sga_l;
	__le32	dlast_sga_h;
	__le16	doff;
	__le16	citer;
	__le16	csr;
	__le16	biter;
};

struct fsl_edma3_sw_tcd {
	dma_addr_t			ptcd;
	void		*vtcd;
};

struct fsl_edma3_slave_config {
	enum dma_transfer_direction	dir;
	enum dma_slave_buswidth		addr_width;
	u32				dev_addr;
	u32				dev2_addr; /* source addr for dev2dev */
	u32				burst;
	u32				attr;
};

struct fsl_edma3_chan {
	struct virt_dma_chan		vchan;
	enum dma_status			status;
	bool				idle;
	struct fsl_edma3_engine		*edma3;
	struct fsl_edma3_desc		*edesc;
	struct fsl_edma3_slave_config	fsc;
	void __iomem			*membase;
	void __iomem                    *mux_addr;
	int				txirq;
	int				errirq;
	int				hw_chanid;
	int				priority;
	int				is_rxchan;
	int				is_remote;
	int				is_multi_fifo;
	bool                            is_sw;
	struct dma_pool			*tcd_pool;
	u32				chn_real_count;
	char                            txirq_name[32];
	char                            errirq_name[32];
	struct platform_device		*pdev;
	struct device			*dev;
	u32				srcid;
	struct clk			*clk;
};

struct fsl_edma3_drvdata {
	bool has_pd;
	u32 dmamuxs;
	bool has_chclk;
	bool has_chmux;
	bool mp_chmux;
	bool edma_v5;
	bool mem_remote;
	bool errirq_share;
};

struct fsl_edma3_desc {
	struct virt_dma_desc		vdesc;
	struct fsl_edma3_chan		*echan;
	bool				iscyclic;
	unsigned int			n_tcds;
	struct fsl_edma3_sw_tcd		tcd[];
};

struct fsl_edma3_reg_save {
	u32 csr;
	u32 sbr;
};

struct fsl_edma3_engine {
	struct dma_device	dma_dev;
	unsigned long		irqflag;
	struct mutex		fsl_edma3_mutex;
	u32			n_chans;
	int			errirq;
	#define MAX_CHAN_NUM	64
	struct fsl_edma3_reg_save edma_regs[MAX_CHAN_NUM];
	bool                    bus_axi;
	const struct fsl_edma3_drvdata *drvdata;
	struct clk		*clk_mp;
	struct clk              *dmaclk;
	struct fsl_edma3_chan	chans[];
};


static struct fsl_edma3_drvdata fsl_edma_imx8q = {
	.has_pd = true,
	.dmamuxs = 0,
	.has_chclk = false,
	.has_chmux = true,
	.mp_chmux = false,
	.mem_remote = true,
	.edma_v5 = false,
	.errirq_share = true,
};

static struct fsl_edma3_drvdata fsl_edma_imx8ulp = {
	.has_pd = false,
	.dmamuxs = 1,
	.has_chclk = true,
	.has_chmux = true,
	.mp_chmux = false,
	.mem_remote = false,
	.edma_v5 = false,
	.errirq_share = false,
};

static struct fsl_edma3_drvdata fsl_edma_imx93 = {
	.has_pd = false,
	.dmamuxs = 0,
	.has_chclk = false,
	.has_chmux = false,
	.mp_chmux = false,
	.mem_remote = false,
	.edma_v5 = false,
	.errirq_share = true,
};

static struct fsl_edma3_drvdata fsl_edma_imx95 = {
	.has_pd = false,
	.dmamuxs = 0,
	.has_chclk = false,
	.has_chmux = false,
	.mp_chmux = true,
	.mem_remote = false,
	.edma_v5 = true,
	.errirq_share = true,
};

static struct fsl_edma3_chan *to_fsl_edma3_chan(struct dma_chan *chan)
{
	return container_of(chan, struct fsl_edma3_chan, vchan.chan);
}

static struct fsl_edma3_desc *to_fsl_edma3_desc(struct virt_dma_desc *vd)
{
	return container_of(vd, struct fsl_edma3_desc, vdesc);
}

static bool is_srcid_in_use(struct fsl_edma3_engine *fsl_edma3, u32 srcid)
{
	struct fsl_edma3_chan *fsl_chan;
	int i;

	for (i = 0; i < fsl_edma3->n_chans; i++) {
		fsl_chan = &fsl_edma3->chans[i];

		if (fsl_chan->srcid && srcid == fsl_chan->srcid) {
			dev_err(&fsl_chan->pdev->dev, "The srcid is using! Can't use repeatly.");
			return true;
		}
	}
	return false;
}

static void fsl_edma3_enable_request(struct fsl_edma3_chan *fsl_chan)
{
	void __iomem *addr = fsl_chan->membase;
	u32 val;

	val = readl(addr + EDMA_CH_SBR);
	if (fsl_chan->is_rxchan)
		val |= EDMA_CH_SBR_RD;
	else
		val |= EDMA_CH_SBR_WR;

	if (fsl_chan->is_remote)
		val &= ~(EDMA_CH_SBR_RD | EDMA_CH_SBR_WR);

	writel(val, addr + EDMA_CH_SBR);

	if ((fsl_chan->edma3->drvdata->has_chmux || fsl_chan->edma3->bus_axi) &&
	    fsl_chan->srcid) {
		if (!readl(fsl_chan->mux_addr))
			writel(fsl_chan->srcid, fsl_chan->mux_addr);
	}
	val = readl(addr + EDMA_CH_CSR);

	val |= EDMA_CH_CSR_ERQ | EDMA_CH_CSR_EEI;
	writel(val, addr + EDMA_CH_CSR);
}

static void fsl_edma3_disable_request(struct fsl_edma3_chan *fsl_chan)
{
	void __iomem *addr = fsl_chan->membase;
	u32 val = readl(addr + EDMA_CH_CSR);

	if ((fsl_chan->edma3->drvdata->has_chmux || fsl_chan->edma3->bus_axi) &&
	    fsl_chan->srcid) {
		writel(0, fsl_chan->mux_addr);
	}
	val &= ~EDMA_CH_CSR_ERQ;
	writel(val, addr + EDMA_CH_CSR);
}

static unsigned int fsl_edma3_get_tcd_attr(enum dma_slave_buswidth addr_width)
{
	switch (addr_width) {
	case 1:
		return EDMA_TCD_ATTR_SSIZE_8BIT | EDMA_TCD_ATTR_DSIZE_8BIT;
	case 2:
		return EDMA_TCD_ATTR_SSIZE_16BIT | EDMA_TCD_ATTR_DSIZE_16BIT;
	case 4:
		return EDMA_TCD_ATTR_SSIZE_32BIT | EDMA_TCD_ATTR_DSIZE_32BIT;
	case 8:
		return EDMA_TCD_ATTR_SSIZE_64BIT | EDMA_TCD_ATTR_DSIZE_64BIT;
	case 16:
		return EDMA_TCD_ATTR_SSIZE_16BYTE | EDMA_TCD_ATTR_DSIZE_16BYTE;
	case 32:
		return EDMA_TCD_ATTR_SSIZE_32BYTE | EDMA_TCD_ATTR_DSIZE_32BYTE;
	case 64:
		return EDMA_TCD_ATTR_SSIZE_64BYTE | EDMA_TCD_ATTR_DSIZE_64BYTE;
	default:
		return EDMA_TCD_ATTR_SSIZE_32BIT | EDMA_TCD_ATTR_DSIZE_32BIT;
	}
}

static void fsl_edma3_free_desc(struct virt_dma_desc *vdesc)
{
	struct fsl_edma3_desc *fsl_desc;
	int i;

	fsl_desc = to_fsl_edma3_desc(vdesc);
	for (i = 0; i < fsl_desc->n_tcds; i++)
		dma_pool_free(fsl_desc->echan->tcd_pool, fsl_desc->tcd[i].vtcd,
			      fsl_desc->tcd[i].ptcd);
	kfree(fsl_desc);
}

static int fsl_edma3_terminate_all(struct dma_chan *chan)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	unsigned long flags;
	LIST_HEAD(head);
	u32 val;

	fsl_edma3_disable_request(fsl_chan);

	/*
	 * Checking ACTIVE to ensure minor loop stop indeed to prevent the
	 * potential illegal memory write if channel not stopped with buffer
	 * freed. Ignore tx channel since no such risk.
	 */
	if (fsl_chan->is_rxchan)
		readl_poll_timeout_atomic(fsl_chan->membase + EDMA_CH_CSR, val,
			!(val & EDMA_CH_CSR_ACTIVE), 2,
			EDMA_MINOR_LOOP_TIMEOUT);

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	fsl_chan->edesc = NULL;
	fsl_chan->idle = true;
	fsl_chan->vchan.cyclic = NULL;
	vchan_get_all_descriptors(&fsl_chan->vchan, &head);
	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);
	vchan_dma_desc_free_list(&fsl_chan->vchan, &head);
	return 0;
}

static int fsl_edma3_pause(struct dma_chan *chan)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	if (fsl_chan->edesc) {
		fsl_edma3_disable_request(fsl_chan);
		fsl_chan->status = DMA_PAUSED;
		fsl_chan->idle = true;
	}
	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);

	return 0;
}

static int fsl_edma3_resume(struct dma_chan *chan)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	if (fsl_chan->edesc) {
		fsl_edma3_enable_request(fsl_chan);
		fsl_chan->status = DMA_IN_PROGRESS;
		fsl_chan->idle = false;
	}
	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);

	return 0;
}

static int fsl_edma3_slave_config(struct dma_chan *chan,
				 struct dma_slave_config *cfg)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);

	fsl_chan->fsc.dir = cfg->direction;
	if (cfg->direction == DMA_DEV_TO_MEM) {
		fsl_chan->fsc.dev_addr = cfg->src_addr;
		fsl_chan->fsc.addr_width = cfg->src_addr_width;
		fsl_chan->fsc.burst = cfg->src_maxburst;
		fsl_chan->fsc.attr = fsl_edma3_get_tcd_attr
					(cfg->src_addr_width);
	} else if (cfg->direction == DMA_MEM_TO_DEV) {
		fsl_chan->fsc.dev_addr = cfg->dst_addr;
		fsl_chan->fsc.addr_width = cfg->dst_addr_width;
		fsl_chan->fsc.burst = cfg->dst_maxburst;
		fsl_chan->fsc.attr = fsl_edma3_get_tcd_attr
					(cfg->dst_addr_width);
	} else if (cfg->direction == DMA_DEV_TO_DEV) {
		fsl_chan->fsc.dev2_addr = cfg->src_addr;
		fsl_chan->fsc.dev_addr = cfg->dst_addr;
		fsl_chan->fsc.addr_width = cfg->dst_addr_width;
		fsl_chan->fsc.burst = cfg->dst_maxburst;
		fsl_chan->fsc.attr = fsl_edma3_get_tcd_attr
					(cfg->dst_addr_width);
	} else {
		return -EINVAL;
	}
	return 0;
}

static size_t fsl_edma3_desc_residue(struct fsl_edma3_chan *fsl_chan,
		struct virt_dma_desc *vdesc, bool in_progress)
{
	struct fsl_edma3_desc *edesc = fsl_chan->edesc;
	void __iomem *addr = fsl_chan->membase;
	enum dma_transfer_direction dir = fsl_chan->fsc.dir;
	dma_addr_t cur_addr, dma_addr;
	size_t len, size;
	u32 nbytes = 0;
	int i;

	if (fsl_chan->edma3->drvdata->edma_v5) {
		struct fsl_edma3_hw_tcd_v5         *vtcd;
		u32 cur_addr_l, cur_addr_h;
		u32 dma_addr_l, dma_addr_h;

		/* calculate the total size in this desc */
		for (len = i = 0; i < fsl_chan->edesc->n_tcds; i++) {
			vtcd = edesc->tcd[i].vtcd;
			if ((vtcd->nbytes & EDMA_TCD_NBYTES_DMLOE) ||
			    (vtcd->nbytes & EDMA_TCD_NBYTES_SMLOE))
				nbytes = EDMA_TCD_NBYTES_MLOFF_NBYTES(vtcd->nbytes);
			else
				nbytes = le32_to_cpu(vtcd->nbytes);
			len += nbytes * le16_to_cpu(vtcd->biter);
		}

		if (!in_progress)
			return len;

		if (dir == DMA_MEM_TO_DEV) {
			do {
				cur_addr_h = readl(addr + EDMA_V5_TCD_SADDR_H);
				cur_addr_l = readl(addr + EDMA_V5_TCD_SADDR_L);
			} while (cur_addr_h != readl(addr + EDMA_V5_TCD_SADDR_H));
			cur_addr = cur_addr_h;
			cur_addr = (cur_addr << 32) | cur_addr_l;
		} else {
			do {
				cur_addr_h = readl(addr + EDMA_V5_TCD_DADDR_H);
				cur_addr_l = readl(addr + EDMA_V5_TCD_DADDR_L);
			} while (cur_addr_h != readl(addr + EDMA_V5_TCD_DADDR_H));
			cur_addr = cur_addr_h;
			cur_addr = (cur_addr << 32) | cur_addr_l;
		}
		/* figure out the finished and calculate the residue */
		for (i = 0; i < fsl_chan->edesc->n_tcds; i++) {
			vtcd = edesc->tcd[i].vtcd;
			if ((vtcd->nbytes & EDMA_TCD_NBYTES_DMLOE) ||
			    (vtcd->nbytes & EDMA_TCD_NBYTES_SMLOE))
				nbytes = EDMA_TCD_NBYTES_MLOFF_NBYTES(vtcd->nbytes);
			else
				nbytes = le32_to_cpu(vtcd->nbytes);

			size = nbytes * le16_to_cpu(vtcd->biter);

			if (dir == DMA_MEM_TO_DEV) {
				dma_addr_l = le32_to_cpu(vtcd->saddr_l);
				dma_addr_h = le32_to_cpu(vtcd->saddr_h);
				dma_addr = dma_addr_h;
				dma_addr = (dma_addr << 32) | dma_addr_l;
			} else {
				dma_addr_l = le32_to_cpu(vtcd->daddr_l);
				dma_addr_h = le32_to_cpu(vtcd->daddr_h);
				dma_addr = dma_addr_h;
				dma_addr = (dma_addr << 32) | dma_addr_l;
			}
			len -= size;
			if (cur_addr >= dma_addr && cur_addr < dma_addr + size) {
				len += dma_addr + size - cur_addr;
				break;
			}
		}
	} else {

		struct fsl_edma3_hw_tcd         *vtcd;
		/* calculate the total size in this desc */
		for (len = i = 0; i < fsl_chan->edesc->n_tcds; i++) {
			vtcd = edesc->tcd[i].vtcd;
			if ((vtcd->nbytes & EDMA_TCD_NBYTES_DMLOE) ||
			    (vtcd->nbytes & EDMA_TCD_NBYTES_SMLOE))
				nbytes = EDMA_TCD_NBYTES_MLOFF_NBYTES(vtcd->nbytes);
			else
				nbytes = le32_to_cpu(vtcd->nbytes);
			len += nbytes * le16_to_cpu(vtcd->biter);
		}

		if (!in_progress)
			return len;

		if (dir == DMA_MEM_TO_DEV)
			cur_addr = readl(addr + EDMA_TCD_SADDR);
		else
			cur_addr = readl(addr + EDMA_TCD_DADDR);

		/* figure out the finished and calculate the residue */
		for (i = 0; i < fsl_chan->edesc->n_tcds; i++) {
			vtcd = edesc->tcd[i].vtcd;
			if ((vtcd->nbytes & EDMA_TCD_NBYTES_DMLOE) ||
			    (vtcd->nbytes & EDMA_TCD_NBYTES_SMLOE))
				nbytes = EDMA_TCD_NBYTES_MLOFF_NBYTES(vtcd->nbytes);
			else
				nbytes = le32_to_cpu(vtcd->nbytes);

			size = nbytes * le16_to_cpu(vtcd->biter);

			if (dir == DMA_MEM_TO_DEV)
				dma_addr = le32_to_cpu(vtcd->saddr);
			else
				dma_addr = le32_to_cpu(vtcd->daddr);

			len -= size;
			if (cur_addr >= dma_addr && cur_addr < dma_addr + size) {
				len += dma_addr + size - cur_addr;
				break;
			}
		}
	}

	return len;
}

static enum dma_status fsl_edma3_tx_status(struct dma_chan *chan,
		dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	struct virt_dma_desc *vdesc;
	enum dma_status status;
	unsigned long flags;

	status = dma_cookie_status(chan, cookie, txstate);
	if (status == DMA_COMPLETE) {
		spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
		txstate->residue = fsl_chan->chn_real_count;
		spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);
		return status;
	}

	if (!txstate)
		return fsl_chan->status;

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	vdesc = vchan_find_desc(&fsl_chan->vchan, cookie);
	if (fsl_chan->edesc && cookie == fsl_chan->edesc->vdesc.tx.cookie)
		txstate->residue = fsl_edma3_desc_residue(fsl_chan, vdesc,
								true);
	else if (fsl_chan->edesc && vdesc)
		txstate->residue = fsl_edma3_desc_residue(fsl_chan, vdesc,
								false);
	else
		txstate->residue = 0;

	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);

	return fsl_chan->status;
}

static void fsl_edma3_set_tcd_regs(struct fsl_edma3_chan *fsl_chan,
				  void *tcd_tmp)
{
	void __iomem *addr = fsl_chan->membase;
	/*
	 * TCD parameters are stored in struct fsl_edma3_hw_tcd in little
	 * endian format. However, we need to load the TCD registers in
	 * big- or little-endian obeying the eDMA engine model endian.
	 */

	if (fsl_chan->edma3->drvdata->edma_v5) {
		struct fsl_edma3_hw_tcd_v5 *tcd = tcd_tmp;

		writew(0, addr + EDMA_V5_TCD_CSR);
		writel(le32_to_cpu(tcd->saddr_l), addr + EDMA_V5_TCD_SADDR_L);
		writel(le32_to_cpu(tcd->saddr_h), addr + EDMA_V5_TCD_SADDR_H);
		writel(le32_to_cpu(tcd->daddr_l), addr + EDMA_V5_TCD_DADDR_L);
		writel(le32_to_cpu(tcd->daddr_h), addr + EDMA_V5_TCD_DADDR_H);

		writew(le16_to_cpu(tcd->attr), addr + EDMA_V5_TCD_ATTR);
		writew(le16_to_cpu(tcd->soff), addr + EDMA_V5_TCD_SOFF);

		writel(le32_to_cpu(tcd->nbytes), addr + EDMA_V5_TCD_NBYTES);
		writel(le32_to_cpu(tcd->slast_l), addr + EDMA_V5_TCD_SLAST_L);
		writel(le32_to_cpu(tcd->slast_h), addr + EDMA_V5_TCD_SLAST_H);

		writew(le16_to_cpu(tcd->citer), addr + EDMA_V5_TCD_CITER);
		writew(le16_to_cpu(tcd->biter), addr + EDMA_V5_TCD_BITER);
		writew(le16_to_cpu(tcd->doff), addr + EDMA_V5_TCD_DOFF);

		writel(le32_to_cpu(tcd->dlast_sga_l), addr + EDMA_V5_TCD_DLAST_SGA_L);
		writel(le32_to_cpu(tcd->dlast_sga_h), addr + EDMA_V5_TCD_DLAST_SGA_H);

		/* Must clear CHa_CSR[DONE] bit before enable TCDa_CSR[ESG] */
		writel(readl(addr + EDMA_CH_CSR), addr + EDMA_CH_CSR);

		writew(le16_to_cpu(tcd->csr), addr + EDMA_V5_TCD_CSR);

	} else {
		struct fsl_edma3_hw_tcd *tcd = tcd_tmp;

		writew(0, addr + EDMA_TCD_CSR);
		writel(le32_to_cpu(tcd->saddr), addr + EDMA_TCD_SADDR);
		writel(le32_to_cpu(tcd->daddr), addr + EDMA_TCD_DADDR);

		writew(le16_to_cpu(tcd->attr), addr + EDMA_TCD_ATTR);
		writew(le16_to_cpu(tcd->soff), addr + EDMA_TCD_SOFF);

		writel(le32_to_cpu(tcd->nbytes), addr + EDMA_TCD_NBYTES);
		writel(le32_to_cpu(tcd->slast), addr + EDMA_TCD_SLAST);

		writew(le16_to_cpu(tcd->citer), addr + EDMA_TCD_CITER);
		writew(le16_to_cpu(tcd->biter), addr + EDMA_TCD_BITER);
		writew(le16_to_cpu(tcd->doff), addr + EDMA_TCD_DOFF);

		writel(le32_to_cpu(tcd->dlast_sga), addr + EDMA_TCD_DLAST_SGA);

		/* Must clear CHa_CSR[DONE] bit before enable TCDa_CSR[ESG] */
		writel(readl(addr + EDMA_CH_CSR), addr + EDMA_CH_CSR);

		writew(le16_to_cpu(tcd->csr), addr + EDMA_TCD_CSR);
	}
}

static inline
void fsl_edma3_fill_tcd(struct fsl_edma3_chan *fsl_chan, void *tcd_tmp,
			dma_addr_t src, dma_addr_t dst, u16 attr, u16 soff,
			u32 nbytes, dma_addr_t slast, u16 citer, u16 biter,
			u16 doff, dma_addr_t dlast_sga, bool major_int,
			bool disable_req, bool enable_sg)
{
	u16 csr = 0;

	if (fsl_chan->edma3->drvdata->edma_v5) {
		struct fsl_edma3_hw_tcd_v5 *tcd = tcd_tmp;
		/*
		 * eDMA hardware SGs require the TCDs to be stored in little
		 * endian format irrespective of the register endian model.
		 * So we put the value in little endian in memory, waiting
		 * for fsl_edma3_set_tcd_regs doing the swap.
		 */
		tcd->saddr_l = cpu_to_le32(src & 0xffffffff);
		tcd->saddr_h = cpu_to_le32((src >> 32) & 0xffffffff);
		tcd->daddr_l = cpu_to_le32(dst & 0xffffffff);
		tcd->daddr_h = cpu_to_le32((dst >> 32) & 0xffffffff);

		tcd->attr = cpu_to_le16(attr);

		tcd->soff = cpu_to_le16(EDMA_TCD_SOFF_SOFF(soff));

		if (fsl_chan->is_multi_fifo) {
			/* set mloff to support multiple fifo */
			nbytes |= EDMA_TCD_NBYTES_MLOFF(-(fsl_chan->fsc.burst * 4));
			/* enable DMLOE/SMLOE */
			if (fsl_chan->fsc.dir == DMA_MEM_TO_DEV) {
				nbytes |= EDMA_TCD_NBYTES_DMLOE;
				nbytes &= ~EDMA_TCD_NBYTES_SMLOE;
			} else {
				nbytes |= EDMA_TCD_NBYTES_SMLOE;
				nbytes &= ~EDMA_TCD_NBYTES_DMLOE;
			}
		}

		tcd->nbytes = cpu_to_le32(EDMA_TCD_NBYTES_NBYTES(nbytes));
		tcd->slast_l = cpu_to_le32(slast & 0xffffffff);
		tcd->slast_h = cpu_to_le32((slast >> 32) & 0xffffffff);

		tcd->citer = cpu_to_le16(EDMA_TCD_CITER_CITER(citer));
		tcd->doff = cpu_to_le16(EDMA_TCD_DOFF_DOFF(doff));

		tcd->dlast_sga_l = cpu_to_le32(dlast_sga & 0xffffffff);
		tcd->dlast_sga_h = cpu_to_le32((dlast_sga >> 32) & 0xffffffff);

		tcd->biter = cpu_to_le16(EDMA_TCD_BITER_BITER(biter));
		if (major_int)
			csr |= EDMA_TCD_CSR_INT_MAJOR;

		if (disable_req)
			csr |= EDMA_TCD_CSR_D_REQ;

		if (enable_sg)
			csr |= EDMA_TCD_CSR_E_SG;

		if (fsl_chan->is_rxchan)
			csr |= EDMA_TCD_CSR_ACTIVE;

		if (fsl_chan->is_sw)
			csr |= EDMA_TCD_CSR_START;

		tcd->csr = cpu_to_le16(csr);
	} else {
		struct fsl_edma3_hw_tcd *tcd = tcd_tmp;
		/*
		 * eDMA hardware SGs require the TCDs to be stored in little
		 * endian format irrespective of the register endian model.
		 * So we put the value in little endian in memory, waiting
		 * for fsl_edma3_set_tcd_regs doing the swap.
		 */
		tcd->saddr = cpu_to_le32(src);
		tcd->daddr = cpu_to_le32(dst);

		tcd->attr = cpu_to_le16(attr);

		tcd->soff = cpu_to_le16(EDMA_TCD_SOFF_SOFF(soff));

		if (fsl_chan->is_multi_fifo) {
			/* set mloff to support multiple fifo */
			nbytes |= EDMA_TCD_NBYTES_MLOFF(-(fsl_chan->fsc.burst * 4));
			/* enable DMLOE/SMLOE */
			if (fsl_chan->fsc.dir == DMA_MEM_TO_DEV) {
				nbytes |= EDMA_TCD_NBYTES_DMLOE;
				nbytes &= ~EDMA_TCD_NBYTES_SMLOE;
			} else {
				nbytes |= EDMA_TCD_NBYTES_SMLOE;
				nbytes &= ~EDMA_TCD_NBYTES_DMLOE;
			}
		}

		tcd->nbytes = cpu_to_le32(EDMA_TCD_NBYTES_NBYTES(nbytes));
		tcd->slast = cpu_to_le32(EDMA_TCD_SLAST_SLAST(slast));

		tcd->citer = cpu_to_le16(EDMA_TCD_CITER_CITER(citer));
		tcd->doff = cpu_to_le16(EDMA_TCD_DOFF_DOFF(doff));

		tcd->dlast_sga = cpu_to_le32(EDMA_TCD_DLAST_SGA_DLAST_SGA(dlast_sga));

		tcd->biter = cpu_to_le16(EDMA_TCD_BITER_BITER(biter));
		if (major_int)
			csr |= EDMA_TCD_CSR_INT_MAJOR;

		if (disable_req)
			csr |= EDMA_TCD_CSR_D_REQ;

		if (enable_sg)
			csr |= EDMA_TCD_CSR_E_SG;

		if (fsl_chan->is_rxchan)
			csr |= EDMA_TCD_CSR_ACTIVE;

		if (fsl_chan->is_sw)
			csr |= EDMA_TCD_CSR_START;

		tcd->csr = cpu_to_le16(csr);
	}
}

static struct fsl_edma3_desc *fsl_edma3_alloc_desc(struct fsl_edma3_chan
						*fsl_chan, int sg_len)
{
	struct fsl_edma3_desc *fsl_desc;
	int i;

	fsl_desc = kzalloc(sizeof(*fsl_desc) + sizeof(struct fsl_edma3_sw_tcd)
				* sg_len, GFP_ATOMIC);
	if (!fsl_desc)
		return NULL;

	fsl_desc->echan = fsl_chan;
	fsl_desc->n_tcds = sg_len;
	for (i = 0; i < sg_len; i++) {
		fsl_desc->tcd[i].vtcd = dma_pool_alloc(fsl_chan->tcd_pool,
					GFP_ATOMIC, &fsl_desc->tcd[i].ptcd);
		if (!fsl_desc->tcd[i].vtcd)
			goto err;
	}
	return fsl_desc;

err:
	while (--i >= 0)
		dma_pool_free(fsl_chan->tcd_pool, fsl_desc->tcd[i].vtcd,
				fsl_desc->tcd[i].ptcd);
	kfree(fsl_desc);
	return NULL;
}

static struct dma_async_tx_descriptor *fsl_edma3_prep_dma_cyclic(
		struct dma_chan *chan, dma_addr_t dma_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction,
		unsigned long flags)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	struct fsl_edma3_desc *fsl_desc;
	dma_addr_t dma_buf_next;
	int sg_len, i;
	dma_addr_t src_addr, dst_addr, last_sg;
	u32 nbytes;
	u16 soff, doff, iter;
	bool major_int = true;

	sg_len = buf_len / period_len;
	fsl_desc = fsl_edma3_alloc_desc(fsl_chan, sg_len);
	if (!fsl_desc)
		return NULL;
	fsl_desc->iscyclic = true;

	dma_buf_next = dma_addr;
	nbytes = fsl_chan->fsc.addr_width * fsl_chan->fsc.burst;

	/*
	 * Choose the suitable burst length if period_len is not multiple of
	 * burst length so that the whole transfer length is multiple of minor
	 * loop(burst length).
	 */
	if (period_len % nbytes) {
		u32 width = fsl_chan->fsc.addr_width;

		for (i = fsl_chan->fsc.burst; i > 1; i--) {
			if (!(period_len % (i * width))) {
				nbytes = i * width;
				break;
			}
		}
		/* if no chance to get suitable burst size, use it as 1 */
		if (i == 1)
			nbytes = width;
	}

	iter = period_len / nbytes;

	for (i = 0; i < sg_len; i++) {
		if (dma_buf_next >= dma_addr + buf_len)
			dma_buf_next = dma_addr;

		/* get next sg's physical address */
		last_sg = fsl_desc->tcd[(i + 1) % sg_len].ptcd;

		if (fsl_chan->fsc.dir == DMA_MEM_TO_DEV) {
			src_addr = dma_buf_next;
			dst_addr = fsl_chan->fsc.dev_addr;
			soff = fsl_chan->fsc.addr_width;
			if (fsl_chan->is_multi_fifo)
				doff = 4;
			else
				doff = 0;
		} else if (fsl_chan->fsc.dir == DMA_DEV_TO_MEM) {
			src_addr = fsl_chan->fsc.dev_addr;
			dst_addr = dma_buf_next;
			if (fsl_chan->is_multi_fifo)
				soff = 4;
			else
				soff = 0;
			doff = fsl_chan->fsc.addr_width;
		} else {
			/* DMA_DEV_TO_DEV */
			src_addr = fsl_chan->fsc.dev2_addr;
			dst_addr = fsl_chan->fsc.dev_addr;
			soff = 0;
			doff = 0;
			major_int = false;
		}

		fsl_edma3_fill_tcd(fsl_chan, fsl_desc->tcd[i].vtcd, src_addr,
				dst_addr, fsl_chan->fsc.attr, soff, nbytes, 0,
				iter, iter, doff, last_sg, major_int, false, true);
		dma_buf_next += period_len;
	}

	return vchan_tx_prep(&fsl_chan->vchan, &fsl_desc->vdesc, flags);
}

static struct dma_async_tx_descriptor *fsl_edma3_prep_slave_sg(
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction direction,
		unsigned long flags, void *context)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	struct fsl_edma3_desc *fsl_desc;
	struct scatterlist *sg;
	dma_addr_t src_addr, dst_addr, last_sg;
	u32 nbytes;
	u16 soff, doff, iter;
	int i;

	if (!is_slave_direction(fsl_chan->fsc.dir))
		return NULL;

	fsl_desc = fsl_edma3_alloc_desc(fsl_chan, sg_len);
	if (!fsl_desc)
		return NULL;
	fsl_desc->iscyclic = false;

	nbytes = fsl_chan->fsc.addr_width * fsl_chan->fsc.burst;
	for_each_sg(sgl, sg, sg_len, i) {
		/* get next sg's physical address */
		last_sg = fsl_desc->tcd[(i + 1) % sg_len].ptcd;

		if (fsl_chan->fsc.dir == DMA_MEM_TO_DEV) {
			src_addr = sg_dma_address(sg);
			dst_addr = fsl_chan->fsc.dev_addr;
			soff = fsl_chan->fsc.addr_width;
			doff = 0;
		} else if (fsl_chan->fsc.dir == DMA_DEV_TO_MEM) {
			src_addr = fsl_chan->fsc.dev_addr;
			dst_addr = sg_dma_address(sg);
			soff = 0;
			doff = fsl_chan->fsc.addr_width;
		} else {
			/* DMA_DEV_TO_DEV */
			src_addr = fsl_chan->fsc.dev2_addr;
			dst_addr = fsl_chan->fsc.dev_addr;
			soff = 0;
			doff = 0;
		}

		/*
		 * Choose the suitable burst length if sg_dma_len is not
		 * multiple of burst length so that the whole transfer length is
		 * multiple of minor loop(burst length).
		 */
		if (sg_dma_len(sg) % nbytes) {
			u32 width = fsl_chan->fsc.addr_width;
			int j;

			for (j = fsl_chan->fsc.burst; j > 1; j--) {
				if (!(sg_dma_len(sg) % (j * width))) {
					nbytes = j * width;
					break;
				}
			}
			/* Set burst size as 1 if there's no suitable one */
			if (j == 1)
				nbytes = width;
		}

		iter = sg_dma_len(sg) / nbytes;
		if (i < sg_len - 1) {
			last_sg = fsl_desc->tcd[(i + 1)].ptcd;
			fsl_edma3_fill_tcd(fsl_chan, fsl_desc->tcd[i].vtcd,
					src_addr, dst_addr, fsl_chan->fsc.attr,
					soff, nbytes, 0, iter, iter, doff,
					last_sg, false, false, true);
		} else {
			last_sg = 0;
			fsl_edma3_fill_tcd(fsl_chan, fsl_desc->tcd[i].vtcd,
					src_addr, dst_addr, fsl_chan->fsc.attr,
					soff, nbytes, 0, iter, iter, doff,
					last_sg, true, true, false);
		}
	}

	return vchan_tx_prep(&fsl_chan->vchan, &fsl_desc->vdesc, flags);
}

static void fsl_edma3_xfer_desc(struct fsl_edma3_chan *fsl_chan)
{
	struct virt_dma_desc *vdesc;

	vdesc = vchan_next_desc(&fsl_chan->vchan);
	if (!vdesc)
		return;
	fsl_chan->edesc = to_fsl_edma3_desc(vdesc);

	fsl_edma3_set_tcd_regs(fsl_chan, fsl_chan->edesc->tcd[0].vtcd);
	fsl_edma3_enable_request(fsl_chan);
	fsl_chan->status = DMA_IN_PROGRESS;
	fsl_chan->idle = false;
}

static struct dma_async_tx_descriptor *fsl_edma3_prep_memcpy(
		struct dma_chan *chan, dma_addr_t dma_dst,
		dma_addr_t dma_src, size_t len, unsigned long flags)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	struct fsl_edma3_desc *fsl_desc;

	fsl_desc = fsl_edma3_alloc_desc(fsl_chan, 1);
	if (!fsl_desc)
		return NULL;
	fsl_desc->iscyclic = false;

	fsl_chan->is_sw = true;

	if (fsl_chan->edma3->drvdata->mem_remote)
		fsl_chan->is_remote = true;

	/* To match with copy_align and max_seg_size so 1 tcd is enough */
	fsl_edma3_fill_tcd(fsl_chan, fsl_desc->tcd[0].vtcd, dma_src, dma_dst,
			EDMA_TCD_ATTR_SSIZE_64BYTE | EDMA_TCD_ATTR_DSIZE_64BYTE,
			64, len, 0, 1, 1, 64, 0, true, true, false);

	return vchan_tx_prep(&fsl_chan->vchan, &fsl_desc->vdesc, flags);
}

static size_t fsl_edma3_desc_residue(struct fsl_edma3_chan *fsl_chan,
		struct virt_dma_desc *vdesc, bool in_progress);

static void fsl_edma3_get_realcnt(struct fsl_edma3_chan *fsl_chan)
{
	fsl_chan->chn_real_count = fsl_edma3_desc_residue(fsl_chan, NULL, true);
}

static void fsl_edma3_err_check(struct fsl_edma3_chan *fsl_chan)
{
	unsigned int ch_err;
	void __iomem *base_addr;
	u32 val;

	base_addr = fsl_chan->membase;
	scoped_guard(spinlock, &fsl_chan->vchan.lock) {
		ch_err = readl(base_addr + EDMA_CH_ES);
		if (!(ch_err & EDMA_CH_ERR))
			return;

		writel(EDMA_CH_ERR, base_addr + EDMA_CH_ES);
		val = readl(base_addr + EDMA_CH_CSR);
		val &= ~EDMA_CH_CSR_ERQ;
		writel(val, base_addr + EDMA_CH_CSR);
	}

	/* Ignore this interrupt since channel has been disabled already */
	if (!fsl_chan->edesc)
		return;

	if (ch_err & EDMA_CH_ERR_DBE)
		dev_err(&fsl_chan->pdev->dev, "Destination Bus Error interrupt.\n");

	if (ch_err & EDMA_CH_ERR_SBE)
		dev_err(&fsl_chan->pdev->dev, "Source Bus Error interrupt.\n");

	if (ch_err & EDMA_CH_ERR_SGE)
		dev_err(&fsl_chan->pdev->dev, "Scatter/Gather Configuration Error interrupt.\n");

	if (ch_err & EDMA_CH_ERR_NCE)
		dev_err(&fsl_chan->pdev->dev, "NBYTES/CITER Configuration Error interrupt.\n");

	if (ch_err & EDMA_CH_ERR_DOE)
		dev_err(&fsl_chan->pdev->dev, "Destination Offset Error interrupt.\n");

	if (ch_err & EDMA_CH_ERR_DAE)
		dev_err(&fsl_chan->pdev->dev, "Destination Address Error interrupt.\n");

	if (ch_err & EDMA_CH_ERR_SOE)
		dev_err(&fsl_chan->pdev->dev, "Source Offset Error interrupt.\n");

	if (ch_err & EDMA_CH_ERR_SAE)
		dev_err(&fsl_chan->pdev->dev, "Source Address Error interrupt.\n");

	if (ch_err & EDMA_CH_ERR_ECX)
		dev_err(&fsl_chan->pdev->dev, "Transfer Canceled interrupt.\n");

	if (ch_err & EDMA_CH_ERR_UCE)
		dev_err(&fsl_chan->pdev->dev, "Uncorrectable TCD error during channel execution interrupt.\n");

	fsl_chan->status = DMA_ERROR;
}

static irqreturn_t fsl_edma3_err_handler(int irq, void *dev_id)
{
	struct fsl_edma3_chan *fsl_chan = dev_id;
	struct fsl_edma3_engine *fsl_edma3 = fsl_chan->edma3;
	unsigned int ch;

	if (!fsl_edma3->drvdata->errirq_share)
		fsl_edma3_err_check(fsl_chan);
	else {
		for (ch = 0; ch < fsl_edma3->n_chans; ch++) {
			fsl_chan = &fsl_edma3->chans[ch];
			fsl_edma3_err_check(fsl_chan);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t fsl_edma3_tx_handler(int irq, void *dev_id)
{
	struct fsl_edma3_chan *fsl_chan = dev_id;
	unsigned int intr;
	void __iomem *base_addr;

	spin_lock(&fsl_chan->vchan.lock);
	/* Ignore this interrupt since channel has been freeed with power off */
	if (!fsl_chan->edesc && !fsl_chan->tcd_pool)
		goto irq_handled;

	base_addr = fsl_chan->membase;

	intr = readl(base_addr + EDMA_CH_INT);
	if (!intr)
		goto irq_handled;

	writel(1, base_addr + EDMA_CH_INT);

	/* Ignore this interrupt since channel has been disabled already */
	if (!fsl_chan->edesc)
		goto irq_handled;

	if (!fsl_chan->edesc->iscyclic) {
		fsl_edma3_get_realcnt(fsl_chan);
		list_del(&fsl_chan->edesc->vdesc.node);
		vchan_cookie_complete(&fsl_chan->edesc->vdesc);
		fsl_chan->edesc = NULL;
		fsl_chan->status = DMA_COMPLETE;
		fsl_chan->idle = true;
	} else {
		vchan_cyclic_callback(&fsl_chan->edesc->vdesc);
	}

	if (!fsl_chan->edesc)
		fsl_edma3_xfer_desc(fsl_chan);
irq_handled:
	spin_unlock(&fsl_chan->vchan.lock);

	return IRQ_HANDLED;
}

static void fsl_edma3_issue_pending(struct dma_chan *chan)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);

	if (vchan_issue_pending(&fsl_chan->vchan) && !fsl_chan->edesc)
		fsl_edma3_xfer_desc(fsl_chan);

	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);
}

static struct dma_chan *fsl_edma3_xlate(struct of_phandle_args *dma_spec,
		struct of_dma *ofdma)
{
	struct fsl_edma3_engine *fsl_edma3 = ofdma->of_dma_data;
	struct dma_chan *chan, *_chan;
	struct fsl_edma3_chan *fsl_chan;
	bool srcid_used = false;

	if (dma_spec->args_count != 3)
		return NULL;

	mutex_lock(&fsl_edma3->fsl_edma3_mutex);
	list_for_each_entry_safe(chan, _chan, &fsl_edma3->dma_dev.channels,
					device_node) {
		if (chan->client_count)
			continue;

		fsl_chan = to_fsl_edma3_chan(chan);
		srcid_used = is_srcid_in_use(fsl_edma3, dma_spec->args[0]);
		if (srcid_used) {
			dev_err(&fsl_chan->pdev->dev, "The srcid %d has been used. Please check srcid config!",
				dma_spec->args[0]);
			return NULL;
		}

		if (fsl_edma3->drvdata->dmamuxs == 0 &&
		    fsl_chan->hw_chanid == dma_spec->args[0]) {
			chan = dma_get_slave_channel(chan);
			chan->device->privatecnt++;
			fsl_chan->priority = dma_spec->args[1];
			fsl_chan->is_rxchan = dma_spec->args[2] & ARGS_RX;
			fsl_chan->is_remote = dma_spec->args[2] & ARGS_REMOTE;
			fsl_chan->is_multi_fifo = dma_spec->args[2] & ARGS_MULTI_FIFO;
			mutex_unlock(&fsl_edma3->fsl_edma3_mutex);
			return chan;
		} else if ((fsl_edma3->drvdata->dmamuxs || fsl_edma3->bus_axi) &&
			   !fsl_chan->srcid) {
			chan = dma_get_slave_channel(chan);
			chan->device->privatecnt++;
			fsl_chan->priority = dma_spec->args[1];
			fsl_chan->srcid = dma_spec->args[0];
			fsl_chan->is_rxchan = dma_spec->args[2] & ARGS_RX;
			fsl_chan->is_remote = dma_spec->args[2] & ARGS_REMOTE;
			fsl_chan->is_multi_fifo = dma_spec->args[2] & ARGS_MULTI_FIFO;
			mutex_unlock(&fsl_edma3->fsl_edma3_mutex);
			return chan;
		}
	}
	mutex_unlock(&fsl_edma3->fsl_edma3_mutex);
	return NULL;
}

static int fsl_edma3_alloc_chan_resources(struct dma_chan *chan)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	struct platform_device *pdev = fsl_chan->pdev;
	int ret;

	if (fsl_chan->edma3->drvdata->has_chclk)
		clk_prepare_enable(fsl_chan->clk);

	if (fsl_chan->edma3->drvdata->edma_v5)
		fsl_chan->tcd_pool = dma_pool_create("tcd_pool", chan->device->dev,
				sizeof(struct fsl_edma3_hw_tcd_v5),
				32, 0);
	else
		fsl_chan->tcd_pool = dma_pool_create("tcd_pool", chan->device->dev,
				sizeof(struct fsl_edma3_hw_tcd),
				32, 0);

	if (fsl_chan->edma3->drvdata->has_pd) {
		pm_runtime_get_sync(fsl_chan->dev);
		/* clear meaningless pending irq anyway */
		if (readl(fsl_chan->membase + EDMA_CH_INT))
			writel(1, fsl_chan->membase + EDMA_CH_INT);
	}

	ret = devm_request_irq(&pdev->dev, fsl_chan->txirq,
			fsl_edma3_tx_handler, fsl_chan->edma3->irqflag,
			fsl_chan->txirq_name, fsl_chan);
	if (ret) {
		dev_err(&pdev->dev, "Can't register %s IRQ.\n",
			fsl_chan->txirq_name);
		if (fsl_chan->edma3->drvdata->has_pd)
			pm_runtime_put_sync_suspend(fsl_chan->dev);

		return ret;
	}

	if (!fsl_chan->edma3->drvdata->errirq_share) {
		ret = devm_request_irq(&pdev->dev, fsl_chan->errirq,
				fsl_edma3_err_handler, fsl_chan->edma3->irqflag,
				fsl_chan->errirq_name, fsl_chan);
		if (ret) {
			dev_err(&pdev->dev, "Can't register %s err IRQ.\n",
				fsl_chan->errirq_name);
			if (fsl_chan->edma3->drvdata->has_pd)
				pm_runtime_put_sync_suspend(fsl_chan->dev);

			return ret;
		}
	}

	return 0;
}

static void fsl_edma3_free_chan_resources(struct dma_chan *chan)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);
	unsigned long flags;
	LIST_HEAD(head);

	devm_free_irq(&fsl_chan->pdev->dev, fsl_chan->txirq, fsl_chan);

	if (!fsl_chan->edma3->drvdata->errirq_share)
		devm_free_irq(&fsl_chan->pdev->dev, fsl_chan->errirq, fsl_chan);

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	fsl_edma3_disable_request(fsl_chan);
	fsl_chan->edesc = NULL;
	vchan_get_all_descriptors(&fsl_chan->vchan, &head);
	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);

	vchan_dma_desc_free_list(&fsl_chan->vchan, &head);
	dma_pool_destroy(fsl_chan->tcd_pool);

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	fsl_chan->tcd_pool = NULL;
	fsl_chan->srcid = 0;
	/* Clear interrupt before power off */
	if (readl(fsl_chan->membase + EDMA_CH_INT))
		writel(1, fsl_chan->membase + EDMA_CH_INT);
	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);

	if (fsl_chan->edma3->drvdata->has_pd)
		pm_runtime_put_sync_suspend(fsl_chan->dev);

	if (fsl_chan->edma3->drvdata->has_chclk)
		clk_disable_unprepare(fsl_chan->clk);

	fsl_chan->is_sw = false;
	fsl_chan->is_remote = false;
}

static void fsl_edma3_synchronize(struct dma_chan *chan)
{
	struct fsl_edma3_chan *fsl_chan = to_fsl_edma3_chan(chan);

	if (fsl_chan->status == DMA_PAUSED)
		fsl_edma3_terminate_all(chan);

	vchan_synchronize(&fsl_chan->vchan);
}

static struct device *fsl_edma3_attach_pd(struct device *dev,
					  struct device_node *np, int index)
{
	const char *domn = "edma0-chan01";
	struct device *pd_chan;
	struct device_link *link;
	int ret;

	ret = of_property_read_string_index(np, "power-domain-names", index,
						&domn);
	if (ret) {
		dev_err(dev, "parse power-domain-names error.(%d)\n", ret);
		return NULL;
	}

	pd_chan = dev_pm_domain_attach_by_name(dev, domn);
	if (IS_ERR_OR_NULL(pd_chan))
		return NULL;

	link = device_link_add(dev, pd_chan, DL_FLAG_STATELESS |
					     DL_FLAG_PM_RUNTIME |
					     DL_FLAG_RPM_ACTIVE);
	if (IS_ERR(link)) {
		dev_err(dev, "Failed to add device_link to %s: %ld\n", domn,
			PTR_ERR(link));
		return NULL;
	}

	return pd_chan;
}

static const struct of_device_id fsl_edma3_dt_ids[] = {
	{ .compatible = "fsl,imx8qm-edma", .data = &fsl_edma_imx8q},
	{ .compatible = "fsl,imx8ulp-edma", .data = &fsl_edma_imx8ulp},
	{ .compatible = "fsl,imx93-edma", .data = &fsl_edma_imx93},
	{ .compatible = "fsl,imx95-edma", .data = &fsl_edma_imx95},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, fsl_edma3_dt_ids);

static int fsl_edma3_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id =
			of_match_device(fsl_edma3_dt_ids, &pdev->dev);
	struct fsl_edma3_engine *fsl_edma3;
	struct fsl_edma3_chan *fsl_chan;
	struct resource *res_mp;
	struct resource *res;
	char *errirq_name;
	int len, chans;
	int ret, i;
	void __iomem *mp_membase, *mp_chan_membase;

	if (!of_id)
		return -EINVAL;

	ret = of_property_read_u32(np, "dma-channels", &chans);
	if (ret) {
		dev_err(&pdev->dev, "Can't get dma-channels.\n");
		return ret;
	}

	len = sizeof(*fsl_edma3) + sizeof(*fsl_chan) * chans;
	fsl_edma3 = devm_kzalloc(&pdev->dev, len, GFP_KERNEL);
	if (!fsl_edma3)
		return -ENOMEM;

	/* Audio edma rx/tx channel shared interrupt */
	if (of_property_read_bool(np, "shared-interrupt"))
		fsl_edma3->irqflag = IRQF_SHARED;

	fsl_edma3->n_chans = chans;
	fsl_edma3->drvdata = (const struct fsl_edma3_drvdata *)of_id->data;

	INIT_LIST_HEAD(&fsl_edma3->dma_dev.channels);

	res_mp = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mp_membase = devm_ioremap_resource(&pdev->dev, res_mp);
	if (IS_ERR(mp_membase))
		return PTR_ERR(mp_membase);

	if (of_property_read_bool(np, "fsl,edma-axi"))
		fsl_edma3->bus_axi = true;

	if (fsl_edma3->drvdata->has_chclk) {
		fsl_edma3->clk_mp = devm_clk_get(&pdev->dev, "edma-mp-clk");
		if (IS_ERR(fsl_edma3->clk_mp)) {
			dev_err(&pdev->dev, "Can't get mp clk.\n");
			return PTR_ERR(fsl_edma3->clk_mp);
		}
		clk_prepare_enable(fsl_edma3->clk_mp);
	} else {
		fsl_edma3->dmaclk = devm_clk_get_optional(&pdev->dev, "edma");
		if (IS_ERR(fsl_edma3->dmaclk)) {
			dev_err(&pdev->dev, "Missing DMA block clock.\n");
			return PTR_ERR(fsl_edma3->dmaclk);
		}
		clk_prepare_enable(fsl_edma3->dmaclk);
	}

	for (i = 0; i < fsl_edma3->n_chans; i++) {
		struct fsl_edma3_chan *fsl_chan = &fsl_edma3->chans[i];
		const char *txirq_name;
		char chanid[3], id_len = 0;
		char clk_name[18];
		char *p = chanid;
		unsigned long val;

		fsl_chan->edma3 = fsl_edma3;
		fsl_chan->pdev = pdev;
		fsl_chan->idle = true;
		fsl_chan->srcid = 0;

		if (fsl_chan->edma3->drvdata->mp_chmux) {
			/* Get mp membase */
			mp_chan_membase = mp_membase + i * EDMA_ADDR_WIDTH;
			if (IS_ERR(mp_chan_membase))
				return PTR_ERR(mp_chan_membase);
			fsl_chan->mux_addr = mp_chan_membase + EDMA_MP_CH_MUX;
		}
		/* Get per channel membase */
		res = platform_get_resource(pdev, IORESOURCE_MEM, i + 1);
		fsl_chan->membase = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(fsl_chan->membase))
			return PTR_ERR(fsl_chan->membase);

		if ((fsl_chan->edma3->drvdata->has_chmux || fsl_edma3->bus_axi) &&
		    !fsl_chan->edma3->drvdata->mp_chmux)
			fsl_chan->mux_addr = fsl_chan->membase + EDMA_CH_MUX;

		if (fsl_edma3->bus_axi) {
			fsl_chan->hw_chanid = ((res->start - res_mp->start) >> 15) & 0x7f;
			fsl_chan->hw_chanid = fsl_chan->hw_chanid - 2;
		} else {
			/* Get the hardware chanel id by the channel membase
			 * channel0:0x10000, channel1:0x20000... total 32 channels
			 * Note: skip first res_mp which we don't care.
			 */
			fsl_chan->hw_chanid = ((res->start - res_mp->start) >> 16) & 0x3f;
			fsl_chan->hw_chanid--;
		}

		ret = of_property_read_string_index(np, "interrupt-names", i,
							&txirq_name);
		if (ret) {
			dev_err(&pdev->dev, "read interrupt-names fail.\n");
			return ret;
		}
		/* Get channel id length from dts, one-digit or double-digit */
		id_len = strlen(txirq_name) - strlen(CHAN_PREFIX) -
			 strlen(CHAN_POSFIX);
		if (id_len > 2) {
			dev_err(&pdev->dev, "%s is edmaX-chanX-tx in dts?\n",
				res->name);
			return -EINVAL;
		}
		/* Grab channel id from txirq_name */
		strncpy(p, txirq_name + strlen(CHAN_PREFIX), id_len);
		*(p + id_len) = '\0';

		/* check if the channel id match well with hw_chanid */
		ret = kstrtoul(chanid, 0, &val);
		if (ret || val != fsl_chan->hw_chanid) {
			dev_err(&pdev->dev, "%s,wrong id?\n", txirq_name);
			return -EINVAL;
		}

		/* request channel irq */
		fsl_chan->txirq = platform_get_irq_byname(pdev, txirq_name);
		if (fsl_chan->txirq < 0) {
			dev_err(&pdev->dev, "Can't get %s irq.\n", txirq_name);
			return fsl_chan->txirq;
		}

		memcpy(fsl_chan->txirq_name, txirq_name, strlen(txirq_name));

		if (!fsl_edma3->drvdata->errirq_share) {
			fsl_chan->errirq = platform_get_irq(pdev, i);
			snprintf(fsl_chan->errirq_name, sizeof(fsl_chan->errirq_name),
				 "%s-err", fsl_chan->txirq_name);
		}

		if (fsl_edma3->drvdata->has_chclk) {
			strncpy(clk_name, txirq_name, strlen(CHAN_PREFIX) + id_len);
			strcpy(clk_name + strlen(CHAN_PREFIX) + id_len, CLK_POSFIX);
			fsl_chan->clk = devm_clk_get(&pdev->dev,
							     (const char *)clk_name);

			if (IS_ERR(fsl_chan->clk))
				return PTR_ERR(fsl_chan->clk);
		}

		fsl_chan->vchan.desc_free = fsl_edma3_free_desc;
		vchan_init(&fsl_chan->vchan, &fsl_edma3->dma_dev);
	}

	if (fsl_edma3->drvdata->errirq_share) {
		fsl_edma3->errirq = platform_get_irq(pdev, fsl_edma3->n_chans);
		if (fsl_edma3->errirq < 0)
			return fsl_edma3->errirq;

		errirq_name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s-err",
					     dev_name(&pdev->dev));
		ret = devm_request_irq(&pdev->dev, fsl_edma3->errirq, fsl_edma3_err_handler,
				       0, errirq_name, &fsl_edma3->chans[0]);
		if (ret) {
			dev_err(&pdev->dev, "Can't register eDMA err IRQ.\n");
			return ret;
		}
	}
	mutex_init(&fsl_edma3->fsl_edma3_mutex);

	dma_cap_set(DMA_PRIVATE, fsl_edma3->dma_dev.cap_mask);
	dma_cap_set(DMA_SLAVE, fsl_edma3->dma_dev.cap_mask);
	dma_cap_set(DMA_CYCLIC, fsl_edma3->dma_dev.cap_mask);
	dma_cap_set(DMA_MEMCPY, fsl_edma3->dma_dev.cap_mask);
	if (fsl_edma3->drvdata->edma_v5)
		dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));

	fsl_edma3->dma_dev.dev = &pdev->dev;
	fsl_edma3->dma_dev.device_alloc_chan_resources
		= fsl_edma3_alloc_chan_resources;
	fsl_edma3->dma_dev.device_free_chan_resources
		= fsl_edma3_free_chan_resources;
	fsl_edma3->dma_dev.device_tx_status = fsl_edma3_tx_status;
	fsl_edma3->dma_dev.device_prep_slave_sg = fsl_edma3_prep_slave_sg;
	fsl_edma3->dma_dev.device_prep_dma_memcpy = fsl_edma3_prep_memcpy;
	fsl_edma3->dma_dev.device_prep_dma_cyclic = fsl_edma3_prep_dma_cyclic;
	fsl_edma3->dma_dev.device_config = fsl_edma3_slave_config;
	fsl_edma3->dma_dev.device_pause = fsl_edma3_pause;
	fsl_edma3->dma_dev.device_resume = fsl_edma3_resume;
	fsl_edma3->dma_dev.device_terminate_all = fsl_edma3_terminate_all;
	fsl_edma3->dma_dev.device_issue_pending = fsl_edma3_issue_pending;
	fsl_edma3->dma_dev.device_synchronize = fsl_edma3_synchronize;
	fsl_edma3->dma_dev.residue_granularity = DMA_RESIDUE_GRANULARITY_SEGMENT;

	fsl_edma3->dma_dev.src_addr_widths = FSL_EDMA_BUSWIDTHS;
	fsl_edma3->dma_dev.dst_addr_widths = FSL_EDMA_BUSWIDTHS;
	fsl_edma3->dma_dev.directions = BIT(DMA_DEV_TO_MEM) |
					BIT(DMA_MEM_TO_DEV) |
					BIT(DMA_DEV_TO_DEV);

	fsl_edma3->dma_dev.copy_align = DMAENGINE_ALIGN_64_BYTES;
	/* Per worst case 'nbytes = 1' take CITER as the max_seg_size */
	dma_set_max_seg_size(fsl_edma3->dma_dev.dev, 0x7fff);

	platform_set_drvdata(pdev, fsl_edma3);

	ret = dma_async_device_register(&fsl_edma3->dma_dev);
	if (ret) {
		dev_err(&pdev->dev, "Can't register Freescale eDMA engine.\n");
		return ret;
	}
	/* Attach power domains from dts for each dma chanel device */
	if (fsl_edma3->drvdata->has_pd) {
		for (i = 0; i < fsl_edma3->n_chans; i++) {
			struct fsl_edma3_chan *fsl_chan = &fsl_edma3->chans[i];
			struct device *dev;

			dev = fsl_edma3_attach_pd(&pdev->dev, np, i);
			if (!dev) {
				dev_err(dev, "edma channel attach failed.\n");
				return -EINVAL;
			}

			fsl_chan->dev = dev;
			/* clear meaningless pending irq anyway */
			writel(1, fsl_chan->membase + EDMA_CH_INT);

			pm_runtime_set_active(fsl_chan->dev);
			pm_runtime_put_sync_suspend(fsl_chan->dev);
		}
	}

	ret = of_dma_controller_register(np, fsl_edma3_xlate, fsl_edma3);
	if (ret) {
		dev_err(&pdev->dev, "Can't register Freescale eDMA of_dma.\n");
		dma_async_device_unregister(&fsl_edma3->dma_dev);
		return ret;
	}

	return 0;
}

static int fsl_edma3_remove(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct fsl_edma3_engine *fsl_edma3 = platform_get_drvdata(pdev);

	of_dma_controller_free(np);
	dma_async_device_unregister(&fsl_edma3->dma_dev);

	if (fsl_edma3->drvdata->errirq_share)
		devm_free_irq(&pdev->dev, fsl_edma3->errirq, &fsl_edma3->chans[0]);

	if (fsl_edma3->drvdata->has_chclk)
		clk_disable_unprepare(fsl_edma3->clk_mp);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int fsl_edma3_suspend_late(struct device *dev)
{
	struct fsl_edma3_engine *fsl_edma = dev_get_drvdata(dev);
	struct fsl_edma3_chan *fsl_chan;
	unsigned long flags;
	void __iomem *addr;
	int i;

	for (i = 0; i < fsl_edma->n_chans; i++) {
		fsl_chan = &fsl_edma->chans[i];
		addr = fsl_chan->membase;

		if ((fsl_chan->edma3->drvdata->has_pd &&
		    pm_runtime_status_suspended(fsl_chan->dev)) ||
		    (!fsl_chan->edma3->drvdata->has_pd && !fsl_chan->srcid))
			continue;

		spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
		fsl_edma->edma_regs[i].csr = readl(addr + EDMA_CH_CSR);
		fsl_edma->edma_regs[i].sbr = readl(addr + EDMA_CH_SBR);
		/* Make sure chan is idle or will force disable. */
		if (unlikely(!fsl_chan->idle)) {
			dev_warn(dev, "WARN: There is non-idle channel.");
			fsl_edma3_disable_request(fsl_chan);
		}
		spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);
	}

	return 0;
}

static int fsl_edma3_resume_early(struct device *dev)
{
	struct fsl_edma3_engine *fsl_edma = dev_get_drvdata(dev);
	struct fsl_edma3_chan *fsl_chan;
	void __iomem *addr;
	unsigned long flags;
	int i;

	for (i = 0; i < fsl_edma->n_chans; i++) {
		fsl_chan = &fsl_edma->chans[i];
		addr = fsl_chan->membase;

		if ((fsl_chan->edma3->drvdata->has_pd &&
		    pm_runtime_status_suspended(fsl_chan->dev)) ||
		    (!fsl_chan->edma3->drvdata->has_pd && !fsl_chan->srcid))
			continue;

		spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
		writel(fsl_edma->edma_regs[i].csr, addr + EDMA_CH_CSR);
		writel(fsl_edma->edma_regs[i].sbr, addr + EDMA_CH_SBR);
		/* restore tcd if this channel not terminated before suspend */
		if (fsl_chan->edesc)
			fsl_edma3_set_tcd_regs(fsl_chan,
						fsl_chan->edesc->tcd[0].vtcd);
		spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);
	}

	return 0;
}
#endif

static const struct dev_pm_ops fsl_edma3_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(fsl_edma3_suspend_late,
				     fsl_edma3_resume_early)
};

static struct platform_driver fsl_edma3_driver = {
	.driver		= {
		.name	= "fsl-edma-v3",
		.of_match_table = fsl_edma3_dt_ids,
		.pm     = &fsl_edma3_pm_ops,
	},
	.probe          = fsl_edma3_probe,
	.remove		= fsl_edma3_remove,
};

static int __init fsl_edma3_init(void)
{
	return platform_driver_register(&fsl_edma3_driver);
}
fs_initcall(fsl_edma3_init);

static void __exit fsl_edma3_exit(void)
{
	platform_driver_unregister(&fsl_edma3_driver);
}
module_exit(fsl_edma3_exit);

MODULE_ALIAS("platform:fsl-edma3");
MODULE_DESCRIPTION("Freescale eDMA-V3 engine driver");
MODULE_LICENSE("GPL v2");
