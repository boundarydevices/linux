/*
 * drivers/amlogic/spi-nor/aml-spifc.c
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

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/spi-nor.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define AML_SPIFC_CMD		0x00
#define AML_SPIFC_ADDR		0x04
#define AML_SPIFC_CTRL		0x08
#define AML_SPIFC_CTRL1		0x0c
#define AML_SPIFC_STATUS	0x10
#define AML_SPIFC_CTRL2		0x14
#define AML_SPIFC_CLK		0x18
#define AML_SPIFC_USER		0x1c
#define AML_SPIFC_USER1		0x20
#define AML_SPIFC_USER2		0x24
#define AML_SPIFC_USER3		0x28
#define AML_SPIFC_PIN		0x2c
#define AML_SPIFC_SLAVE		0x30
#define AML_SPIFC_SLAVE1	0x34
#define AML_SPIFC_SLAVE2	0x38
#define AML_SPIFC_SLAVE3	0x3c
#define AML_SPIFC_CACHE0	0x40
#define AML_SPIFC_CACHE1	0x44
#define AML_SPIFC_CACHE2	0x48
#define AML_SPIFC_CACHE3	0x4c
#define AML_SPIFC_CACHE4	0x50
#define AML_SPIFC_CACHE5	0x54
#define AML_SPIFC_CACHE6	0x58
#define AML_SPIFC_CACHE7	0x5c
#define AML_SPIFC_BUF0		0x60
#define AML_SPIFC_BUF1		0x64
#define AML_SPIFC_BUF2		0x68
#define AML_SPIFC_BUF3		0x6c
#define AML_SPIFC_BUF4		0x70
#define AML_SPIFC_BUF5		0x74
#define AML_SPIFC_BUF6		0x78
#define AML_SPIFC_BUF7		0x7c

/* command register config bit */
#define CMD_READ_FINISH		BIT(31)
#define CMD_USER_DEFINE		BIT(18)
#define CMD_ADDR_OUT		BIT(15)
#define CMD_DATA_IN			BIT(13)
#define CMD_DATA_OUT		BIT(12)

/* control register config bit */
#define CTRL_READ_QIO		BIT(24)
#define CTRL_READ_DIO		BIT(23)
#define CTRL_WP_ENABLE		BIT(21)
#define CTRL_READ_QOUT		BIT(20)
#define CTRL_AHB_ENABLE		BIT(17)
#define CTRL_READ_DOUT		BIT(14)
#define CTRL_READ_FAST		BIT(13)

/* user register config bit */
#define USER_CMD_DEFINE		BIT(31)
#define USER_ADDR_OUT		BIT(30)
#define USER_DUMMY_OUT		BIT(29)
#define USER_DATA_IN		BIT(28)
#define USER_DATA_OUT		BIT(27)
#define USER_WRITE_1WIRE	BIT(16)
#define USER_WRITE_QIO		BIT(15)
#define USER_WRITE_DIO		BIT(14)
#define USER_WRITE_QOUT		BIT(13)
#define USER_WRITE_DOUT		BIT(12)
#define USER_CS_SETUP		BIT(5)
#define USER_CS_HOLD		BIT(4)
#define USER_AHB_CONFIG		BIT(3)
#define USER_COMPATIBLE		BIT(2)
#define USER_ADDR_WIDTH		BIT(1)

/* user register1  */
#define USER1_ADDR_SHIFTS	26
#define USER1_DOUT_SHIFTS	17
#define USER1_DIN_SHIFTS	8

/* clock register */
#define CLK_EQUAL_SYS		BIT(31)
#define CLK_PRESCALE_CNT	18
#define CLK_DIV_CNT			12
#define CLK_HDIV_CNT		6
#define CLK_LDIV_CNT		0

/* user register2 config bit */
#define USER2_CMD_BIT		7
#define USER2_CMD_SHIFTS	28

/* slave register config bit */
#define SLAVE_SW_RESET		BIT(31)
#define SLAVE_DEV_MODE		BIT(30)
#define SLAVE_XFER_DONE		BIT(4)

#define AML_SPIFC_CACHE_SIZE	64

#define AML_SPIFC_OP_READ	BIT(0)
#define AML_SPIFC_OP_WRITE	BIT(1)

/* please note that "val" can not be 0 */
#define aml_mask(val)	GENMASK((fls(val) - 1), (ffs(val) - 1))
/* set function set bit to 1, clear function set bit to 0 */
#define amlsf_set_bits(val, reg, mask) \
	writel((readl(reg) | ((val) & (mask))), (reg))
#define amlsf_clr_bits(val, reg, mask) \
	writel((readl(reg) & (~((val) & (mask)))), (reg))
#define amlsf_set_1bit(val, reg)	writel((readl(reg) | (val)), (reg))
#define amlsf_clr_1bit(val, reg)	writel((readl(reg) & (~(val))), (reg))
#define amlsf_clr_reg(reg)			writel(0UL, (reg))
/* #define AML_SPIFC_AHB	1 */
#define AML_SPIFC_MAX_CLK_RATE	166666666

struct aml_spi_flash {
	struct device *dev;
	struct mutex lock;

	void __iomem *regbase;
#ifdef AML_SPIFC_AHB
	void __iomem *ahbbase;
#endif
	struct clk *clk;
	unsigned int clkrate;
	void *priv;
	unsigned int cs;

	struct spi_nor	*nor;
};

static void aml_spifc_get(struct aml_spi_flash *spi_flash)
{
	gpio_set_value(spi_flash->cs, 0);
}

static void aml_spifc_relax(struct aml_spi_flash *spi_flash)
{
	gpio_set_value(spi_flash->cs, 1);
}

static void aml_spifc_reset_read_mode(struct aml_spi_flash *spi_flash)
{
	unsigned int ctrl;

	ctrl = CTRL_READ_QIO |
			CTRL_READ_DIO |
			CTRL_READ_QOUT |
			CTRL_READ_DOUT |
			CTRL_READ_FAST;
	amlsf_clr_bits(ctrl, spi_flash->regbase +
				   AML_SPIFC_CTRL, aml_mask(ctrl));
}

static void aml_spifc_set_read_mode(struct aml_spi_flash *spi_flash)
{
	struct spi_nor *nor = spi_flash->nor;

	aml_spifc_reset_read_mode(spi_flash);

	switch (nor->flash_read) {
	case SPI_NOR_FAST:
		amlsf_set_1bit(CTRL_READ_FAST, spi_flash->regbase +
					   AML_SPIFC_CTRL);
		break;
	case SPI_NOR_DUAL:
		amlsf_set_1bit(CTRL_READ_DOUT, spi_flash->regbase +
					   AML_SPIFC_CTRL);
		break;
	case SPI_NOR_QUAD:
		amlsf_set_1bit(CTRL_READ_QOUT, spi_flash->regbase +
					   AML_SPIFC_CTRL);
		break;
	case SPI_NOR_NORMAL:
	default:
		break;
	}
}

static unsigned int aml_spifc_set_write_mode(struct aml_spi_flash *spi_flash)
{
	unsigned int user = 0UL;
	/*
	 * TODO:
	 * set write mode by program_proto in next version
	 * the kernel-4.9 do not support D/Q write yet
	 */
	return user;
}

static int aml_spifc_transfer(struct aml_spi_flash *spi_flash,
u_char *buf, size_t length, u8 op_flag)
{
	unsigned int user1;
	size_t len, count;
	unsigned int *ptr;
	int i;
	u8 temp_buf[AML_SPIFC_CACHE_SIZE];

	if (length > AML_SPIFC_CACHE_SIZE)
		return -ENOBUFS;

	len = length;
	count = (len / 4) + !!(len % 4);
	if (op_flag == AML_SPIFC_OP_READ) {
		ptr = (u32 *)temp_buf;
		writel(USER_DATA_IN, spi_flash->regbase +
			   AML_SPIFC_USER);
		user1 = ((len << 3) - 1) << USER1_DIN_SHIFTS;
		writel(user1, spi_flash->regbase +
			   AML_SPIFC_USER1);
		amlsf_clr_reg(spi_flash->regbase +
					  AML_SPIFC_USER2);
		amlsf_clr_reg(spi_flash->regbase +
					  AML_SPIFC_ADDR);
		writel(CMD_USER_DEFINE, spi_flash->regbase +
			   AML_SPIFC_CMD);
		while (readl(spi_flash->regbase + AML_SPIFC_CMD) &
			   CMD_USER_DEFINE)
		;
		for (i = 0; i < count; i++)
			*ptr++ = readl(spi_flash->regbase +
						   AML_SPIFC_CACHE0 + 4 * i);
		memcpy(buf, temp_buf, len);
	} else if (op_flag == AML_SPIFC_OP_WRITE) {
		ptr = (u32 *)buf;
		for (i = 0; i < count; i++)
			writel(*ptr++, spi_flash->regbase +
				   AML_SPIFC_CACHE0 + 4 * i);
		writel(USER_DATA_OUT, spi_flash->regbase +
			   AML_SPIFC_USER);
		user1 = ((len << 3) - 1) << USER1_DOUT_SHIFTS;
		writel(user1, spi_flash->regbase +
			   AML_SPIFC_USER1);
		amlsf_clr_reg(spi_flash->regbase +
					  AML_SPIFC_USER2);
		amlsf_clr_reg(spi_flash->regbase +
					  AML_SPIFC_ADDR);
		writel(CMD_USER_DEFINE, spi_flash->regbase +
			   AML_SPIFC_CMD);
		while (readl(spi_flash->regbase + AML_SPIFC_CMD) &
			   CMD_USER_DEFINE)
		;
	}
	return 0;
}

static int aml_spifc_prep(struct spi_nor *nor, enum spi_nor_ops ops)
{
	struct aml_spi_flash *spi_flash = nor->priv;

	mutex_lock(&spi_flash->lock);

	return 0;
}

static void aml_spifc_unprep(struct spi_nor *nor, enum spi_nor_ops ops)
{
	struct aml_spi_flash *spi_flash = nor->priv;

	mutex_unlock(&spi_flash->lock);
}

static int aml_spifc_read_reg(struct spi_nor *nor, u8 opcode, u8 *buf, int len)
{
	struct aml_spi_flash *spi_flash = nor->priv;
	unsigned int user, user1, user2;
	unsigned int *din;
	int count, i;
	u8 temp_buf[AML_SPIFC_CACHE_SIZE];

	if (len > AML_SPIFC_CACHE_SIZE)
		return -ENOBUFS;

	aml_spifc_get(spi_flash);
	aml_spifc_reset_read_mode(spi_flash);

	din = (unsigned int *)temp_buf;
	count = (len / 4) + !!(len % 4);
	user = USER_CMD_DEFINE | USER_DATA_IN;
	user1 = (((len << 3) - 1) & GENMASK(8, 0)) << USER1_DIN_SHIFTS;
	user2 = (USER2_CMD_BIT << USER2_CMD_SHIFTS) | opcode;
	writel(user, spi_flash->regbase +
		   AML_SPIFC_USER);
	writel(user1, spi_flash->regbase +
		   AML_SPIFC_USER1);
	writel(user2, spi_flash->regbase +
		   AML_SPIFC_USER2);
	writel(CMD_USER_DEFINE, spi_flash->regbase + AML_SPIFC_CMD);
	while (readl(spi_flash->regbase + AML_SPIFC_CMD) &
		   CMD_USER_DEFINE)
		;
	for (i = 0; i < count; i++)
		*din++ = readl(spi_flash->regbase +
					   AML_SPIFC_CACHE0 + 4 * i);
	memcpy(buf, temp_buf, len);
	aml_spifc_relax(spi_flash);
	return 0;
}

ssize_t aml_spifc_read(struct spi_nor *nor, loff_t from,
size_t len, u_char *read_buf)
{
	struct aml_spi_flash *spi_flash = nor->priv;
	u_char *din;
	unsigned int user, user1, user2;
	size_t offset, trans;
	loff_t addr;
	int ret;

	aml_spifc_get(spi_flash);
	din = read_buf;
	user = USER_CMD_DEFINE | USER_ADDR_OUT;
	if (nor->addr_width == 4) {
		addr = from & GENMASK(31, 0);
		user1 = 31 << USER1_ADDR_SHIFTS;
		user |= USER_ADDR_WIDTH;
	} else {
		addr = from & GENMASK(23, 0);
		addr = addr << 8;
		user1 = 23 << USER1_ADDR_SHIFTS;
	}
	aml_spifc_set_read_mode(spi_flash);
	user2 = nor->read_opcode | USER2_CMD_BIT << USER2_CMD_SHIFTS;
	writel(user, spi_flash->regbase +
		   AML_SPIFC_USER);
	writel(user1, spi_flash->regbase +
		   AML_SPIFC_USER1);
	writel(user2, spi_flash->regbase +
		   AML_SPIFC_USER2);
	writel(addr, spi_flash->regbase +
		   AML_SPIFC_ADDR);
	writel(CMD_USER_DEFINE, spi_flash->regbase +
		   AML_SPIFC_CMD);
	while (readl(spi_flash->regbase + AML_SPIFC_CMD) &
		   CMD_USER_DEFINE)
		;
	for (offset = 0; offset < len; offset += AML_SPIFC_CACHE_SIZE) {
		trans = min_t(size_t, (len - offset), AML_SPIFC_CACHE_SIZE);
		ret = aml_spifc_transfer(spi_flash, din,
				trans, AML_SPIFC_OP_READ);
		if (ret) {
			dev_warn(nor->dev, "spi-nor read error %s %d\n",
					 __func__, __LINE__);
			aml_spifc_relax(spi_flash);
			return ret;
		}
		din += trans;
	}
	aml_spifc_relax(spi_flash);
	return len;
}

static int aml_spifc_write_reg(struct spi_nor *nor, u8 opcode, u8 *buf, int len)
{
	struct aml_spi_flash *spi_flash = nor->priv;
	unsigned int user, user1, user2;
	unsigned int *dout;
	int count, i;
	u8 temp_buf[AML_SPIFC_CACHE_SIZE];

	if (len > AML_SPIFC_CACHE_SIZE)
		return -ENOBUFS;

	aml_spifc_get(spi_flash);
	dout = (unsigned int *)temp_buf;
	user = USER_CMD_DEFINE;
	if (len > 0) {
		user |= USER_DATA_OUT;
		user1 = (((len << 3) - 1) & GENMASK(8, 0)) << USER1_DOUT_SHIFTS;
		memcpy(temp_buf, buf, len);
		count = (len / 4) + !!(len % 4);
		for (i = 0; i < count; i++)
			writel(*dout++, spi_flash->regbase +
				   AML_SPIFC_CACHE0 + 4 * i);
	} else
		user1 = 0;
	user2 = (USER2_CMD_BIT << USER2_CMD_SHIFTS) | opcode;

	writel(user, spi_flash->regbase +
		   AML_SPIFC_USER);
	writel(user1, spi_flash->regbase +
		   AML_SPIFC_USER1);
	writel(user2, spi_flash->regbase +
		   AML_SPIFC_USER2);
	writel(CMD_USER_DEFINE, spi_flash->regbase +
		   AML_SPIFC_CMD);
	while (readl(spi_flash->regbase + AML_SPIFC_CMD) &
		   CMD_USER_DEFINE)
		;
	aml_spifc_relax(spi_flash);
	return 0;
}

static ssize_t aml_spifc_write(struct spi_nor *nor, loff_t to,
size_t len, const u_char *write_buf)
{
	struct aml_spi_flash *spi_flash = nor->priv;
	u_char dout[AML_SPIFC_CACHE_SIZE] = {0xFF};
	size_t offset, trans;
	loff_t addr;
	unsigned int user, user1, user2;
	int ret;

	addr = to;
	aml_spifc_get(spi_flash);
	user = aml_spifc_set_write_mode(spi_flash);
	user |= USER_CMD_DEFINE | USER_ADDR_OUT;
	if (nor->addr_width == 4) {
		addr = to & GENMASK(31, 0);
		user1 = 31 << USER1_ADDR_SHIFTS;
		user |= USER_ADDR_WIDTH;
	} else {
		addr = to & GENMASK(23, 0);
		/* controller request addr by this way */
		addr = addr << 8;
		user1 = 23 << USER1_ADDR_SHIFTS;
	}
	user2 = nor->program_opcode |
			USER2_CMD_BIT << USER2_CMD_SHIFTS;
	writel(user, spi_flash->regbase +
		   AML_SPIFC_USER);
	writel(user1, spi_flash->regbase +
		   AML_SPIFC_USER1);
	writel(user2, spi_flash->regbase +
		   AML_SPIFC_USER2);
	writel(addr, spi_flash->regbase +
		   AML_SPIFC_ADDR);
	writel(CMD_USER_DEFINE, spi_flash->regbase +
		   AML_SPIFC_CMD);
	while (readl(spi_flash->regbase + AML_SPIFC_CMD) &
		   CMD_USER_DEFINE)
		;

	for (offset = 0; offset < len; offset += AML_SPIFC_CACHE_SIZE) {
		trans = min_t(size_t, (len - offset), AML_SPIFC_CACHE_SIZE);
		memcpy(dout, write_buf + offset, trans);
		ret = aml_spifc_transfer(spi_flash, dout,
				trans, AML_SPIFC_OP_WRITE);
		if (ret) {
			dev_warn(nor->dev, "spi-nor write error %s %d\n",
					 __func__, __LINE__);
			aml_spifc_relax(spi_flash);
			return ret;
		}
	}
	aml_spifc_relax(spi_flash);
	return len;
}

static int aml_spifc_hw_init(struct aml_spi_flash *spi_flash)
{
	unsigned int div, clk;

	amlsf_set_1bit(SLAVE_SW_RESET, spi_flash->regbase +
				  AML_SPIFC_SLAVE);
	/* do not compatible to applo */
	amlsf_clr_1bit(USER_COMPATIBLE, spi_flash->regbase +
				 AML_SPIFC_USER);
	amlsf_clr_1bit(SLAVE_DEV_MODE, spi_flash->regbase +
				 AML_SPIFC_SLAVE);
	div = AML_SPIFC_MAX_CLK_RATE / spi_flash->clkrate;
	if (div < 2)
		div = 2UL;
	if (div > 0x3f)
		div = 0x3fUL;
	amlsf_clr_reg(spi_flash->regbase +
				  AML_SPIFC_CLK);
	clk = (div - 1) << CLK_DIV_CNT;
	clk |= ((div >> 1) - 1) << CLK_HDIV_CNT;
	clk |= (div - 1) << CLK_LDIV_CNT;
	writel(clk, spi_flash->regbase +
		   AML_SPIFC_CLK);
	return 0;
}

static int aml_spifc_init(struct aml_spi_flash *spi_flash,
struct device_node *np)
{
	struct device *dev = spi_flash->dev;
	struct spi_nor *nor;
	struct mtd_info *mtd;
	unsigned int read_caps = 0;
	int ret;

	nor = devm_kzalloc(dev, sizeof(*nor), GFP_KERNEL);
	if (!nor)
		return -ENOMEM;

	if (!np)
		pr_info("np is a null node\n");
	nor->dev = dev;
	spi_nor_set_flash_node(nor, np);

	ret = of_property_read_u32(np, "spifc-frequency",
			&spi_flash->clkrate);
	if (ret) {
		dev_err(dev, "There's no spifc-frequency property for %pOF\n",
			np);
		return ret;
	}
	spi_flash->cs = of_get_named_gpio(np, "cs_gpios", 0);
	gpio_free(spi_flash->cs);
	gpio_request(spi_flash->cs, "spifc-cs");
	gpio_direction_output(spi_flash->cs, 1);

	aml_spifc_hw_init(spi_flash);
	ret = of_property_read_u32(np, "read-capability", &read_caps);
	spi_flash->nor = nor;
	nor->priv = spi_flash;

	nor->prepare = aml_spifc_prep;
	nor->unprepare = aml_spifc_unprep;
	nor->read_reg = aml_spifc_read_reg;
	nor->write_reg = aml_spifc_write_reg;
	nor->read = aml_spifc_read;
	nor->write = aml_spifc_write;
	nor->erase = NULL;
	ret = spi_nor_scan(nor, NULL, read_caps);
	if (ret)
		return ret;

	mtd = &nor->mtd;
	mtd->name = (np->name)?np->name:"amlogic spi-nor";
	ret = mtd_device_register(mtd, NULL, 0);
	if (ret)
		return ret;

	return 0;
}
static int aml_spifc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct aml_spi_flash *spi_flash;
	struct device_node *np;
	int ret;

	spi_flash = devm_kzalloc(dev, sizeof(*spi_flash), GFP_KERNEL);
	if (!spi_flash)
		return -ENOMEM;

	platform_set_drvdata(pdev, spi_flash);
	spi_flash->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	spi_flash->regbase = devm_ioremap_resource(dev, res);
	if (IS_ERR(spi_flash->regbase))
		return PTR_ERR(spi_flash->regbase);
#ifdef AML_SPIFC_AHB
	res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "ahb_register");
	spi_flash->ahbbase = devm_ioremap_resource(dev, res);
	if (IS_ERR(spi_flash->ahbbase))
		return PTR_ERR(spi_flash->ahbbase);
#endif

	spi_flash->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(spi_flash->clk))
		return PTR_ERR(spi_flash->clk);

	ret = clk_prepare_enable(spi_flash->clk);
	if (ret)
		goto err_out;

	mutex_init(&spi_flash->lock);
	np = of_get_next_available_child(pdev->dev.of_node, NULL);
	if (!np) {
		dev_err(&pdev->dev, "no aml-spifc to configure\n");
		goto err_out;
	}
	ret = aml_spifc_init(spi_flash, np);

err_out:
	if (ret)
		mutex_destroy(&spi_flash->lock);
	clk_disable_unprepare(spi_flash->clk);
	return ret;
}

static int aml_spifc_remove(struct platform_device *pdev)
{
	struct aml_spi_flash *spi_flash = platform_get_drvdata(pdev);

	mtd_device_unregister(&(spi_flash->nor->mtd));
	mutex_destroy(&spi_flash->lock);
	clk_disable_unprepare(spi_flash->clk);
	return 0;
}

static const struct of_device_id aml_spi_nor_dt_ids[] = {
	{ .compatible = "amlogic,aml-spi-nor"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aml_spi_nor_dt_ids);

static struct platform_driver aml_spifc_driver = {
	.driver = {
		.name	= "amlogic-spifc",
		.of_match_table = aml_spi_nor_dt_ids,
	},
	.probe	= aml_spifc_probe,
	.remove	= aml_spifc_remove,
};
module_platform_driver(aml_spifc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Amlogic SPI Nor Flash Controller Driver");

