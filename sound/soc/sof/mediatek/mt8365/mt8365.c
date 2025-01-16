// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Copyright (c) 2025 MediaTek Inc.
 *
 * Author: Aary Patil <aary.patil@mediatek.com>
 *
 * Hardware interface for audio DSP on mt8365
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/module.h>

#include <sound/sof.h>
#include <sound/sof/xtensa.h>
#include "../../ops.h"
#include "../../sof-of-dev.h"
#include "../../sof-audio.h"
#include "../adsp_helper.h"
#include "../mtk-adsp-common.h"
#include "mt8365.h"
#include "mt8365-clk.h"

static void mt8365_dsp_handle_reply(struct snd_sof_dev *sdev);
static void mt8365_dsp_handle_request(struct snd_sof_dev *sdev);
static bool firmware_boot;

static int mt8365_dsp_ipi_send(struct snd_sof_dev *sdev, uint32_t msg)
{
	sof_mailbox_write(sdev, sdev->debug_box.offset + MTK_ADSP_SRAM_REG_OP_CPU2DSP,
			  &msg, sizeof(msg));

	/* Trigger CPU->DSP IPI interrupt */
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RG_INT2CIRQ, CPU2DSP_IRQ, CPU2DSP_IRQ);

	return 0;
}

static void mt8365_dsp_ipi_recv(struct snd_sof_dev *sdev)
{
	uint32_t msg;

	if (firmware_boot) {
		sof_mailbox_read(sdev, sdev->debug_box.offset + MTK_ADSP_SRAM_REG_OP_DSP2CPU,
				 &msg, sizeof(msg));
	} else {
		msg = MTK_ADSP_IPC_OP_REQ;
		firmware_boot = true;
	}

	switch (msg) {
	case MTK_ADSP_IPC_OP_RSP:
		mt8365_dsp_handle_reply(sdev);
		break;
	case MTK_ADSP_IPC_OP_REQ:
		mt8365_dsp_handle_request(sdev);
		break;
	default:
		dev_err(sdev->dev, "wrong message from DSP 0x%x\n", msg);
		break;
	}
}

static irqreturn_t mt8365_dsp_ipi_irq(int irq, void *data)
{
	struct snd_sof_dev *sdev = data;

	/* Clear DSP->CPU wakeup interrupt */
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RG_INT2CIRQ, DSP2SPM_IRQ_B, DSP2SPM_IRQ_B);

	/* Clear DSP->CPU IPI interrupt */
	snd_sof_dsp_update_bits(sdev, DSP_REG_BAR, DSP_RG_INT2CIRQ, DSP2CPU_IRQ, 0);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t mt8365_dsp_ipi_isr(int irq, void *data)
{
	struct snd_sof_dev *sdev = data;

	mt8365_dsp_ipi_recv(sdev);

	return IRQ_HANDLED;
}

static int mt8365_get_mailbox_offset(struct snd_sof_dev *sdev)
{
	return MBOX_OFFSET;
}

static int mt8365_get_window_offset(struct snd_sof_dev *sdev, u32 id)
{
	return MBOX_OFFSET;
}

static int mt8365_send_msg(struct snd_sof_dev *sdev,
			   struct snd_sof_ipc_msg *msg)
{
	sof_mailbox_write(sdev, sdev->host_box.offset, msg->msg_data,
			  msg->msg_size);

	return mt8365_dsp_ipi_send(sdev, MTK_ADSP_IPC_OP_REQ);
}

static void mt8365_dsp_handle_reply(struct snd_sof_dev *sdev)
{
	unsigned long flags;

	spin_lock_irqsave(&sdev->ipc_lock, flags);
	snd_sof_ipc_process_reply(sdev, 0);
	spin_unlock_irqrestore(&sdev->ipc_lock, flags);
}

static void mt8365_dsp_handle_request(struct snd_sof_dev *sdev)
{
	u32 p; /* panic code */
	int ret;

	/* Read the message from the debug box. */
	sof_mailbox_read(sdev, sdev->debug_box.offset + 4,
			 &p, sizeof(p));

	/* Check to see if the message is a panic code 0x0dead*** */
	if ((p & SOF_IPC_PANIC_MAGIC_MASK) == SOF_IPC_PANIC_MAGIC) {
		snd_sof_dsp_panic(sdev, p, true);
	} else {
		snd_sof_ipc_msgs_rx(sdev);

		/* tell DSP cmd is done */
		ret = mt8365_dsp_ipi_send(sdev, MTK_ADSP_IPC_OP_RSP);
		if (ret)
			dev_err(sdev->dev, "request send ipc failed");
	}
}

static int platform_parse_resource(struct platform_device *pdev, void *data)
{
	struct resource *mmio;
	struct resource res;
	struct device_node *mem_region;
	struct device *dev = &pdev->dev;
	struct mtk_adsp_chip_info *adsp = data;
	int ret;

	mem_region = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!mem_region) {
		dev_err(dev, "no dma memory-region phandle\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(mem_region, 0, &res);
	of_node_put(mem_region);
	if (ret) {
		dev_err(dev, "of_address_to_resource dma failed\n");
		return ret;
	}

	dev_dbg(dev, "DMA %pR\n", &res);

	adsp->pa_shared_dram = (phys_addr_t)res.start;
	adsp->shared_size = resource_size(&res);
	if (adsp->pa_shared_dram & DRAM_REMAP_MASK) {
		dev_err(dev, "adsp shared dma memory(%#x) is not 4K-aligned\n",
			(u32)adsp->pa_shared_dram);
		return -EINVAL;
	}

	ret = of_reserved_mem_device_init(dev);
	if (ret) {
		dev_err(dev, "of_reserved_mem_device_init failed\n");
		return ret;
	}

	mem_region = of_parse_phandle(dev->of_node, "memory-region", 1);
	if (!mem_region) {
		dev_err(dev, "no memory-region sysmem phandle\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(mem_region, 0, &res);
	of_node_put(mem_region);
	if (ret) {
		dev_err(dev, "of_address_to_resource sysmem failed\n");
		return ret;
	}

	adsp->pa_dram = (phys_addr_t)res.start;
	adsp->dramsize = resource_size(&res);
	if (adsp->pa_dram & DRAM_REMAP_MASK) {
		dev_err(dev, "adsp memory(%#x) is not 4K-aligned\n",
			(u32)adsp->pa_dram);
		return -EINVAL;
	}

	if (adsp->dramsize < TOTAL_SIZE_SHARED_DRAM_FROM_TAIL) {
		dev_err(dev, "adsp memory(%#x) is not enough for share\n",
			adsp->dramsize);
		return -EINVAL;
	}

	dev_dbg(dev, "dram pbase=%pa, dramsize=%#x\n",
		&adsp->pa_dram, adsp->dramsize);

	/* Parse CFG base */
	mmio = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg");
	if (!mmio) {
		dev_err(dev, "no ADSP-CFG register resource\n");
		return -ENXIO;
	}
	/* remap for DSP register accessing */
	adsp->va_cfgreg = devm_ioremap_resource(dev, mmio);
	if (IS_ERR(adsp->va_cfgreg))
		return PTR_ERR(adsp->va_cfgreg);

	adsp->pa_cfgreg = (phys_addr_t)mmio->start;
	adsp->cfgregsize = resource_size(mmio);

	dev_dbg(dev, "cfgreg-vbase=%p, cfgregsize=%#x\n",
		adsp->va_cfgreg, adsp->cfgregsize);

	/* Parse SRAM */
	mem_region = of_parse_phandle(dev->of_node, "memory-region", 2);
	if (!mem_region) {
		dev_err(dev, "no sram memory-region phandle\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(mem_region, 0, &res);
	of_node_put(mem_region);
	if (ret) {
		dev_err(dev, "of_address_to_resource sram failed\n");
		return ret;
	}

	adsp->pa_sram = (phys_addr_t)res.start;
	adsp->sramsize = resource_size(&res);

	dev_dbg(dev, "sram pbase=%pa,%#x\n", &adsp->pa_sram, adsp->sramsize);

	return ret;
}

static int adsp_sram_power_on(struct device *dev, bool on)
{
	void __iomem *va_dspsysreg;
	u32 srampool_con;

	va_dspsysreg = ioremap(ADSP_SRAM_POOL_CON, 0x4);
	if (!va_dspsysreg) {
		dev_err(dev, "failed to ioremap sram pool base %#x\n",
			ADSP_SRAM_POOL_CON);
		return -ENOMEM;
	}

	srampool_con = readl(va_dspsysreg);
	if (on)
		writel(srampool_con & ~DSP_SRAM_POOL_PD_MASK, va_dspsysreg);
	else
		writel(srampool_con | DSP_SRAM_POOL_PD_MASK, va_dspsysreg);

	iounmap(va_dspsysreg);
	return 0;
}

/*  Init the basic DSP DRAM address */
static int adsp_memory_remap_init(struct device *dev, struct mtk_adsp_chip_info *adsp)
{
	void __iomem *vaddr_emi_map;
	int offset;

	if (!adsp)
		return -ENXIO;

	vaddr_emi_map = devm_ioremap(dev, DSP_EMI_MAP_ADDR, 0x4);
	if (!vaddr_emi_map) {
		dev_err(dev, "failed to ioremap emi map base %#x\n",
			DSP_EMI_MAP_ADDR);
		return -ENOMEM;
	}

	offset = adsp->pa_dram - DRAM_PHYS_BASE_FROM_DSP_VIEW;
	adsp->dram_offset = offset;
	offset >>= DRAM_REMAP_SHIFT;
	dev_dbg(dev, "adsp->pa_dram %pa, offset %#x\n", &adsp->pa_dram, offset);
	writel(offset, vaddr_emi_map);
	if (offset != readl(vaddr_emi_map)) {
		dev_err(dev, "write emi map fail : %#x\n", readl(vaddr_emi_map));
		return -EIO;
	}

	return 0;
}

static int adsp_shared_base_ioremap(struct platform_device *pdev, void *data)
{
	struct device *dev = &pdev->dev;
	struct mtk_adsp_chip_info *adsp = data;

	/* remap shared-dram base to be non-cachable */
	adsp->shared_dram = devm_ioremap(dev, adsp->pa_shared_dram,
					 adsp->shared_size);
	if (!adsp->shared_dram) {
		dev_err(dev, "failed to ioremap base %pa size %#x\n",
			adsp->shared_dram, adsp->shared_size);
		return -ENOMEM;
	}

	dev_dbg(dev, "shared-dram vbase=%p, phy addr :%pa,  size=%#x\n",
		adsp->shared_dram, &adsp->pa_shared_dram, adsp->shared_size);

	return 0;
}

static int mt8365_run(struct snd_sof_dev *sdev)
{
	u32 adsp_bootup_addr;

	adsp_bootup_addr = SRAM_PHYS_BASE_FROM_DSP_VIEW;
	dev_dbg(sdev->dev, "HIFIxDSP boot from base : 0x%08X\n", adsp_bootup_addr);
	sof_hifixdsp_boot_sequence(sdev, adsp_bootup_addr);

	return 0;
}

static int mt8365_dsp_probe(struct snd_sof_dev *sdev)
{
	struct platform_device *pdev = container_of(sdev->dev, struct platform_device, dev);
	struct adsp_priv *priv;
	int ret, irq;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	sdev->pdata->hw_pdata = priv;
	priv->dev = sdev->dev;
	priv->sdev = sdev;

	priv->adsp = devm_kzalloc(&pdev->dev, sizeof(struct mtk_adsp_chip_info), GFP_KERNEL);
	if (!priv->adsp)
		return -ENOMEM;

	ret = platform_parse_resource(pdev, priv->adsp);
	if (ret)
		return ret;

	ret = mt8365_adsp_init_clock(sdev);
	if (ret) {
		dev_err(sdev->dev, "mt8365_adsp_init_clock failed\n");
		return -EINVAL;
	}

	ret = adsp_clock_on(sdev);
	if (ret) {
		dev_err(sdev->dev, "adsp_clock_on fail!\n");
		return -EINVAL;
	}

	ret = adsp_sram_power_on(sdev->dev, true);
	if (ret) {
		dev_err(sdev->dev, "adsp_sram_power_on fail!\n");
		goto exit_clk_disable;
	}

	ret = adsp_memory_remap_init(&pdev->dev, priv->adsp);
	if (ret) {
		dev_err(sdev->dev, "adsp_memory_remap_init fail!\n");
		goto err_adsp_sram_power_off;
	}

	sdev->bar[SOF_FW_BLK_TYPE_IRAM] = devm_ioremap(sdev->dev,
						       priv->adsp->pa_sram,
						       priv->adsp->sramsize);
	if (!sdev->bar[SOF_FW_BLK_TYPE_IRAM]) {
		dev_err(sdev->dev, "failed to ioremap base %pa size %#x\n",
			&priv->adsp->pa_sram, priv->adsp->sramsize);
		ret = -EINVAL;
		goto err_adsp_sram_power_off;
	}

	priv->adsp->va_sram = sdev->bar[SOF_FW_BLK_TYPE_IRAM];

	sdev->bar[SOF_FW_BLK_TYPE_SRAM] = devm_ioremap(sdev->dev,
						       priv->adsp->pa_dram,
						       priv->adsp->dramsize);
	if (!sdev->bar[SOF_FW_BLK_TYPE_SRAM]) {
		dev_err(sdev->dev, "failed to ioremap base %pa size %#x\n",
			&priv->adsp->pa_dram, priv->adsp->dramsize);
		ret = -EINVAL;
		goto err_adsp_sram_power_off;
	}
	priv->adsp->va_dram = sdev->bar[SOF_FW_BLK_TYPE_SRAM];

	ret = adsp_shared_base_ioremap(pdev, priv->adsp);
	if (ret) {
		dev_err(sdev->dev, "adsp_shared_base_ioremap fail!\n");
		goto err_adsp_sram_power_off;
	}

	sdev->bar[DSP_REG_BAR] = priv->adsp->va_cfgreg;

	sdev->mmio_bar = SOF_FW_BLK_TYPE_SRAM;
	sdev->mailbox_bar = SOF_FW_BLK_TYPE_SRAM;

	/* set default mailbox offset for FW ready message */
	sdev->dsp_box.offset = mt8365_get_mailbox_offset(sdev);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(sdev->dev, irq, mt8365_dsp_ipi_irq,
					mt8365_dsp_ipi_isr, IRQF_TRIGGER_NONE,
					dev_name(sdev->dev), sdev);
	if (ret < 0)
		return ret;

	return 0;

exit_pdev_unregister:
	platform_device_unregister(priv->ipc_dev);
err_adsp_sram_power_off:
	adsp_sram_power_on(&pdev->dev, false);
exit_clk_disable:
	adsp_clock_off(sdev);

	return ret;
}

static int mt8365_dsp_shutdown(struct snd_sof_dev *sdev)
{
	return snd_sof_suspend(sdev->dev);
}

static int mt8365_dsp_remove(struct snd_sof_dev *sdev)
{
	struct platform_device *pdev = container_of(sdev->dev, struct platform_device, dev);
	struct adsp_priv *priv = sdev->pdata->hw_pdata;

	platform_device_unregister(priv->ipc_dev);
	adsp_sram_power_on(&pdev->dev, false);
	adsp_clock_off(sdev);

	return 0;
}

static int mt8365_dsp_suspend(struct snd_sof_dev *sdev, u32 target_state)
{
	struct platform_device *pdev = container_of(sdev->dev, struct platform_device, dev);
	int ret;

	/* reset dsp */
	sof_hifixdsp_shutdown(sdev);

	/* power down adsp sram */
	ret = adsp_sram_power_on(&pdev->dev, false);
	if (ret) {
		dev_err(sdev->dev, "adsp_sram_power_off fail!\n");
		return ret;
	}

	/* turn off adsp clock */
	return adsp_clock_off(sdev);
}

static int mt8365_dsp_resume(struct snd_sof_dev *sdev)
{
	int ret;

	/* turn on adsp clock */
	ret = adsp_clock_on(sdev);
	if (ret) {
		dev_err(sdev->dev, "adsp_clock_on fail!\n");
		return ret;
	}

	/* power on adsp sram */
	ret = adsp_sram_power_on(sdev->dev, true);
	if (ret)
		dev_err(sdev->dev, "adsp_sram_power_on fail!\n");

	return ret;
}

/* on mt8365 there is 1 to 1 match between type and BAR idx */
static int mt8365_get_bar_index(struct snd_sof_dev *sdev, u32 type)
{
	return type;
}

static int mt8365_pcm_hw_params(struct snd_sof_dev *sdev,
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_sof_platform_stream_params *platform_params)
{
	platform_params->cont_update_posn = 1;

	return 0;
}

static snd_pcm_uframes_t mt8365_pcm_pointer(struct snd_sof_dev *sdev,
					    struct snd_pcm_substream *substream)
{
	int ret;
	snd_pcm_uframes_t pos;
	struct snd_sof_pcm *spcm;
	struct sof_ipc_stream_posn posn;
	struct snd_sof_pcm_stream *stream;
	struct snd_soc_component *scomp = sdev->component;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);

	spcm = snd_sof_find_spcm_dai(scomp, rtd);
	if (!spcm) {
		dev_warn_ratelimited(sdev->dev, "warn: can't find PCM with DAI ID %d\n",
				     rtd->dai_link->id);
		return 0;
	}

	stream = &spcm->stream[substream->stream];
	ret = snd_sof_ipc_msg_data(sdev, stream, &posn, sizeof(posn));
	if (ret < 0) {
		dev_warn(sdev->dev, "failed to read stream position: %d\n", ret);
		return 0;
	}

	memcpy(&stream->posn, &posn, sizeof(posn));
	pos = spcm->stream[substream->stream].posn.host_posn;
	pos = bytes_to_frames(substream->runtime, pos);

	return pos;
}

static void mt8365_adsp_dump(struct snd_sof_dev *sdev, u32 flags)
{
	u32 dbg_pc, dbg_data, dbg_bus0, dbg_bus1, dbg_inst;
	u32 dbg_ls0stat, dbg_ls1stat, faultbus, faultinfo, swrest;

	/* dump debug registers */
	dbg_pc = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_PDEBUGPC);
	dbg_data = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_PDEBUGDATA);
	dbg_bus0 = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_PDEBUGBUS0);
	dbg_bus1 = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_PDEBUGBUS1);
	dbg_inst = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_PDEBUGINST);
	dbg_ls0stat = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_PDEBUGLS0STAT);
	dbg_ls1stat = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_PDEBUGLS1STAT);
	faultbus = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_PFAULTBUS);
	faultinfo = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_PFAULTINFO);
	swrest = snd_sof_dsp_read(sdev, DSP_REG_BAR, DSP_RESET_SW);

	dev_info(sdev->dev, "adsp dump : pc %#x, data %#x, bus0 %#x, bus1 %#x, swrest %#x",
		 dbg_pc, dbg_data, dbg_bus0, dbg_bus1, swrest);
	dev_info(sdev->dev, "dbg_inst %#x, ls0stat %#x, ls1stat %#x, faultbus %#x, faultinfo %#x",
		 dbg_inst, dbg_ls0stat, dbg_ls1stat, faultbus, faultinfo);

	mtk_adsp_dump(sdev, flags);
}

static struct snd_soc_dai_driver mt8365_dai[] = {
{
	.name = "SOF_DL1",
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
	},
},
{
	.name = "SOF_DL2",
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
	},
},
{
	.name = "SOF_AWB",
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
	},
},
{
	.name = "SOF_VUL",
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
	},
},
};

/* mt8365 ops */
static struct snd_sof_dsp_ops sof_mt8365_ops = {
	/* probe and remove */
	.probe		= mt8365_dsp_probe,
	.remove		= mt8365_dsp_remove,
	.shutdown	= mt8365_dsp_shutdown,

	/* DSP core boot */
	.run		= mt8365_run,

	/* Block IO */
	.block_read	= sof_block_read,
	.block_write	= sof_block_write,

	/* Mailbox IO */
	.mailbox_read	= sof_mailbox_read,
	.mailbox_write	= sof_mailbox_write,

	/* Register IO */
	.write		= sof_io_write,
	.read		= sof_io_read,
	.write64	= sof_io_write64,
	.read64		= sof_io_read64,

	/* ipc */
	.send_msg		= mt8365_send_msg,
	.get_mailbox_offset	= mt8365_get_mailbox_offset,
	.get_window_offset	= mt8365_get_window_offset,
	.ipc_msg_data		= sof_ipc_msg_data,
	.set_stream_data_offset = sof_set_stream_data_offset,

	/* misc */
	.get_bar_index	= mt8365_get_bar_index,

	/* stream callbacks */
	.pcm_open	= sof_stream_pcm_open,
	.pcm_hw_params	= mt8365_pcm_hw_params,
	.pcm_pointer	= mt8365_pcm_pointer,
	.pcm_close	= sof_stream_pcm_close,

	/* firmware loading */
	.load_firmware	= snd_sof_load_firmware_memcpy,

	/* Firmware ops */
	.dsp_arch_ops = &sof_xtensa_arch_ops,

	/* Debug information */
	.dbg_dump = mt8365_adsp_dump,
	.debugfs_add_region_item = snd_sof_debugfs_add_region_item_iomem,

	/* DAI drivers */
	.drv = mt8365_dai,
	.num_drv = ARRAY_SIZE(mt8365_dai),

	/* PM */
	.suspend	= mt8365_dsp_suspend,
	.resume		= mt8365_dsp_resume,

	/* ALSA HW info flags */
	.hw_info =	SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_NO_PERIOD_WAKEUP,
};

static struct snd_sof_of_mach sof_mt8365_machs[] = {
	{
		.compatible = "mediatek,mt8365",
		.sof_tplg_filename = "sof-mt8365.tplg"
	}, {
		/* sentinel */
	}
};

static const struct sof_dev_desc sof_of_mt8365_desc = {
	.of_machines = sof_mt8365_machs,
	.ipc_supported_mask	= BIT(SOF_IPC),
	.ipc_default		= SOF_IPC,
	.default_fw_path = {
		[SOF_IPC] = "mediatek/sof",
	},
	.default_tplg_path = {
		[SOF_IPC] = "mediatek/sof-tplg",
	},
	.default_fw_filename = {
		[SOF_IPC] = "sof-mt8365.ri",
	},
	.nocodec_tplg_filename = "sof-mt8365-nocodec.tplg",
	.ops = &sof_mt8365_ops,
	.ipc_timeout = 1000,
};

static const struct of_device_id sof_of_mt8365_ids[] = {
	{ .compatible = "mediatek,mt8365-dsp", .data = &sof_of_mt8365_desc},
	{ }
};
MODULE_DEVICE_TABLE(of, sof_of_mt8365_ids);

/* DT driver definition */
static struct platform_driver snd_sof_of_mt8365_driver = {
	.probe = sof_of_probe,
	.remove = sof_of_remove,
	.shutdown = sof_of_shutdown,
	.driver = {
	.name = "sof-audio-of-mt8365",
		.pm = &sof_of_pm,
		.of_match_table = sof_of_mt8365_ids,
	},
};
module_platform_driver(snd_sof_of_mt8365_driver);

MODULE_IMPORT_NS(SND_SOC_SOF_XTENSA);
MODULE_IMPORT_NS(SND_SOC_SOF_MTK_COMMON);
MODULE_LICENSE("Dual BSD/GPL");
