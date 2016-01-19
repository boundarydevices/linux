/*
 * Copyright 2015 www.starterkit.ru <info@starterkit.ru>
 *
 * Based on:
 * Driver for Intersil|Techwell TW6869 based DVR cards
 * (c) 2011-12 liran <jli11@intersil.com> [Intersil|Techwell China]
 *
 * V4L2 PCI Skeleton Driver
 * Copyright 2014 Cisco Systems, Inc. and/or its affiliates.
 * All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "tw6869.h"

MODULE_DESCRIPTION("tw6869/65 media bridge driver");
MODULE_AUTHOR("starterkit <info@starterkit.ru>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.5.1");
MODULE_DEVICE_TABLE(pci, tw6869_pci_tbl);

static const struct pci_device_id tw6869_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_TECHWELL, PCI_DEVICE_ID_6869)},
	{PCI_DEVICE(PCI_VENDOR_ID_TECHWELL, PCI_DEVICE_ID_6865)},
	{ 0, }
};

static int tw6869_dummy_dma_locked(struct tw6869_dma *dma)
{
	tw_err(dma->dev, "DMA %u\n", dma->id);
	return 0;
}

static void tw6869_dummy_dma_srst(struct tw6869_dma *dma)
{
	tw_err(dma->dev, "DMA %u\n", dma->id);
}

static void tw6869_dummy_dma_ctrl(struct tw6869_dma *dma)
{
	tw_err(dma->dev, "DMA %u\n", dma->id);
}

static void tw6869_dummy_dma_cfg(struct tw6869_dma *dma)
{
	tw_err(dma->dev, "DMA %u\n", dma->id);
}

static void tw6869_dummy_dma_isr(struct tw6869_dma *dma)
{
	tw_err(dma->dev, "DMA %u\n", dma->id);
}

static void tw6869_delayed_dma_on(struct work_struct *work)
{
	struct tw6869_dma *dma =
		container_of(to_delayed_work(work), struct tw6869_dma, hw_on);
	unsigned long flags;

	spin_lock_irqsave(&dma->dev->rlock, flags);
	if (tw_dma_active(dma) && !dma->low_power) {
		if ((*dma->locked)(dma)) {
			tw_set(dma->dev, R32_DMA_CHANNEL_ENABLE, BIT(dma->id));
			tw_set(dma->dev, R32_DMA_CMD, BIT(31) | BIT(dma->id));
			dma->fld = 0;
			dma->pb = 0;
			if (!tw_dma_is_on(dma))
				tw_err(dma->dev, "DMA %u: failed ON\n", dma->id);
		} else { /* reschedule itself */
			mod_delayed_work(system_wq, &dma->hw_on, dma->delay);
		}
	}
	spin_unlock_irqrestore(&dma->dev->rlock, flags);
}

static void tw6869_dma_init(struct tw6869_dev *dev)
{
	struct tw6869_dma *dma;
	unsigned int id;

	for (id = 0; id < TW_ID_MAX; id++) {
		if (TW_VID & BIT(id)) {
			dma = &dev->vch[ID2CH(id)].dma;
			dma->reg[0] = R32_VDMA_P_ADDR(id);
			dma->reg[1] = R32_VDMA_B_ADDR(id);
			dma->reg[2] = R32_VDMA_F2_P_ADDR(id);
			dma->reg[3] = R32_VDMA_F2_B_ADDR(id);
			dma->delay = HZ / 10;
		} else {
			dma = &dev->ach[ID2CH(id)].dma;
			dma->reg[0] = R32_ADMA_P_ADDR(id);
			dma->reg[1] = R32_ADMA_B_ADDR(id);
			dma->reg[2] = 0x0;
			dma->reg[3] = 0x0;
			dma->delay = 0x0;
		}
		dma->id = id;
		dma->dev = dev;
		dma->locked = tw6869_dummy_dma_locked;
		dma->srst = tw6869_dummy_dma_srst;
		dma->ctrl = tw6869_dummy_dma_ctrl;
		dma->cfg = tw6869_dummy_dma_cfg;
		dma->isr = tw6869_dummy_dma_isr;

		dev->dma[id] = dma;
		spin_lock_init(&dma->lock);
		INIT_DELAYED_WORK(&dma->hw_on, tw6869_delayed_dma_on);
	}
}

static void tw6869_dma_reset(struct tw6869_dma *dma)
{
	if (tw_dma_active(dma) && !dma->low_power) {
		unsigned int is_on;

		/* Resets the video (audio) portion to its default state */
		(*dma->srst)(dma);

		tw_clear(dma->dev, R32_DMA_CMD, BIT(dma->id));
		tw_clear(dma->dev, R32_DMA_CHANNEL_ENABLE, BIT(dma->id));
		is_on = tw_dma_is_on(dma);

		if (++dma->err < TW_DMA_ERR_MAX) {
			mod_delayed_work(system_wq, &dma->hw_on, dma->delay);
			tw_dbg(dma->dev, "DMA %u\n", dma->id);
		} else {
			dma->is_on = is_on;
			tw_err(dma->dev, "DMA %u: forced OFF\n", dma->id);
		}
	}
}

static irqreturn_t tw6869_irq(int irq, void *dev_id)
{
	struct tw6869_dev *dev = dev_id;
	unsigned long ints = tw_read(dev, R32_INT_STATUS);

	if (ints) {
		unsigned int pbs, errs, pars, cmd, id;

		pbs = tw_read(dev, R32_PB_STATUS);
		errs = tw_read(dev, R32_FIFO_STATUS);
		pars = tw_read(dev, R32_VIDEO_PARSER_STATUS);
		cmd = tw_read(dev, R32_DMA_CMD);

		errs = (ints >> 24 | errs >> 24 | errs >> 16) & TW_VID;
		ints = (ints | errs) & cmd;

		for_each_set_bit(id, &ints, TW_ID_MAX) {
			struct tw6869_dma *dma = dev->dma[id];
			unsigned int err = (errs >> id) & 0x1;
			unsigned int fld = ((pbs >> 24) >> id) & 0x1;
			unsigned int pb = (pbs >> id) & 0x1;

			if (!err && dma->fld == fld && dma->pb == pb) {
				dma->err = 0;
				(*dma->isr)(dma);
			} else {
				spin_lock(&dev->rlock);
				tw6869_dma_reset(dma);
				spin_unlock(&dev->rlock);
			}
		}
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static void tw6869_reset(struct tw6869_dev *dev)
{
	/* Software Reset */
	tw_write(dev, R32_SYS_SOFT_RST, 0x01);
	tw_write(dev, R32_SYS_SOFT_RST, 0x0F);

	/* Reset Internal audio and video decoders */
	tw_write(dev, R8_AVSRST(0x0), 0x1F);
	tw_write(dev, R8_AVSRST(0x4), 0x1F);

	/* Reset and disable all DMA channels */
	tw_write(dev, R32_DMA_CMD, 0x0);
	tw_write(dev, R32_DMA_CHANNEL_ENABLE, 0x0);

	/* FIFO overflow and DMA pointer check */
	tw_write(dev, R32_DMA_CONFIG, 0xFFFFFF0C);

	/* Minimum time span for DMA interrupting host (default: 0x00098968) */
	tw_write(dev, R32_DMA_TIMER_INTERVAL, 0x00098968);

	/* DMA timeout (default: 0x140C8584) */
	tw_write(dev, R32_DMA_CHANNEL_TIMEOUT, 0x14FFFFFF);

	/* Frame mode DMA */
	tw_write(dev, R32_PHASE_REF, 0xAAAA144D);

	/* SK-TW6869: special mode */
	tw_write(dev, R8_VERTICAL_CONTROL1(0x0), 0xFF);
	tw_write(dev, R8_VERTICAL_CONTROL1(0x4), 0xFF);
	tw_write(dev, R8_MISC_CONTROL1(0x0), 0x56);
	tw_write(dev, R8_MISC_CONTROL1(0x4), 0x56);

	/* Audio DMA 4096 bytes, sampling frequency reference 48 kHz */
	tw_write(dev, R32_AUDIO_CONTROL1, 0x80000001 | (0x0A2C << 5));
	tw_write(dev, R32_AUDIO_CONTROL2, 0x0A2C2AAA);
}

static int tw6869_probe(struct pci_dev *pdev, const struct pci_device_id *pid)
{
	struct tw6869_dev *dev;
	int ret;

	/* Allocate a new instance */
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	pci_set_master(pdev);
	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&pdev->dev, "no suitable DMA available\n");
		return ret;
	}

	ret = pcim_iomap_regions(pdev, BIT(TW_MMIO_BAR), KBUILD_MODNAME);
	if (ret)
		return ret;

	dev->mmio = pcim_iomap_table(pdev)[TW_MMIO_BAR];
	dev->vch_max = (pid->device == PCI_DEVICE_ID_6865) ? 4 : TW_CH_MAX;
	/* SK-TW6869: only 4 audio inputs is available */
	dev->ach_max = (1) ? 4 : TW_CH_MAX;
	dev->pdev = pdev;
	spin_lock_init(&dev->rlock);

	tw6869_reset(dev);
	tw6869_dma_init(dev);

	ret = devm_request_irq(&pdev->dev, pdev->irq,
		tw6869_irq, IRQF_SHARED, KBUILD_MODNAME, dev);
	if (ret) {
		dev_err(&pdev->dev, "request_irq failed\n");
		return ret;
	}

	ret = tw6869_video_register(dev);
	if (ret)
		return ret;

	ret = tw6869_audio_register(dev);
	if (ret)
		dev_warn(&pdev->dev, "pls check CONFIG_SND_DYNAMIC_MINORS=y\n");

	dev_info(&pdev->dev, "driver loaded\n");
	return 0;
}

static void tw6869_remove(struct pci_dev *pdev)
{
	struct v4l2_device *v4l2_dev = pci_get_drvdata(pdev);
	struct tw6869_dev *dev =
		container_of(v4l2_dev, struct tw6869_dev, v4l2_dev);

	tw6869_audio_unregister(dev);
	tw6869_video_unregister(dev);
}

#ifdef CONFIG_PM
static int tw6869_suspend(struct pci_dev *pdev , pm_message_t state)
{
	struct v4l2_device *v4l2_dev = pci_get_drvdata(pdev);
	struct tw6869_dev *dev =
		container_of(v4l2_dev, struct tw6869_dev, v4l2_dev);
	unsigned long flags;
	unsigned int id;

	spin_lock_irqsave(&dev->rlock, flags);
	/* Reset and disable all DMA channels */
	tw_write(dev, R32_DMA_CMD, 0x0);
	tw_write(dev, R32_DMA_CHANNEL_ENABLE, 0x0);

	/* Power down A/V ADC */
	tw_write(dev, R8_ANALOG_PWR_DOWN(0x0), 0x1F);
	tw_write(dev, R8_ANALOG_PWR_DOWN(0x4), 0x1F);

	for (id = 0; id < TW_ID_MAX; id++)
		dev->dma[id]->low_power = 1;
	spin_unlock_irqrestore(&dev->rlock, flags);

	synchronize_irq(pdev->irq);

	for (id = 0; id < TW_ID_MAX; id++)
		cancel_delayed_work_sync(&dev->dma[id]->hw_on);

	pci_save_state(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));
	return 0;
}

static int tw6869_resume(struct pci_dev *pdev)
{
	struct v4l2_device *v4l2_dev = pci_get_drvdata(pdev);
	struct tw6869_dev *dev =
		container_of(v4l2_dev, struct tw6869_dev, v4l2_dev);
	unsigned long flags;
	unsigned int id;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	tw6869_reset(dev);
	msleep(200);

	for (id = 0; id < TW_ID_MAX; id++) {
		struct tw6869_dma *dma = dev->dma[id];

		spin_lock_irqsave(&dma->lock, flags);
		if (tw_dma_active(dma)) {
			(*dma->ctrl)(dma);
			tw_dma_set_addr(dma);
		}
		spin_unlock_irqrestore(&dma->lock, flags);

		spin_lock_irqsave(&dev->rlock, flags);
		if (tw_dma_active(dma)) {
			(*dma->cfg)(dma);
			tw_dma_enable(dma);
		}
		spin_unlock_irqrestore(&dev->rlock, flags);
	}
	return 0;
}
#endif /* CONFIG_PM */

static struct pci_driver tw6869_driver = {
	.name = KBUILD_MODNAME,
	.probe = tw6869_probe,
	.remove = tw6869_remove,
	.id_table = tw6869_pci_tbl,
#ifdef CONFIG_PM
	.suspend  = tw6869_suspend,
	.resume   = tw6869_resume,
#endif
};

module_pci_driver(tw6869_driver);
