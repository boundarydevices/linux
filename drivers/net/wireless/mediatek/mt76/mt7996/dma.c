// SPDX-License-Identifier: ISC
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#include "mt7996.h"
#include "../dma.h"
#include "mac.h"

int mt7996_init_tx_queues(struct mt7996_phy *phy, int idx, int n_desc,
			  int ring_base, struct mtk_wed_device *wed)
{
	struct mt7996_dev *dev = phy->dev;
	u32 flags = 0;

	if (mtk_wed_device_active(wed)) {
		ring_base += MT_TXQ_ID(0) * MT_RING_SIZE;
		idx -= MT_TXQ_ID(0);

		if (phy->mt76->band_idx == MT_BAND2)
			flags = MT_WED_Q_TX(0);
		else
			flags = MT_WED_Q_TX(idx);
	}

	return mt76_connac_init_tx_queues(phy->mt76, idx, n_desc,
					  ring_base, wed, flags);
}

static int mt7996_poll_tx(struct napi_struct *napi, int budget)
{
	struct mt7996_dev *dev;

	dev = container_of(napi, struct mt7996_dev, mt76.tx_napi);

	mt76_connac_tx_cleanup(&dev->mt76);
	if (napi_complete_done(napi, 0))
		mt7996_irq_enable(dev, MT_INT_TX_DONE_MCU);

	return 0;
}

static void mt7996_dma_config(struct mt7996_dev *dev)
{
#define Q_CONFIG(q, wfdma, int, id) do {		\
	if (wfdma)					\
		dev->q_wfdma_mask |= (1 << (q));	\
	dev->q_int_mask[(q)] = int;			\
	dev->q_id[(q)] = id;				\
} while (0)

#define MCUQ_CONFIG(q, wfdma, int, id)	Q_CONFIG(q, (wfdma), (int), (id))
#define RXQ_CONFIG(q, wfdma, int, id)	Q_CONFIG(__RXQ(q), (wfdma), (int), (id))
#define TXQ_CONFIG(q, wfdma, int, id)	Q_CONFIG(__TXQ(q), (wfdma), (int), (id))

	/* rx queue */
	RXQ_CONFIG(MT_RXQ_MCU, WFDMA0, MT_INT_RX_DONE_WM, MT7996_RXQ_MCU_WM);
	RXQ_CONFIG(MT_RXQ_MCU_WA, WFDMA0, MT_INT_RX_DONE_WA, MT7996_RXQ_MCU_WA);

	/* band0/band1 */
	RXQ_CONFIG(MT_RXQ_MAIN, WFDMA0, MT_INT_RX_DONE_BAND0, MT7996_RXQ_BAND0);
	RXQ_CONFIG(MT_RXQ_MAIN_WA, WFDMA0, MT_INT_RX_DONE_WA_MAIN, MT7996_RXQ_MCU_WA_MAIN);

	/* band2 */
	RXQ_CONFIG(MT_RXQ_BAND2, WFDMA0, MT_INT_RX_DONE_BAND2, MT7996_RXQ_BAND2);
	RXQ_CONFIG(MT_RXQ_BAND2_WA, WFDMA0, MT_INT_RX_DONE_WA_TRI, MT7996_RXQ_MCU_WA_TRI);

	if (dev->has_rro) {
		/* band0 */
		RXQ_CONFIG(MT_RXQ_RRO_BAND0, WFDMA0, MT_INT_RX_DONE_RRO_BAND0,
			   MT7996_RXQ_RRO_BAND0);
		RXQ_CONFIG(MT_RXQ_MSDU_PAGE_BAND0, WFDMA0, MT_INT_RX_DONE_MSDU_PG_BAND0,
			   MT7996_RXQ_MSDU_PG_BAND0);
		RXQ_CONFIG(MT_RXQ_TXFREE_BAND0, WFDMA0, MT_INT_RX_TXFREE_MAIN,
			   MT7996_RXQ_TXFREE0);
		/* band1 */
		RXQ_CONFIG(MT_RXQ_MSDU_PAGE_BAND1, WFDMA0, MT_INT_RX_DONE_MSDU_PG_BAND1,
			   MT7996_RXQ_MSDU_PG_BAND1);
		/* band2 */
		RXQ_CONFIG(MT_RXQ_RRO_BAND2, WFDMA0, MT_INT_RX_DONE_RRO_BAND2,
			   MT7996_RXQ_RRO_BAND2);
		RXQ_CONFIG(MT_RXQ_MSDU_PAGE_BAND2, WFDMA0, MT_INT_RX_DONE_MSDU_PG_BAND2,
			   MT7996_RXQ_MSDU_PG_BAND2);
		RXQ_CONFIG(MT_RXQ_TXFREE_BAND2, WFDMA0, MT_INT_RX_TXFREE_TRI,
			   MT7996_RXQ_TXFREE2);

		RXQ_CONFIG(MT_RXQ_RRO_IND, WFDMA0, MT_INT_RX_DONE_RRO_IND,
			   MT7996_RXQ_RRO_IND);
	}

	/* data tx queue */
	TXQ_CONFIG(0, WFDMA0, MT_INT_TX_DONE_BAND0, MT7996_TXQ_BAND0);
	TXQ_CONFIG(1, WFDMA0, MT_INT_TX_DONE_BAND1, MT7996_TXQ_BAND1);
	TXQ_CONFIG(2, WFDMA0, MT_INT_TX_DONE_BAND2, MT7996_TXQ_BAND2);

	/* mcu tx queue */
	MCUQ_CONFIG(MT_MCUQ_WM, WFDMA0, MT_INT_TX_DONE_MCU_WM, MT7996_TXQ_MCU_WM);
	MCUQ_CONFIG(MT_MCUQ_WA, WFDMA0, MT_INT_TX_DONE_MCU_WA, MT7996_TXQ_MCU_WA);
	MCUQ_CONFIG(MT_MCUQ_FWDL, WFDMA0, MT_INT_TX_DONE_FWDL, MT7996_TXQ_FWDL);
}

static void __mt7996_dma_prefetch(struct mt7996_dev *dev, u32 ofs)
{
#define PREFETCH(_base, _depth)	((_base) << 16 | (_depth))
	/* prefetch SRAM wrapping boundary for tx/rx ring. */
	mt76_wr(dev, MT_MCUQ_EXT_CTRL(MT_MCUQ_FWDL) + ofs, PREFETCH(0x0, 0x2));
	mt76_wr(dev, MT_MCUQ_EXT_CTRL(MT_MCUQ_WM) + ofs, PREFETCH(0x20, 0x2));
	mt76_wr(dev, MT_TXQ_EXT_CTRL(0) + ofs, PREFETCH(0x40, 0x4));
	mt76_wr(dev, MT_TXQ_EXT_CTRL(1) + ofs, PREFETCH(0x80, 0x4));
	mt76_wr(dev, MT_MCUQ_EXT_CTRL(MT_MCUQ_WA) + ofs, PREFETCH(0xc0, 0x2));
	mt76_wr(dev, MT_TXQ_EXT_CTRL(2) + ofs, PREFETCH(0xe0, 0x4));
	mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_MCU) + ofs, PREFETCH(0x120, 0x2));
	mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_MCU_WA) + ofs, PREFETCH(0x140, 0x2));
	mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_MAIN_WA) + ofs, PREFETCH(0x160, 0x2));
	mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_BAND2_WA) + ofs, PREFETCH(0x180, 0x2));
	mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_MAIN) + ofs, PREFETCH(0x1a0, 0x10));
	mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_BAND2) + ofs, PREFETCH(0x2a0, 0x10));

	if (dev->has_rro) {
		mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_RRO_BAND0) + ofs,
			PREFETCH(0x3a0, 0x10));
		mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_RRO_BAND2) + ofs,
			PREFETCH(0x4a0, 0x10));
		mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_MSDU_PAGE_BAND0) + ofs,
			PREFETCH(0x5a0, 0x4));
		mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_MSDU_PAGE_BAND1) + ofs,
			PREFETCH(0x5e0, 0x4));
		mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_MSDU_PAGE_BAND2) + ofs,
			PREFETCH(0x620, 0x4));
		mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_TXFREE_BAND0) + ofs,
			PREFETCH(0x660, 0x4));
		mt76_wr(dev, MT_RXQ_BAND1_CTRL(MT_RXQ_TXFREE_BAND2) + ofs,
			PREFETCH(0x6a0, 0x4));
	}
#undef PREFETCH

	mt76_set(dev, WF_WFDMA0_GLO_CFG_EXT1 + ofs, WF_WFDMA0_GLO_CFG_EXT1_CALC_MODE);
}

void mt7996_dma_prefetch(struct mt7996_dev *dev)
{
	__mt7996_dma_prefetch(dev, 0);
	if (dev->hif2)
		__mt7996_dma_prefetch(dev, MT_WFDMA0_PCIE1(0) - MT_WFDMA0(0));
}

static void mt7996_dma_disable(struct mt7996_dev *dev, bool reset)
{
	u32 hif1_ofs = 0;

	if (dev->hif2)
		hif1_ofs = MT_WFDMA0_PCIE1(0) - MT_WFDMA0(0);

	if (reset) {
		mt76_clear(dev, MT_WFDMA0_RST,
			   MT_WFDMA0_RST_DMASHDL_ALL_RST |
			   MT_WFDMA0_RST_LOGIC_RST);

		mt76_set(dev, MT_WFDMA0_RST,
			 MT_WFDMA0_RST_DMASHDL_ALL_RST |
			 MT_WFDMA0_RST_LOGIC_RST);

		if (dev->hif2) {
			mt76_clear(dev, MT_WFDMA0_RST + hif1_ofs,
				   MT_WFDMA0_RST_DMASHDL_ALL_RST |
				   MT_WFDMA0_RST_LOGIC_RST);

			mt76_set(dev, MT_WFDMA0_RST + hif1_ofs,
				 MT_WFDMA0_RST_DMASHDL_ALL_RST |
				 MT_WFDMA0_RST_LOGIC_RST);
		}
	}

	/* disable */
	mt76_clear(dev, MT_WFDMA0_GLO_CFG,
		   MT_WFDMA0_GLO_CFG_TX_DMA_EN |
		   MT_WFDMA0_GLO_CFG_RX_DMA_EN |
		   MT_WFDMA0_GLO_CFG_OMIT_TX_INFO |
		   MT_WFDMA0_GLO_CFG_OMIT_RX_INFO |
		   MT_WFDMA0_GLO_CFG_OMIT_RX_INFO_PFET2);

	if (dev->hif2) {
		mt76_clear(dev, MT_WFDMA0_GLO_CFG + hif1_ofs,
			   MT_WFDMA0_GLO_CFG_TX_DMA_EN |
			   MT_WFDMA0_GLO_CFG_RX_DMA_EN |
			   MT_WFDMA0_GLO_CFG_OMIT_TX_INFO |
			   MT_WFDMA0_GLO_CFG_OMIT_RX_INFO |
			   MT_WFDMA0_GLO_CFG_OMIT_RX_INFO_PFET2);
	}
}

void mt7996_dma_start(struct mt7996_dev *dev, bool reset, bool wed_reset)
{
	struct mtk_wed_device *wed = &dev->mt76.mmio.wed;
	u32 hif1_ofs = 0;
	u32 irq_mask;

	if (dev->hif2)
		hif1_ofs = MT_WFDMA0_PCIE1(0) - MT_WFDMA0(0);

	/* enable WFDMA Tx/Rx */
	if (!reset) {
		if (mtk_wed_device_active(wed) && mtk_wed_get_rx_capa(wed))
			mt76_set(dev, MT_WFDMA0_GLO_CFG,
				 MT_WFDMA0_GLO_CFG_TX_DMA_EN |
				 MT_WFDMA0_GLO_CFG_OMIT_TX_INFO);
		else
			mt76_set(dev, MT_WFDMA0_GLO_CFG,
				 MT_WFDMA0_GLO_CFG_TX_DMA_EN |
				 MT_WFDMA0_GLO_CFG_RX_DMA_EN |
				 MT_WFDMA0_GLO_CFG_OMIT_TX_INFO |
				 MT_WFDMA0_GLO_CFG_OMIT_RX_INFO_PFET2);

		if (dev->hif2)
			mt76_set(dev, MT_WFDMA0_GLO_CFG + hif1_ofs,
				 MT_WFDMA0_GLO_CFG_TX_DMA_EN |
				 MT_WFDMA0_GLO_CFG_RX_DMA_EN |
				 MT_WFDMA0_GLO_CFG_OMIT_TX_INFO |
				 MT_WFDMA0_GLO_CFG_OMIT_RX_INFO_PFET2);
	}

	/* enable interrupts for TX/RX rings */
	irq_mask = MT_INT_MCU_CMD | MT_INT_RX_DONE_MCU | MT_INT_TX_DONE_MCU;

	if (!dev->mphy.band_idx)
		irq_mask |= MT_INT_BAND0_RX_DONE;

	if (dev->dbdc_support)
		irq_mask |= MT_INT_BAND1_RX_DONE;

	if (dev->tbtc_support)
		irq_mask |= MT_INT_BAND2_RX_DONE;

	if (mtk_wed_device_active(wed) && wed_reset) {
		u32 wed_irq_mask = irq_mask;

		wed_irq_mask |= MT_INT_TX_DONE_BAND0 | MT_INT_TX_DONE_BAND1;
		mt76_wr(dev, MT_INT_MASK_CSR, wed_irq_mask);
		mtk_wed_device_start(wed, wed_irq_mask);
	}

	irq_mask = reset ? MT_INT_MCU_CMD : irq_mask;

	mt7996_irq_enable(dev, irq_mask);
	mt7996_irq_disable(dev, 0);
}

static void mt7996_dma_enable(struct mt7996_dev *dev, bool reset)
{
	u32 hif1_ofs = 0;

	if (dev->hif2)
		hif1_ofs = MT_WFDMA0_PCIE1(0) - MT_WFDMA0(0);

	/* reset dma idx */
	mt76_wr(dev, MT_WFDMA0_RST_DTX_PTR, ~0);
	if (dev->hif2)
		mt76_wr(dev, MT_WFDMA0_RST_DTX_PTR + hif1_ofs, ~0);

	/* configure delay interrupt off */
	mt76_wr(dev, MT_WFDMA0_PRI_DLY_INT_CFG0, 0);
	mt76_wr(dev, MT_WFDMA0_PRI_DLY_INT_CFG1, 0);
	mt76_wr(dev, MT_WFDMA0_PRI_DLY_INT_CFG2, 0);

	if (dev->hif2) {
		mt76_wr(dev, MT_WFDMA0_PRI_DLY_INT_CFG0 + hif1_ofs, 0);
		mt76_wr(dev, MT_WFDMA0_PRI_DLY_INT_CFG1 + hif1_ofs, 0);
		mt76_wr(dev, MT_WFDMA0_PRI_DLY_INT_CFG2 + hif1_ofs, 0);
	}

	/* configure perfetch settings */
	mt7996_dma_prefetch(dev);

	/* hif wait WFDMA idle */
	mt76_set(dev, MT_WFDMA0_BUSY_ENA,
		 MT_WFDMA0_BUSY_ENA_TX_FIFO0 |
		 MT_WFDMA0_BUSY_ENA_TX_FIFO1 |
		 MT_WFDMA0_BUSY_ENA_RX_FIFO);

	if (dev->hif2)
		mt76_set(dev, MT_WFDMA0_BUSY_ENA + hif1_ofs,
			 MT_WFDMA0_PCIE1_BUSY_ENA_TX_FIFO0 |
			 MT_WFDMA0_PCIE1_BUSY_ENA_TX_FIFO1 |
			 MT_WFDMA0_PCIE1_BUSY_ENA_RX_FIFO);

	mt76_poll(dev, MT_WFDMA_EXT_CSR_HIF_MISC,
		  MT_WFDMA_EXT_CSR_HIF_MISC_BUSY, 0, 1000);

	/* GLO_CFG_EXT0 */
	mt76_set(dev, WF_WFDMA0_GLO_CFG_EXT0,
		 WF_WFDMA0_GLO_CFG_EXT0_RX_WB_RXD |
		 WF_WFDMA0_GLO_CFG_EXT0_WED_MERGE_MODE);

	/* GLO_CFG_EXT1 */
	mt76_set(dev, WF_WFDMA0_GLO_CFG_EXT1,
		 WF_WFDMA0_GLO_CFG_EXT1_TX_FCTRL_MODE);

	if (dev->hif2) {
		/* GLO_CFG_EXT0 */
		mt76_set(dev, WF_WFDMA0_GLO_CFG_EXT0 + hif1_ofs,
			 WF_WFDMA0_GLO_CFG_EXT0_RX_WB_RXD |
			 WF_WFDMA0_GLO_CFG_EXT0_WED_MERGE_MODE);

		/* GLO_CFG_EXT1 */
		mt76_set(dev, WF_WFDMA0_GLO_CFG_EXT1 + hif1_ofs,
			 WF_WFDMA0_GLO_CFG_EXT1_TX_FCTRL_MODE);

		mt76_set(dev, MT_WFDMA_HOST_CONFIG,
			 MT_WFDMA_HOST_CONFIG_PDMA_BAND);
	}

	if (dev->hif2) {
		/* fix hardware limitation, pcie1's rx ring3 is not available
		 * so, redirect pcie0 rx ring3 interrupt to pcie1
		 */
		if (mtk_wed_device_active(&dev->mt76.mmio.wed) &&
		    dev->has_rro)
			mt76_set(dev, MT_WFDMA0_RX_INT_PCIE_SEL + hif1_ofs,
				 MT_WFDMA0_RX_INT_SEL_RING6);
		else
			mt76_set(dev, MT_WFDMA0_RX_INT_PCIE_SEL,
				 MT_WFDMA0_RX_INT_SEL_RING3);
	}

	mt7996_dma_start(dev, reset, true);
}

#ifdef CONFIG_NET_MEDIATEK_SOC_WED
int mt7996_dma_rro_init(struct mt7996_dev *dev)
{
	struct mt76_dev *mdev = &dev->mt76;
	u32 irq_mask;
	int ret;

	/* ind cmd */
	mdev->q_rx[MT_RXQ_RRO_IND].flags = MT_WED_RRO_Q_IND;
	mdev->q_rx[MT_RXQ_RRO_IND].wed = &mdev->mmio.wed;
	ret = mt76_queue_alloc(dev, &mdev->q_rx[MT_RXQ_RRO_IND],
			       MT_RXQ_ID(MT_RXQ_RRO_IND),
			       MT7996_RX_RING_SIZE,
			       0, MT_RXQ_RRO_IND_RING_BASE);
	if (ret)
		return ret;

	/* rx msdu page queue for band0 */
	mdev->q_rx[MT_RXQ_MSDU_PAGE_BAND0].flags =
		MT_WED_RRO_Q_MSDU_PG(0) | MT_QFLAG_WED_RRO_EN;
	mdev->q_rx[MT_RXQ_MSDU_PAGE_BAND0].wed = &mdev->mmio.wed;
	ret = mt76_queue_alloc(dev, &mdev->q_rx[MT_RXQ_MSDU_PAGE_BAND0],
			       MT_RXQ_ID(MT_RXQ_MSDU_PAGE_BAND0),
			       MT7996_RX_RING_SIZE,
			       MT7996_RX_MSDU_PAGE_SIZE,
			       MT_RXQ_RING_BASE(MT_RXQ_MSDU_PAGE_BAND0));
	if (ret)
		return ret;

	if (dev->dbdc_support) {
		/* rx msdu page queue for band1 */
		mdev->q_rx[MT_RXQ_MSDU_PAGE_BAND1].flags =
			MT_WED_RRO_Q_MSDU_PG(1) | MT_QFLAG_WED_RRO_EN;
		mdev->q_rx[MT_RXQ_MSDU_PAGE_BAND1].wed = &mdev->mmio.wed;
		ret = mt76_queue_alloc(dev, &mdev->q_rx[MT_RXQ_MSDU_PAGE_BAND1],
				       MT_RXQ_ID(MT_RXQ_MSDU_PAGE_BAND1),
				       MT7996_RX_RING_SIZE,
				       MT7996_RX_MSDU_PAGE_SIZE,
				       MT_RXQ_RING_BASE(MT_RXQ_MSDU_PAGE_BAND1));
		if (ret)
			return ret;
	}

	if (dev->tbtc_support) {
		/* rx msdu page queue for band2 */
		mdev->q_rx[MT_RXQ_MSDU_PAGE_BAND2].flags =
			MT_WED_RRO_Q_MSDU_PG(2) | MT_QFLAG_WED_RRO_EN;
		mdev->q_rx[MT_RXQ_MSDU_PAGE_BAND2].wed = &mdev->mmio.wed;
		ret = mt76_queue_alloc(dev, &mdev->q_rx[MT_RXQ_MSDU_PAGE_BAND2],
				       MT_RXQ_ID(MT_RXQ_MSDU_PAGE_BAND2),
				       MT7996_RX_RING_SIZE,
				       MT7996_RX_MSDU_PAGE_SIZE,
				       MT_RXQ_RING_BASE(MT_RXQ_MSDU_PAGE_BAND2));
		if (ret)
			return ret;
	}

	irq_mask = mdev->mmio.irqmask | MT_INT_RRO_RX_DONE |
		   MT_INT_TX_DONE_BAND2;
	mt76_wr(dev, MT_INT_MASK_CSR, irq_mask);
	mtk_wed_device_start_hw_rro(&mdev->mmio.wed, irq_mask, false);
	mt7996_irq_enable(dev, irq_mask);

	return 0;
}
#endif /* CONFIG_NET_MEDIATEK_SOC_WED */

int mt7996_dma_init(struct mt7996_dev *dev)
{
	struct mtk_wed_device *wed = &dev->mt76.mmio.wed;
	struct mtk_wed_device *wed_hif2 = &dev->mt76.mmio.wed_hif2;
	u32 rx_base;
	u32 hif1_ofs = 0;
	int ret;

	mt7996_dma_config(dev);

	mt76_dma_attach(&dev->mt76);

	if (dev->hif2)
		hif1_ofs = MT_WFDMA0_PCIE1(0) - MT_WFDMA0(0);

	mt7996_dma_disable(dev, true);

	/* init tx queue */
	ret = mt7996_init_tx_queues(&dev->phy,
				    MT_TXQ_ID(dev->mphy.band_idx),
				    MT7996_TX_RING_SIZE,
				    MT_TXQ_RING_BASE(0),
				    wed);
	if (ret)
		return ret;

	/* command to WM */
	ret = mt76_init_mcu_queue(&dev->mt76, MT_MCUQ_WM,
				  MT_MCUQ_ID(MT_MCUQ_WM),
				  MT7996_TX_MCU_RING_SIZE,
				  MT_MCUQ_RING_BASE(MT_MCUQ_WM));
	if (ret)
		return ret;

	/* command to WA */
	ret = mt76_init_mcu_queue(&dev->mt76, MT_MCUQ_WA,
				  MT_MCUQ_ID(MT_MCUQ_WA),
				  MT7996_TX_MCU_RING_SIZE,
				  MT_MCUQ_RING_BASE(MT_MCUQ_WA));
	if (ret)
		return ret;

	/* firmware download */
	ret = mt76_init_mcu_queue(&dev->mt76, MT_MCUQ_FWDL,
				  MT_MCUQ_ID(MT_MCUQ_FWDL),
				  MT7996_TX_FWDL_RING_SIZE,
				  MT_MCUQ_RING_BASE(MT_MCUQ_FWDL));
	if (ret)
		return ret;

	/* event from WM */
	ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_MCU],
			       MT_RXQ_ID(MT_RXQ_MCU),
			       MT7996_RX_MCU_RING_SIZE,
			       MT_RX_BUF_SIZE,
			       MT_RXQ_RING_BASE(MT_RXQ_MCU));
	if (ret)
		return ret;

	/* event from WA */
	ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_MCU_WA],
			       MT_RXQ_ID(MT_RXQ_MCU_WA),
			       MT7996_RX_MCU_RING_SIZE_WA,
			       MT_RX_BUF_SIZE,
			       MT_RXQ_RING_BASE(MT_RXQ_MCU_WA));
	if (ret)
		return ret;

	/* rx data queue for band0 and band1 */
	if (mtk_wed_device_active(wed) && mtk_wed_get_rx_capa(wed)) {
		dev->mt76.q_rx[MT_RXQ_MAIN].flags = MT_WED_Q_RX(0);
		dev->mt76.q_rx[MT_RXQ_MAIN].wed = wed;
	}

	ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_MAIN],
			       MT_RXQ_ID(MT_RXQ_MAIN),
			       MT7996_RX_RING_SIZE,
			       MT_RX_BUF_SIZE,
			       MT_RXQ_RING_BASE(MT_RXQ_MAIN));
	if (ret)
		return ret;

	/* tx free notify event from WA for band0 */
	if (mtk_wed_device_active(wed) && !dev->has_rro) {
		dev->mt76.q_rx[MT_RXQ_MAIN_WA].flags = MT_WED_Q_TXFREE;
		dev->mt76.q_rx[MT_RXQ_MAIN_WA].wed = wed;
	}

	ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_MAIN_WA],
			       MT_RXQ_ID(MT_RXQ_MAIN_WA),
			       MT7996_RX_MCU_RING_SIZE,
			       MT_RX_BUF_SIZE,
			       MT_RXQ_RING_BASE(MT_RXQ_MAIN_WA));
	if (ret)
		return ret;

	if (dev->tbtc_support || dev->mphy.band_idx == MT_BAND2) {
		/* rx data queue for band2 */
		rx_base = MT_RXQ_RING_BASE(MT_RXQ_BAND2) + hif1_ofs;
		ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_BAND2],
				       MT_RXQ_ID(MT_RXQ_BAND2),
				       MT7996_RX_RING_SIZE,
				       MT_RX_BUF_SIZE,
				       rx_base);
		if (ret)
			return ret;

		/* tx free notify event from WA for band2
		 * use pcie0's rx ring3, but, redirect pcie0 rx ring3 interrupt to pcie1
		 */
		if (mtk_wed_device_active(wed_hif2) && !dev->has_rro) {
			dev->mt76.q_rx[MT_RXQ_BAND2_WA].flags = MT_WED_Q_TXFREE;
			dev->mt76.q_rx[MT_RXQ_BAND2_WA].wed = wed_hif2;
		}

		ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_BAND2_WA],
				       MT_RXQ_ID(MT_RXQ_BAND2_WA),
				       MT7996_RX_MCU_RING_SIZE,
				       MT_RX_BUF_SIZE,
				       MT_RXQ_RING_BASE(MT_RXQ_BAND2_WA));
		if (ret)
			return ret;
	}

	if (mtk_wed_device_active(wed) && mtk_wed_get_rx_capa(wed) &&
	    dev->has_rro) {
		/* rx rro data queue for band0 */
		dev->mt76.q_rx[MT_RXQ_RRO_BAND0].flags =
			MT_WED_RRO_Q_DATA(0) | MT_QFLAG_WED_RRO_EN;
		dev->mt76.q_rx[MT_RXQ_RRO_BAND0].wed = wed;
		ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_RRO_BAND0],
				       MT_RXQ_ID(MT_RXQ_RRO_BAND0),
				       MT7996_RX_RING_SIZE,
				       MT7996_RX_BUF_SIZE,
				       MT_RXQ_RING_BASE(MT_RXQ_RRO_BAND0));
		if (ret)
			return ret;

		/* tx free notify event from WA for band0 */
		dev->mt76.q_rx[MT_RXQ_TXFREE_BAND0].flags = MT_WED_Q_TXFREE;
		dev->mt76.q_rx[MT_RXQ_TXFREE_BAND0].wed = wed;

		ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_TXFREE_BAND0],
				       MT_RXQ_ID(MT_RXQ_TXFREE_BAND0),
				       MT7996_RX_MCU_RING_SIZE,
				       MT7996_RX_BUF_SIZE,
				       MT_RXQ_RING_BASE(MT_RXQ_TXFREE_BAND0));
		if (ret)
			return ret;

		if (dev->tbtc_support || dev->mphy.band_idx == MT_BAND2) {
			/* rx rro data queue for band2 */
			dev->mt76.q_rx[MT_RXQ_RRO_BAND2].flags =
				MT_WED_RRO_Q_DATA(1) | MT_QFLAG_WED_RRO_EN;
			dev->mt76.q_rx[MT_RXQ_RRO_BAND2].wed = wed;
			ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_RRO_BAND2],
					       MT_RXQ_ID(MT_RXQ_RRO_BAND2),
					       MT7996_RX_RING_SIZE,
					       MT7996_RX_BUF_SIZE,
					       MT_RXQ_RING_BASE(MT_RXQ_RRO_BAND2) + hif1_ofs);
			if (ret)
				return ret;

			/* tx free notify event from MAC for band2 */
			if (mtk_wed_device_active(wed_hif2)) {
				dev->mt76.q_rx[MT_RXQ_TXFREE_BAND2].flags = MT_WED_Q_TXFREE;
				dev->mt76.q_rx[MT_RXQ_TXFREE_BAND2].wed = wed_hif2;
			}
			ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_TXFREE_BAND2],
					       MT_RXQ_ID(MT_RXQ_TXFREE_BAND2),
					       MT7996_RX_MCU_RING_SIZE,
					       MT7996_RX_BUF_SIZE,
					       MT_RXQ_RING_BASE(MT_RXQ_TXFREE_BAND2) + hif1_ofs);
			if (ret)
				return ret;
		}
	}

	ret = mt76_init_queues(dev, mt76_dma_rx_poll);
	if (ret < 0)
		return ret;

	netif_napi_add_tx(&dev->mt76.tx_napi_dev, &dev->mt76.tx_napi,
			  mt7996_poll_tx);
	napi_enable(&dev->mt76.tx_napi);

	mt7996_dma_enable(dev, false);

	return 0;
}

void mt7996_dma_reset(struct mt7996_dev *dev, bool force)
{
	struct mt76_phy *phy2 = dev->mt76.phys[MT_BAND1];
	struct mt76_phy *phy3 = dev->mt76.phys[MT_BAND2];
	u32 hif1_ofs = MT_WFDMA0_PCIE1(0) - MT_WFDMA0(0);
	int i;

	mt76_clear(dev, MT_WFDMA0_GLO_CFG,
		   MT_WFDMA0_GLO_CFG_TX_DMA_EN |
		   MT_WFDMA0_GLO_CFG_RX_DMA_EN);

	if (dev->hif2)
		mt76_clear(dev, MT_WFDMA0_GLO_CFG + hif1_ofs,
			   MT_WFDMA0_GLO_CFG_TX_DMA_EN |
			   MT_WFDMA0_GLO_CFG_RX_DMA_EN);

	usleep_range(1000, 2000);

	for (i = 0; i < __MT_TXQ_MAX; i++) {
		mt76_queue_tx_cleanup(dev, dev->mphy.q_tx[i], true);
		if (phy2)
			mt76_queue_tx_cleanup(dev, phy2->q_tx[i], true);
		if (phy3)
			mt76_queue_tx_cleanup(dev, phy3->q_tx[i], true);
	}

	for (i = 0; i < __MT_MCUQ_MAX; i++)
		mt76_queue_tx_cleanup(dev, dev->mt76.q_mcu[i], true);

	mt76_for_each_q_rx(&dev->mt76, i)
		mt76_queue_rx_cleanup(dev, &dev->mt76.q_rx[i]);

	mt76_tx_status_check(&dev->mt76, true);

	/* reset wfsys */
	if (force)
		mt7996_wfsys_reset(dev);

	mt7996_dma_disable(dev, force);

	/* reset hw queues */
	for (i = 0; i < __MT_TXQ_MAX; i++) {
		mt76_queue_reset(dev, dev->mphy.q_tx[i]);
		if (phy2)
			mt76_queue_reset(dev, phy2->q_tx[i]);
		if (phy3)
			mt76_queue_reset(dev, phy3->q_tx[i]);
	}

	for (i = 0; i < __MT_MCUQ_MAX; i++)
		mt76_queue_reset(dev, dev->mt76.q_mcu[i]);

	mt76_for_each_q_rx(&dev->mt76, i) {
		mt76_queue_reset(dev, &dev->mt76.q_rx[i]);
	}

	mt76_tx_status_check(&dev->mt76, true);

	mt76_for_each_q_rx(&dev->mt76, i)
		mt76_queue_rx_reset(dev, i);

	mt7996_dma_enable(dev, !force);
}

void mt7996_dma_cleanup(struct mt7996_dev *dev)
{
	mt7996_dma_disable(dev, true);

	mt76_dma_cleanup(&dev->mt76);
}
