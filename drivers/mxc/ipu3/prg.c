/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ipu-v3-prg.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx6q-iomuxc-gpr.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "prg-regs.h"

#define PRG_CHAN_NUM	3

struct prg_chan {
	unsigned int pre_num;
	struct mutex mutex;	/* for in_use */
	bool in_use;
};

struct ipu_prg_data {
	unsigned int id;
	void __iomem *base;
	unsigned long memory;
	struct clk *axi_clk;
	struct clk *apb_clk;
	struct list_head list;
	struct device *dev;
	struct prg_chan chan[PRG_CHAN_NUM];
	struct regmap *regmap;
	spinlock_t lock;
};

static LIST_HEAD(prg_list);
static DEFINE_MUTEX(prg_lock);

static inline void prg_write(struct ipu_prg_data *prg,
			u32 value, unsigned int offset)
{
	writel(value, prg->base + offset);
}

static inline u32 prg_read(struct ipu_prg_data *prg, unsigned offset)
{
	return readl(prg->base + offset);
}

static struct ipu_prg_data *get_prg(unsigned int ipu_id)
{
	struct ipu_prg_data *prg;

	mutex_lock(&prg_lock);
	list_for_each_entry(prg, &prg_list, list) {
		if (prg->id == ipu_id) {
			mutex_unlock(&prg_lock);
			return prg;
		}
	}
	mutex_unlock(&prg_lock);

	return NULL;
}

static int alloc_prg_chan(struct ipu_prg_data *prg, unsigned int pre_num)
{
	u32 mask, shift;
	int i;

	if (!prg)
		return -EINVAL;

	if ((prg->id == 0 && pre_num == 0) ||
	    (prg->id == 1 && pre_num == 3)) {
		mutex_lock(&prg->chan[0].mutex);
		if (!prg->chan[0].in_use) {
			prg->chan[0].in_use = true;
			prg->chan[0].pre_num = pre_num;
			mutex_unlock(&prg->chan[0].mutex);
			return 0;
		}
		mutex_unlock(&prg->chan[0].mutex);
		return -EBUSY;
	}

	for (i = 1; i < PRG_CHAN_NUM; i++) {
		mutex_lock(&prg->chan[i].mutex);
		if (!prg->chan[i].in_use) {
			prg->chan[i].in_use = true;
			prg->chan[i].pre_num = pre_num;

			if (pre_num == 1) {
				mask = IMX6Q_GPR5_PRE_PRG_SEL0_MASK;
				shift = IMX6Q_GPR5_PRE_PRG_SEL0_SHIFT;
			} else {
				mask = IMX6Q_GPR5_PRE_PRG_SEL1_MASK;
				shift = IMX6Q_GPR5_PRE_PRG_SEL1_SHIFT;
			}

			mutex_lock(&prg_lock);
			regmap_update_bits(prg->regmap, IOMUXC_GPR5, mask,
				((i - 1) + (prg->id << 1)) << shift);
			mutex_unlock(&prg_lock);
			mutex_unlock(&prg->chan[i].mutex);
			return i;
		}
		mutex_unlock(&prg->chan[i].mutex);
	}
	return -EBUSY;
}

static inline int get_prg_chan(struct ipu_prg_data *prg, unsigned int pre_num)
{
	int i;

	if (!prg)
		return -EINVAL;

	for (i = 0; i < PRG_CHAN_NUM; i++) {
		mutex_lock(&prg->chan[i].mutex);
		if (prg->chan[i].in_use &&
		    prg->chan[i].pre_num == pre_num) {
			mutex_unlock(&prg->chan[i].mutex);
			return i;
		}
		mutex_unlock(&prg->chan[i].mutex);
	}
	return -ENOENT;
}

int ipu_prg_config(struct ipu_prg_config *config)
{
	struct ipu_prg_data *prg = get_prg(config->id);
	struct ipu_soc *ipu = ipu_get_soc(config->id);
	int prg_ch, axi_id;
	u32 reg;

	if (!prg || config->crop_line > 3 || !ipu)
		return -EINVAL;

	if (config->height & ~IPU_PR_CH_HEIGHT_MASK)
		return -EINVAL;

	prg_ch = alloc_prg_chan(prg, config->pre_num);
	if (prg_ch < 0)
		return prg_ch;

	axi_id = ipu_ch_param_get_axi_id(ipu, config->ipu_ch, IPU_INPUT_BUFFER);

	clk_prepare_enable(prg->axi_clk);
	clk_prepare_enable(prg->apb_clk);

	spin_lock(&prg->lock);
	/* clear all load enable to impact other channels */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg &= ~IPU_PR_CTRL_CH_CNT_LOAD_EN_MASK;
	prg_write(prg, reg, IPU_PR_CTRL);

	/* counter load enable */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg |= IPU_PR_CTRL_CH_CNT_LOAD_EN(prg_ch);
	prg_write(prg, reg, IPU_PR_CTRL);

	/* AXI ID */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg &= ~IPU_PR_CTRL_SOFT_CH_ARID_MASK(prg_ch);
	reg |= IPU_PR_CTRL_SOFT_CH_ARID(prg_ch, axi_id);
	prg_write(prg, reg, IPU_PR_CTRL);

	/* so */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg &= ~IPU_PR_CTRL_CH_SO_MASK(prg_ch);
	reg |= IPU_PR_CTRL_CH_SO(prg_ch, config->so);
	prg_write(prg, reg, IPU_PR_CTRL);

	/* vflip */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg &= ~IPU_PR_CTRL_CH_VFLIP_MASK(prg_ch);
	reg |= IPU_PR_CTRL_CH_VFLIP(prg_ch, config->vflip);
	prg_write(prg, reg, IPU_PR_CTRL);

	/* block mode */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg &= ~IPU_PR_CTRL_CH_BLOCK_MODE_MASK(prg_ch);
	reg |= IPU_PR_CTRL_CH_BLOCK_MODE(prg_ch, config->block_mode);
	prg_write(prg, reg, IPU_PR_CTRL);

	/* disable bypass */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg &= ~IPU_PR_CTRL_CH_BYPASS(prg_ch);
	prg_write(prg, reg, IPU_PR_CTRL);

	/* stride */
	reg = prg_read(prg, IPU_PR_STRIDE(prg_ch));
	reg &= ~IPU_PR_STRIDE_MASK;
	reg |= config->stride - 1;
	prg_write(prg, reg, IPU_PR_STRIDE(prg_ch));

	/* ilo */
	reg = prg_read(prg, IPU_PR_CH_ILO(prg_ch));
	reg &= ~IPU_PR_CH_ILO_MASK;
	reg |= IPU_PR_CH_ILO_NUM(config->ilo);
	prg_write(prg, reg, IPU_PR_CH_ILO(prg_ch));

	/* height */
	reg = prg_read(prg, IPU_PR_CH_HEIGHT(prg_ch));
	reg &= ~IPU_PR_CH_HEIGHT_MASK;
	reg |= IPU_PR_CH_HEIGHT_NUM(config->height);
	prg_write(prg, reg, IPU_PR_CH_HEIGHT(prg_ch));

	/* ipu height */
	reg = prg_read(prg, IPU_PR_CH_HEIGHT(prg_ch));
	reg &= ~IPU_PR_CH_IPU_HEIGHT_MASK;
	reg |= IPU_PR_CH_IPU_HEIGHT_NUM(config->ipu_height);
	prg_write(prg, reg, IPU_PR_CH_HEIGHT(prg_ch));

	/* crop */
	reg = prg_read(prg, IPU_PR_CROP_LINE);
	reg &= ~IPU_PR_CROP_LINE_MASK(prg_ch);
	reg |= IPU_PR_CROP_LINE_NUM(prg_ch, config->crop_line);
	prg_write(prg, reg, IPU_PR_CROP_LINE);

	/* buffer address */
	reg = prg_read(prg, IPU_PR_CH_BADDR(prg_ch));
	reg &= ~IPU_PR_CH_BADDR_MASK;
	reg |= config->baddr;
	prg_write(prg, reg, IPU_PR_CH_BADDR(prg_ch));

	/* offset */
	reg = prg_read(prg, IPU_PR_CH_OFFSET(prg_ch));
	reg &= ~IPU_PR_CH_OFFSET_MASK;
	reg |= config->offset;
	prg_write(prg, reg, IPU_PR_CH_OFFSET(prg_ch));

	/* threshold */
	reg = prg_read(prg, IPU_PR_ADDR_THD);
	reg &= ~IPU_PR_ADDR_THD_MASK;
	reg |= prg->memory;
	prg_write(prg, reg, IPU_PR_ADDR_THD);

	/* shadow enable */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg |= IPU_PR_CTRL_SHADOW_EN;
	prg_write(prg, reg, IPU_PR_CTRL);

	/* register update */
	reg = prg_read(prg, IPU_PR_REG_UPDATE);
	reg |= IPU_PR_REG_UPDATE_EN;
	prg_write(prg, reg, IPU_PR_REG_UPDATE);
	spin_unlock(&prg->lock);

	clk_disable_unprepare(prg->apb_clk);

	return 0;
}
EXPORT_SYMBOL(ipu_prg_config);

int ipu_prg_disable(unsigned int ipu_id, unsigned int pre_num)
{
	struct ipu_prg_data *prg = get_prg(ipu_id);
	int prg_ch;
	u32 reg;

	if (!prg)
		return -EINVAL;

	prg_ch = get_prg_chan(prg, pre_num);
	if (prg_ch < 0)
		return prg_ch;

	clk_prepare_enable(prg->apb_clk);

	spin_lock(&prg->lock);
	/* clear all load enable to impact other channels */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg &= ~IPU_PR_CTRL_CH_CNT_LOAD_EN_MASK;
	prg_write(prg, reg, IPU_PR_CTRL);

	/* counter load enable */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg |= IPU_PR_CTRL_CH_CNT_LOAD_EN(prg_ch);
	prg_write(prg, reg, IPU_PR_CTRL);

	/* enable bypass */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg |= IPU_PR_CTRL_CH_BYPASS(prg_ch);
	prg_write(prg, reg, IPU_PR_CTRL);

	/* shadow enable */
	reg = prg_read(prg, IPU_PR_CTRL);
	reg |= IPU_PR_CTRL_SHADOW_EN;
	prg_write(prg, reg, IPU_PR_CTRL);

	/* register update */
	reg = prg_read(prg, IPU_PR_REG_UPDATE);
	reg |= IPU_PR_REG_UPDATE_EN;
	prg_write(prg, reg, IPU_PR_REG_UPDATE);
	spin_unlock(&prg->lock);

	clk_disable_unprepare(prg->apb_clk);
	clk_disable_unprepare(prg->axi_clk);

	mutex_lock(&prg->chan[prg_ch].mutex);
	prg->chan[prg_ch].in_use = false;
	mutex_unlock(&prg->chan[prg_ch].mutex);

	return 0;
}
EXPORT_SYMBOL(ipu_prg_disable);

int ipu_prg_wait_buf_ready(unsigned int ipu_id, unsigned int pre_num,
			   unsigned int hsk_line_num,
			   int pre_store_out_height)
{
	struct ipu_prg_data *prg = get_prg(ipu_id);
	int prg_ch, timeout = 1000;
	u32 reg;

	if (!prg)
		return -EINVAL;

	prg_ch = get_prg_chan(prg, pre_num);
	if (prg_ch < 0)
		return prg_ch;

	clk_prepare_enable(prg->apb_clk);

	spin_lock(&prg->lock);
	if (pre_store_out_height <= (4 << hsk_line_num)) {
		do {
			reg = prg_read(prg, IPU_PR_STATUS);
			udelay(1000);
			timeout--;
		} while (!(reg & IPU_PR_STATUS_BUF_RDY(prg_ch, 0)) && timeout);
	} else {
		do {
			reg = prg_read(prg, IPU_PR_STATUS);
			udelay(1000);
			timeout--;
		} while ((!(reg & IPU_PR_STATUS_BUF_RDY(prg_ch, 0)) ||
			  !(reg & IPU_PR_STATUS_BUF_RDY(prg_ch, 1))) && timeout);
	}
	spin_unlock(&prg->lock);

	clk_disable_unprepare(prg->apb_clk);

	if (!timeout)
		dev_err(prg->dev, "wait for buffer ready timeout\n");

	return 0;
}
EXPORT_SYMBOL(ipu_prg_wait_buf_ready);

static int ipu_prg_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node, *memory;
	struct ipu_prg_data *prg;
	struct resource *res;
	int id, i;

	prg = devm_kzalloc(&pdev->dev, sizeof(*prg), GFP_KERNEL);
	if (!prg)
		return -ENOMEM;
	prg->dev = &pdev->dev;

	for (i = 0; i < PRG_CHAN_NUM; i++)
		mutex_init(&prg->chan[i].mutex);

	spin_lock_init(&prg->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	prg->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(prg->base))
		return PTR_ERR(prg->base);

	prg->axi_clk = devm_clk_get(&pdev->dev, "axi");
	if (IS_ERR(prg->axi_clk)) {
		dev_err(&pdev->dev, "failed to get the axi clk\n");
		return PTR_ERR(prg->axi_clk);
	}

	prg->apb_clk = devm_clk_get(&pdev->dev, "apb");
	if (IS_ERR(prg->apb_clk)) {
		dev_err(&pdev->dev, "failed to get the apb clk\n");
		return PTR_ERR(prg->apb_clk);
	}

	prg->regmap = syscon_regmap_lookup_by_phandle(np, "gpr");
	if (IS_ERR(prg->regmap)) {
		dev_err(&pdev->dev, "failed to get regmap\n");
		return PTR_ERR(prg->regmap);
	}

	memory = of_parse_phandle(np, "memory-region", 0);
	if (!memory)
		return -ENODEV;

	prg->memory = of_translate_address(memory,
				of_get_address(memory, 0, NULL, NULL));

	id = of_alias_get_id(np, "prg");
	if (id < 0) {
		dev_err(&pdev->dev, "failed to get PRG id\n");
		return id;
	}
	prg->id = id;

	mutex_lock(&prg_lock);
	list_add_tail(&prg->list, &prg_list);
	mutex_unlock(&prg_lock);

	platform_set_drvdata(pdev, prg);

	dev_info(&pdev->dev, "driver probed\n");

	return 0;
}

static int ipu_prg_remove(struct platform_device *pdev)
{
	struct ipu_prg_data *prg = platform_get_drvdata(pdev);

	mutex_lock(&prg_lock);
	list_del(&prg->list);
	mutex_unlock(&prg_lock);

	return 0;
}

static const struct of_device_id imx_ipu_prg_dt_ids[] = {
	{ .compatible = "fsl,imx6q-prg", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_ipu_prg_dt_ids);

static struct platform_driver ipu_prg_driver = {
	.driver = {
			.name = "imx-prg",
			.of_match_table = of_match_ptr(imx_ipu_prg_dt_ids),
		  },
	.probe  = ipu_prg_probe,
	.remove = ipu_prg_remove,
};

static int __init ipu_prg_init(void)
{
	return platform_driver_register(&ipu_prg_driver);
}
subsys_initcall(ipu_prg_init);

static void __exit ipu_prg_exit(void)
{
	platform_driver_unregister(&ipu_prg_driver);
}
module_exit(ipu_prg_exit);

MODULE_DESCRIPTION("i.MX PRG driver");
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_LICENSE("GPL");
