// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 BayLibre SAS
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/of_reserved_mem.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>

#include "remoteproc_internal.h"

/* From MT8183 4.5 Vision Processor Unit (VPU).pdf datasheet */
#define SW_RST					(0x0000000C)
#define  SW_RST_OCD_HALT_ON_RST			BIT(12)
#define  SW_RST_IPU_D_RST			BIT(8)
#define  SW_RST_IPU_B_RST			BIT(4)
#define CORE_CTRL				(0x00000110)
#define  CORE_CTRL_PDEBUG_ENABLE		BIT(31)
#define	 CORE_CTRL_SRAM_64K_iMEM		(0x00 << 27)
#define	 CORE_CTRL_SRAM_96K_iMEM		(0x01 << 27)
#define	 CORE_CTRL_SRAM_128K_iMEM		(0x02 << 27)
#define	 CORE_CTRL_SRAM_192K_iMEM		(0x03 << 27)
#define	 CORE_CTRL_SRAM_256K_iMEM		(0x04 << 27)
#define  CORE_CTRL_PBCLK_ENABLE			BIT(26)
#define  CORE_CTRL_RUN_STALL			BIT(23)
#define  CORE_CTRL_STATE_VECTOR_SELECT		BIT(19)
#define  CORE_CTRL_PIF_GATED			BIT(17)
#define  CORE_CTRL_NMI				BIT(0)
#define CORE_CTL_XTENSA_INT			(0x00000118)
#define CORE_DEFAULT0				(0x0000013C)
#define  CORE_DEFAULT0_QOS_SWAP_0		(0x00 << 28)
#define  CORE_DEFAULT0_QOS_SWAP_1		(0x01 << 28)
#define  CORE_DEFAULT0_QOS_SWAP_2		(0x02 << 28)
#define  CORE_DEFAULT0_QOS_SWAP_3		(0x03 << 28)
#define  CORE_DEFAULT0_ARUSER_USE_IOMMU		(0x10 << 23)
#define  CORE_DEFAULT0_AWUSER_USE_IOMMU		(0x10 << 18)
#define CORE_DEFAULT1				(0x00000140)
#define  CORE_DEFAULT0_ARUSER_IDMA_USE_IOMMU	(0x10 << 0)
#define  CORE_DEFAULT0_AWUSER_IDMA_USE_IOMMU	(0x10 << 5)
#define CORE_DEFAULT2				(0x00000144)
#define CORE_DEFAULT2_DBG_EN			BIT(3)
#define CORE_DEFAULT2_NIDEN			BIT(2)
#define CORE_DEFAULT2_SPNIDEN			BIT(1)
#define CORE_DEFAULT2_SPIDEN			BIT(0)
#define CORE_XTENSA_ALTRESETVEC			(0x000001F8)

struct mtk_vpu_rproc {
	struct device *dev;
	struct rproc *rproc;

	void __iomem *base;
	int irq;
	struct clk *axi;
	struct clk *ipu;
	struct clk *jtag;

#ifdef CONFIG_MTK_APU_JTAG
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_default;
	struct pinctrl_state *pinctrl_jtag;
	bool jtag_enabled;
#endif
};

static u32 vpu_read32(struct mtk_vpu_rproc *vpu_rproc, u32 off)
{
	return readl(vpu_rproc->base + off);
}

static void vpu_write32(struct mtk_vpu_rproc *vpu_rproc, u32 off, u32 value)
{
	writel(value, vpu_rproc->base + off);
}

static int mtk_vpu_rproc_start(struct rproc *rproc)
{
	struct mtk_vpu_rproc *vpu_rproc = rproc->priv;
	u32 core_ctrl;

	vpu_write32(vpu_rproc, CORE_XTENSA_ALTRESETVEC, rproc->bootaddr);

	core_ctrl = vpu_read32(vpu_rproc, CORE_CTRL);
	core_ctrl |= CORE_CTRL_PDEBUG_ENABLE | CORE_CTRL_PBCLK_ENABLE |
		     CORE_CTRL_STATE_VECTOR_SELECT | CORE_CTRL_RUN_STALL |
		     CORE_CTRL_PIF_GATED;
	vpu_write32(vpu_rproc, CORE_CTRL, core_ctrl);

	vpu_write32(vpu_rproc, SW_RST, SW_RST_OCD_HALT_ON_RST |
				       SW_RST_IPU_B_RST | SW_RST_IPU_D_RST);
	ndelay(27);
	vpu_write32(vpu_rproc, SW_RST, 0);

	core_ctrl &= ~CORE_CTRL_PIF_GATED;
	vpu_write32(vpu_rproc, CORE_CTRL, core_ctrl);

	vpu_write32(vpu_rproc, CORE_DEFAULT0, CORE_DEFAULT0_AWUSER_USE_IOMMU |
					      CORE_DEFAULT0_ARUSER_USE_IOMMU |
					      CORE_DEFAULT0_QOS_SWAP_1);
	vpu_write32(vpu_rproc, CORE_DEFAULT1,
		    CORE_DEFAULT0_AWUSER_IDMA_USE_IOMMU |
		    CORE_DEFAULT0_ARUSER_IDMA_USE_IOMMU);

	core_ctrl &= ~CORE_CTRL_RUN_STALL;
	vpu_write32(vpu_rproc, CORE_CTRL, core_ctrl);

	return 0;
}

static int mtk_vpu_rproc_stop(struct rproc *rproc)
{
	struct mtk_vpu_rproc *vpu_rproc = rproc->priv;
	u32 core_ctrl;

	core_ctrl = vpu_read32(vpu_rproc, CORE_CTRL);
	vpu_write32(vpu_rproc, CORE_CTRL, core_ctrl | CORE_CTRL_RUN_STALL);

	return 0;
}

static void mtk_vpu_rproc_kick(struct rproc *rproc, int vqid)
{
	struct mtk_vpu_rproc *vpu_rproc = rproc->priv;

	vpu_write32(vpu_rproc, CORE_CTL_XTENSA_INT, 1 << vqid);
}

int mtk_vpu_elf_sanity_check(struct rproc *rproc, const struct firmware *fw)
{
	struct mtk_vpu_rproc *vpu_rproc = rproc->priv;
	const u8 *elf_data = fw->data;
	struct elf32_hdr *ehdr;
	struct elf32_phdr *phdr;
	int ret;
	int i;

	ret = rproc_elf_sanity_check(rproc, fw);
	if (ret)
		return ret;

	ehdr = (struct elf32_hdr *)elf_data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		/* Remove empty PT_LOAD section */
		if (phdr->p_type == PT_LOAD && !phdr->p_paddr)
			phdr->p_type = PT_NULL;
		/*
		 * Workaround: Currently, the CPU can't access to the APU
		 * local RAM. This removes the local RAM section from the
		 * firmware. Please note that may cause some issues.
		 */
		if (phdr->p_paddr == 0x7ff00000) {
			dev_warn_once(vpu_rproc->dev,
				      "Skipping the APU local RAM section\n");
			phdr->p_type = PT_NULL;
		}
	}

	return 0;
}

static const struct rproc_ops mtk_vpu_rproc_ops = {
	.start			= mtk_vpu_rproc_start,
	.stop			= mtk_vpu_rproc_stop,
	.kick			= mtk_vpu_rproc_kick,
	.load			= rproc_elf_load_segments,
	.parse_fw		= rproc_elf_load_rsc_table,
	.find_loaded_rsc_table	= rproc_elf_find_loaded_rsc_table,
	.sanity_check		= mtk_vpu_elf_sanity_check,
	.get_boot_addr		= rproc_elf_get_boot_addr,
};

static irqreturn_t mtk_vpu_rproc_callback(int irq, void *data)
{
	struct rproc *rproc = (struct rproc *)data;
	struct mtk_vpu_rproc *vpu_rproc = (struct mtk_vpu_rproc *)rproc->priv;

	vpu_write32(vpu_rproc, CORE_CTRL + 0x4, 1);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t handle_event(int irq, void *data)
{
	struct rproc *rproc = (struct rproc *)data;

	rproc_vq_interrupt(rproc, 0);
	rproc_vq_interrupt(rproc, 1);

	return IRQ_HANDLED;
}

#ifdef CONFIG_MTK_APU_JTAG

static int vpu_enable_jtag(struct mtk_vpu_rproc *vpu_rproc)
{
	int ret = 0;

	if (vpu_rproc->jtag_enabled)
		return -EINVAL;

	ret = pinctrl_select_state(vpu_rproc->pinctrl,
				   vpu_rproc->pinctrl_jtag);
	if (ret < 0) {
		dev_err(vpu_rproc->dev, "Failed to configure pins for JTAG\n");
		return ret;
	}

	vpu_write32(vpu_rproc, CORE_DEFAULT2,
		    CORE_DEFAULT2_SPNIDEN | CORE_DEFAULT2_SPIDEN |
		    CORE_DEFAULT2_NIDEN | CORE_DEFAULT2_DBG_EN);

	vpu_rproc->jtag_enabled = 1;

	return ret;
}

static int vpu_disable_jtag(struct mtk_vpu_rproc *vpu_rproc)
{
	int ret = 0;

	if (!vpu_rproc->jtag_enabled)
		return -EINVAL;

	vpu_write32(vpu_rproc, CORE_DEFAULT2, 0);

	ret = pinctrl_select_state(vpu_rproc->pinctrl,
				   vpu_rproc->pinctrl_default);
	if (ret < 0) {
		dev_err(vpu_rproc->dev,
			"Failed to configure pins to default\n");
		return ret;
	}

	vpu_rproc->jtag_enabled = 0;

	return ret;
}

static ssize_t rproc_jtag_read(struct file *filp, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	struct rproc *rproc = filp->private_data;
	struct mtk_vpu_rproc *vpu_rproc = (struct mtk_vpu_rproc *)rproc->priv;
	char *buf = vpu_rproc->jtag_enabled ? "enabled\n" : "disabled\n";

	return simple_read_from_buffer(userbuf, count, ppos, buf, strlen(buf));
}

static ssize_t rproc_jtag_write(struct file *filp, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct rproc *rproc = filp->private_data;
	struct mtk_vpu_rproc *vpu_rproc = (struct mtk_vpu_rproc *)rproc->priv;
	char buf[10];
	int ret;

	if (count < 1 || count > sizeof(buf))
		return -EINVAL;

	ret = copy_from_user(buf, user_buf, count);
	if (ret)
		return -EFAULT;

	/* remove end of line */
	if (buf[count - 1] == '\n')
		buf[count - 1] = '\0';

	if (!strncmp(buf, "1", count) || !strncmp(buf, "enabled", count))
		ret = vpu_enable_jtag(vpu_rproc);
	else if (!strncmp(buf, "0", count) || !strncmp(buf, "disabled", count))
		ret = vpu_disable_jtag(vpu_rproc);
	else
		return -EINVAL;

	return ret ? ret : count;
}

static const struct file_operations rproc_jtag_ops = {
	.read = rproc_jtag_read,
	.write = rproc_jtag_write,
	.open = simple_open,
};

static int vpu_jtag_probe(struct mtk_vpu_rproc *vpu_rproc)
{
	int ret;

	if (!vpu_rproc->rproc->dbg_dir)
		return -ENODEV;

	debugfs_create_file("jtag", 0600, vpu_rproc->rproc->dbg_dir,
			    vpu_rproc->rproc, &rproc_jtag_ops);

	vpu_rproc->pinctrl = devm_pinctrl_get(vpu_rproc->dev);
	if (IS_ERR(vpu_rproc->pinctrl)) {
		dev_warn(vpu_rproc->dev, "Failed to find JTAG pinctrl\n");
		return PTR_ERR(vpu_rproc->pinctrl);
	}

	vpu_rproc->pinctrl_default = pinctrl_lookup_state(vpu_rproc->pinctrl,
							PINCTRL_STATE_DEFAULT);
	if (IS_ERR(vpu_rproc->pinctrl_default))
		return PTR_ERR(vpu_rproc->pinctrl_default);

	vpu_rproc->pinctrl_jtag = pinctrl_lookup_state(vpu_rproc->pinctrl,
						       "jtag");
	if (IS_ERR(vpu_rproc->pinctrl_jtag))
		return PTR_ERR(vpu_rproc->pinctrl_jtag);

	ret = pinctrl_select_state(vpu_rproc->pinctrl,
				   vpu_rproc->pinctrl_default);
	if (ret < 0)
		return ret;

	return 0;
}
#endif /* CONFIG_MTK_APU_JTAG */

static int mtk_vpu_rproc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_vpu_rproc *vpu_rproc;
	struct rproc *rproc;
	struct resource *res;
	int ret;

	rproc = rproc_alloc(dev, "apu", &mtk_vpu_rproc_ops, NULL,
			    sizeof(*vpu_rproc));
	if (!rproc)
		return -ENOMEM;

	rproc->recovery_disabled = true;
	rproc->has_iommu = false;

	vpu_rproc = rproc->priv;
	vpu_rproc->rproc = rproc;
	vpu_rproc->dev = dev;

	platform_set_drvdata(pdev, rproc);

	rproc->domain = iommu_get_domain_for_dev(dev);
	if (!rproc->domain) {
		dev_err(dev, "Failed to get the IOMMU domain\n");
		ret = -EINVAL;
		goto free_rproc;
	}


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	vpu_rproc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(vpu_rproc->base)) {
		dev_err(&pdev->dev, "Failed to map mmio\n");
		ret = PTR_ERR(vpu_rproc->base);
		goto free_rproc;
	}

	vpu_rproc->irq = platform_get_irq(pdev, 0);
	if (vpu_rproc->irq < 0) {
		ret = vpu_rproc->irq;
		goto free_rproc;
	}

	ret = devm_request_threaded_irq(dev, vpu_rproc->irq,
					mtk_vpu_rproc_callback, handle_event,
					IRQF_SHARED | IRQF_ONESHOT,
					"mtk_vpu-remoteproc", rproc);
	if (ret) {
		dev_err(dev, "devm_request_threaded_irq error: %d\n", ret);
		goto free_rproc;
	}

	vpu_rproc->ipu = devm_clk_get(dev, "ipu");
	if (IS_ERR(vpu_rproc->ipu)) {
		dev_err(dev, "Failed to get ipu clock\n");
		ret = PTR_ERR(vpu_rproc->ipu);
		goto free_rproc;
	}

	ret = clk_prepare_enable(vpu_rproc->ipu);
	if (ret) {
		dev_err(dev, "Failed to enable ipu clock\n");
		goto free_rproc;
	}

	vpu_rproc->axi = devm_clk_get(dev, "axi");
	if (IS_ERR(vpu_rproc->axi)) {
		dev_err(dev, "Failed to get axi clock\n");
		ret = PTR_ERR(vpu_rproc->axi);
		goto clk_disable_ipu;
	}

	ret = clk_prepare_enable(vpu_rproc->axi);
	if (ret) {
		dev_err(dev, "Failed to enable axi clock\n");
		goto clk_disable_ipu;
	}

	vpu_rproc->jtag = devm_clk_get(vpu_rproc->dev, "jtag");
	if (IS_ERR(vpu_rproc->jtag)) {
		dev_err(vpu_rproc->dev, "Failed to get jtag clock\n");
		ret = PTR_ERR(vpu_rproc->jtag);
		goto clk_disable_axi;
	}

	ret = clk_prepare_enable(vpu_rproc->jtag);
	if (ret) {
		dev_err(vpu_rproc->dev, "Failed to enable jtag clock\n");
		goto clk_disable_axi;
	}

	ret = of_reserved_mem_device_init(dev);
	if (ret) {
		dev_err(dev, "device does not have specific CMA pool\n");
		goto clk_disable_jtag;
	}

	ret = rproc_add(rproc);
	if (ret) {
		dev_err(dev, "rproc_add failed: %d\n", ret);
		goto free_mem;
	}

#ifdef CONFIG_MTK_APU_JTAG
	ret = vpu_jtag_probe(vpu_rproc);
	if (ret)
		dev_warn(dev, "Failed to configure jtag\n");
#endif

	return 0;

free_mem:
	of_reserved_mem_device_release(dev);
clk_disable_jtag:
	clk_disable_unprepare(vpu_rproc->jtag);
clk_disable_axi:
	clk_disable_unprepare(vpu_rproc->axi);
clk_disable_ipu:
	clk_disable_unprepare(vpu_rproc->ipu);
free_rproc:
	rproc_free(rproc);

	return ret;
}

static int mtk_vpu_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_vpu_rproc *vpu_rproc = (struct mtk_vpu_rproc *)rproc->priv;
	struct device *dev = &pdev->dev;

	disable_irq(vpu_rproc->irq);

#ifdef CONFIG_MTK_APU_JTAG
	vpu_disable_jtag(vpu_rproc);
#endif
	rproc_del(rproc);
	of_reserved_mem_device_release(dev);
	clk_disable_unprepare(vpu_rproc->jtag);
	clk_disable_unprepare(vpu_rproc->axi);
	clk_disable_unprepare(vpu_rproc->ipu);
	rproc_free(rproc);

	return 0;
}

static const struct of_device_id mtk_vpu_rproc_of_match[] __maybe_unused = {
	{ .compatible = "mediatek,mt8183-apu", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_vpu_rproc_of_match);

static struct platform_driver mtk_vpu_rproc_driver = {
	.probe = mtk_vpu_rproc_probe,
	.remove = mtk_vpu_rproc_remove,
	.driver = {
		.name = "mtk_vpu-rproc",
		.of_match_table = of_match_ptr(mtk_vpu_rproc_of_match),
	},
};
module_platform_driver(mtk_vpu_rproc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Alexandre Bailon");
MODULE_DESCRIPTION("Mt8183 VPU Remote Processor control driver");
