/*
 * drivers/amlogic/ethernet/phy/phy_debug.c
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

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/pinctrl/consumer.h>
#include <linux/net_tstamp.h>
#include <linux/reset.h>
#include <linux/of_mdio.h>

#ifdef CONFIG_DWMAC_MESON
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include "phy_debug.h"

void __iomem *PREG_ETH_REG0;
void __iomem *PREG_ETH_REG1;

static int g_phyreg;
static void __iomem *c_ioaddr;
static ssize_t show_phy_reg(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "current phy reg = 0x%x\n", g_phyreg);
}

static ssize_t set_phy_reg(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ovl;
	int r = kstrtoint(buf, 0, &ovl);

	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}
	g_phyreg = ovl;
	pr_info("---ovl=0x%x\n", ovl);
	return count;
}

static ssize_t show_phy_reg_value(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct phy_device *phy_dev = dev_get_drvdata(dev);
	int ret, val, i;

	for (i = 0; i < 32; i++)
		pr_info("%d: 0x%x\n", i, phy_read(phy_dev, i));
	val = phy_read(phy_dev, g_phyreg);
	ret = snprintf(buf, PAGE_SIZE, "phy reg 0x%x = 0x%x\n", g_phyreg, val);

	return ret;
}

static ssize_t set_phy_reg_value(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ovl;
	int ret;

	struct phy_device *phy_dev = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &ovl);
	pr_info("---reg 0x%x: ovl=0x%x\n", g_phyreg, ovl);
	phy_write(phy_dev, g_phyreg, ovl);
	return count;
}

static struct device_attribute phy_reg_attrs[] = {
	__ATTR(phy_reg, 0644, show_phy_reg, set_phy_reg),
	__ATTR(phy_reg_value, 0644, show_phy_reg_value, set_phy_reg_value)
};

static struct phy_device *c_phy_dev;
void am_net_dump_phyreg(void)
{
	int reg = 0;
	int val = 0;

	if (!c_phy_dev)
		return;

	pr_info("========== ETH PHY new regs ==========\n");
	for (reg = 0; reg < 32; reg++) {
		val = phy_read(c_phy_dev, reg);
		pr_info("[reg_%d] 0x%x\n", reg, val);
	}
}

static int am_net_read_phyreg(int argc, char **argv)
{
	int reg = 0;
	int val = 0;
	int r = 0;

	if (!c_phy_dev)
		return -1;
	if (argc < 2 || !argv || !argv[0] || !argv[1]) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	r = kstrtoint(argv[1], 0, &reg);
	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}

	if (reg >= 0 && reg <= 31) {
		val = phy_read(c_phy_dev, reg);
		pr_info("read phy [reg_%d] 0x%x\n", reg, val);
	} else if (reg == 32) {
		pr_info("phy features:0x%x\n", c_phy_dev->drv->features);
	} else {
		pr_info("Invalid parameter\n");
	}

	return 0;
}

static int am_net_write_phyreg(int argc, char **argv)
{
	int reg = 0;
	int val = 0;
	int r = 0;

	if (!c_phy_dev)
		return -1;

	if (argc < 3 || !argv || !argv[0] || !argv[1] || !argv[2]) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	r = kstrtoint(argv[1], 0, &reg);
	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}

	r = kstrtoint(argv[2], 0, &val);
	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}

	if (reg >= 0 && reg <= 31) {
		phy_write(c_phy_dev, reg, val);
		pr_info(
			"write phy [reg_%d] 0x%x, 0x%x\n",
			reg, val, phy_read(c_phy_dev, reg));
	} else if (reg == 32) {
		if (val > 255) {
			pr_info("Invalid parameter\n");
			return 0;
		}
		c_phy_dev->drv->features &= 0xff;
		c_phy_dev->drv->features |= val << 8;
	} else if (reg == 33) {
		if (val > 255) {
			pr_info("Invalid parameter\n");
			return 0;
		}
		c_phy_dev->drv->features &= 0xff00;
		c_phy_dev->drv->features |= val;
	} else {
		pr_info("Invalid parameter\n");
	}

	return 0;
}

void am_net_dump_phy_wol_reg(void)
{
	int reg;
	int val;
	int val15;
	int val16;

	if (!c_phy_dev)
		return;

	pr_info("========== ETH PHY wol regs ==========\n");
	for (reg = 0; reg < 13; reg++) {
		/*Bit 15: READ*/
		/*Bit 14: Write*/
		/*Bit 12:11: BANK_SEL (0: DSP, 1: WOL, 3: BIST)*/
		/*Bit 10: Test Mode*/
		/*Bit 9:5: Read Address*/
		/*Bit 4:0: Write Address*/
		val = (1 << 15) | (1 << 11) | (1 << 10) | (reg << 5);
		phy_write(c_phy_dev, 0x14, val);
		val15 = phy_read(c_phy_dev, 0x15);
		val16 = phy_read(c_phy_dev, 0x16);
		val = val16 * 65536 + val15;
		pr_info("wol [reg_%d] 0x%x\n", reg, val);
	}
}

void am_net_dump_phy_bist_reg(void)
{
	int reg;
	int val;
	int val15;
	int val16;

	if (!c_phy_dev)
		return;

	pr_info("========== ETH PHY bist regs ==========\n");
	for (reg = 0; reg < 32; reg++) {
		/*Bit 15: READ*/
		/*Bit 14: Write*/
		/*Bit 12:11: BANK_SEL (0: DSP, 1: WOL, 3: BIST)*/
		/*Bit 10: Test Mode*/
		/*Bit 9:5: Read Address*/
		/*Bit 4:0: Write Address*/

		val = ((1 << 15) | (1 << 12) | (1 << 11) |
			(1 << 10) | (reg << 5));
		phy_write(c_phy_dev, 0x14, val);
		val15 = phy_read(c_phy_dev, 0x15);
		val16 = phy_read(c_phy_dev, 0x16);
		val = val16 * 65536 + val15;
		pr_info("bist [reg_%d] 0x%x\n", reg, val);
	}
}

static int read_tst_cntl_reg(int argc, char **argv)
{
	int rd_data_hi;
	int rd_addr;
	int rd_data = 0;

	if (argc < 2 || (!argv) || (!argv[0]) || (!argv[1])) {
		pr_info("Invalid syntax\n");
		return -1;
	}
	if (kstrtoint(argv[1], 16, &rd_addr) < 0)
		return -1;

	if (rd_addr >= 0 && rd_addr <= 31) {
		phy_write(
			c_phy_dev, 20,
			((1 << 15) | (1 << 10) | ((rd_addr & 0x1f) << 5)));

		rd_data = phy_read(c_phy_dev, 21);
		rd_data_hi = phy_read(c_phy_dev, 22);
		rd_data = ((rd_data_hi & 0xffff) << 16) | rd_data;
		pr_info("read tstcntl phy [reg_%d] 0x%x\n", rd_addr, rd_data);
	} else {
		pr_info("Invalid parameter\n");
	}

	return rd_data;
}

static int return_write_val(int rd_addr)
{
	int rd_data;
	int rd_data_hi;

	phy_write(
		c_phy_dev, 20,
		((1 << 15) | (1 << 10) | ((rd_addr & 0x1f) << 5)));
	rd_data = phy_read(c_phy_dev, 21);
	rd_data_hi = phy_read(c_phy_dev, 22);
	rd_data = ((rd_data_hi & 0xffff) << 16) | rd_data;

	return rd_data;
}

static int write_tst_cntl_reg(int argc, char **argv)
{
	int wr_addr, wr_data;

	if (argc < 3 || (!argv) || (!argv[0]) || (!argv[1]) || (!argv[2])) {
		pr_info("Invalid syntax\n");
		return -1;
	}

	if (kstrtoint(argv[1], 16, &wr_addr) < 0)
		return -1;

	if (kstrtoint(argv[2], 16, &wr_data) < 0)
		return -1;

	if (wr_addr >= 0 && wr_addr <= 31) {
		phy_write(c_phy_dev, 23, (wr_data & 0xffff));

		phy_write(
			c_phy_dev, 20,
			((1 << 14) | (1 << 10) | ((wr_addr << 0) & 0x1f)));

		pr_info("write phy tstcntl [reg_%d] 0x%x, 0x%x\n",
			wr_addr, wr_data, return_write_val(wr_addr));
	} else {
		pr_info("Invalid parameter\n");
	}

	return 0;
}

static void tstcntl_dump_phyreg(void)
{
	int rd_addr;
	int rd_data;
	int rd_data_hi;

	pr_info("========== ETH TST PHY regs ==========\n");
	for (rd_addr = 0; rd_addr < 32; rd_addr++) {
		phy_write(
			c_phy_dev, 20,
			((1 << 15) | (1 << 10) | ((rd_addr & 0x1f) << 5)));

		rd_data = phy_read(c_phy_dev, 21);
		rd_data_hi = phy_read(c_phy_dev, 22);
		rd_data = ((rd_data_hi & 0xffff) << 16) | rd_data;
		pr_info("tstcntl phy [reg_%d] 0x%x\n", rd_addr, rd_data);
	}
}

void am_net_dump_phy_extended_reg(void)
{
	int reg;
	int val;
	int val15;
	int val16;

	if (!c_phy_dev)
		return;

	pr_info("========== ETH PHY extended regs ==========\n");
	for (reg = 0; reg < 32; reg++) {
		/*Bit 15: READ*/
		/*Bit 14: Write*/
		/*Bit 12:11: BANK_SEL (0: DSP, 1: WOL, 3: BIST)*/
		/*Bit 10: Test Mode*/
		/*Bit 9:5: Read Address*/
		/*Bit 4:0: Write Address*/

		val = ((1 << 15) | (1 << 10) | (reg << 5));
		phy_write(c_phy_dev, 0x14, val);
		val15 = phy_read(c_phy_dev, 0x15);
		val16 = phy_read(c_phy_dev, 0x16);
		val = (val16 << 16) + val15;
		pr_info("extended [reg_%d] 0x%x\n", reg, val);
	}
}

static void am_net_dump_macreg(void)
{
	int reg;
	int val;

	pr_info("========== ETH_MAC regs ==========\n");
	for (reg = ETH_MAC_0_Configuration;
		reg <= ETH_MMC_rxicmp_err_octets; reg += 0x4) {
		val = readl(c_ioaddr + reg);
		pr_info("[0x%04x] 0x%x\n", reg, val);
	}

	pr_info("========== ETH_DMA regs ==========\n");
	for (reg = ETH_DMA_0_Bus_Mode;
		reg <= ETH_DMA_21_Curr_Host_Re_Buffer_Addr; reg += 0x4) {
		val = readl(c_ioaddr + reg);
		pr_info("[0x%04x] 0x%x\n", reg, val);
	}
}

static int am_net_read_macreg(int argc, char **argv)
{
	int reg = 0;
	int val = 0;
	int r = 0;

	if (argc < 2 || (!argv) || (!argv[0]) || (!argv[1])) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	r  = kstrtoint(argv[1], 0, &reg);
	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}

	if (reg >= 0 && reg <= ETH_DMA_21_Curr_Host_Re_Buffer_Addr) {
		val = readl(c_ioaddr + reg);
		pr_info("read mac [0x4%x] 0x%x\n", reg, val);
	} else {
		pr_info("Invalid parameter\n");
	}

	return 0;
}

static int am_net_write_macreg(int argc, char **argv)
{
	int reg;
	int val;
	int r;

	if ((argc < 3) || (!argv) || (!argv[0]) || (!argv[1]) || (!argv[2])) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	r = kstrtoint(argv[1], 0, &reg);
	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}

	r = kstrtoint(argv[2], 0, &val);
	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}

	if (reg >= 0 && reg <= ETH_DMA_21_Curr_Host_Re_Buffer_Addr) {
		writel(val, (c_ioaddr + reg));
		pr_info("write mac [0x%x] 0x%x, 0x%x\n",
			reg, val, readl(c_ioaddr + reg));
	} else {
		pr_info("Invalid parameter\n");
	}

	return 0;
}

static const char *g_phyreg_help = {
	"Usage:\n"
	"    echo d > phyreg;            //dump ethernet phy reg\n"
	"    echo r reg > phyreg;        //read ethernet phy reg\n"
	"    echo w reg val > phyreg;    //write ethernet phy reg\n"
};

static ssize_t eth_phyreg_help(
	struct class *class, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", g_phyreg_help);
}

static unsigned char adc_data[32 * 32];
static unsigned char adc_freq[64];

static inline void am_init_tst_mode(void)
{
	phy_write(c_phy_dev, 20, 0x0400);
	phy_write(c_phy_dev, 20, 0x0000);
	phy_write(c_phy_dev, 20, 0x0400);
}

static inline void am_close_tst_mode(void)
{
	phy_write(c_phy_dev, 20, 0x0000);
}

static void am_net_adc_show(void)
{
	int rd_data = 0;
	int rd_data_hi = 0;
	int i, j, v;
	char buf[128];

	pr_info("%s, %d enter\n", __func__, __LINE__);

	am_init_tst_mode();

	for (i = 0; i < (32 * 32); i++) {
		v = (1 << 15) | (1 << 10) | ((16 & 0x1f) << 5);
		phy_write(c_phy_dev, 20, v);
		rd_data = phy_read(c_phy_dev, 21);
		rd_data_hi = phy_read(c_phy_dev, 22);
		adc_data[i] = rd_data & 0x3f;
	}

	am_close_tst_mode();

	for (i = 0; i < 32; i++) {
		for (j = 0; j < 32; j++)
			pr_info("%02x ", adc_data[i * 32 + j]);

		pr_info("\n");
	}

	for (i = 0; i < 64; i++)
		adc_freq[i] = 0;

	for (i = 0; i < 32; i++) {
		for (j = 0; j < 32; j++) {
			if (adc_data[i * 32 + j] > 31)
				adc_freq[adc_data[i * 32 + j] - 32]++;
			else
				adc_freq[32 + adc_data[i * 32 + j]]++;

			pr_info("%02x ", adc_data[i * 32 + j]);
		}
		pr_info("\n");
	}

	for (i = 0; i < 64; i++) {
		pr_info("%d(%04d):\t", i - 32, adc_freq[i]);
		if (adc_freq[i] > 128)
			adc_freq[i] = 128;

		for (j = 0; j < adc_freq[i]; j++)
			buf[j] = '#';

		buf[j] = '\0';

		pr_info("%s\n", buf);
	}
}

static void am_net_eye_pattern_on(void)
{
	unsigned int value;

	c_phy_dev->autoneg = AUTONEG_DISABLE;
	/*stop check wol when doing eye pattern, otherwise it will reset phy*/
	enable_wol_check = 0;
	phy_write(c_phy_dev, 0, 0x2100);
	value = phy_read(c_phy_dev, 17);
	pr_info("0x11 %x\n", value);
	phy_write(c_phy_dev, 17, (value | 0x0004));
}
static void am_net_eye_pattern_off(void)
{
	unsigned int value;

	c_phy_dev->autoneg = AUTONEG_ENABLE;
	enable_wol_check = 1;
	phy_write(c_phy_dev, 0, 0x1000);
	value = phy_read(c_phy_dev, 17);
	pr_info("0x11 %x\n", value);
	phy_write(c_phy_dev, 17, (value & ~0x0004));
}

static void am_net_debug_mode(void)
{	/*disable autoneg*/
	c_phy_dev->autoneg = AUTONEG_DISABLE;
	/*disable wol check*/
	enable_wol_check = 0;
}

static ssize_t eth_phyreg_func(
	struct class *class, struct class_attribute *attr,
	const char *buf, size_t count)
{
	int argc;
	char *buff, *p, *para;
	char *argv[4];
	char cmd;

	buff = kstrdup(buf, GFP_KERNEL);
	p = buff;
	for (argc = 0; argc < 4; argc++) {
		para = strsep(&p, " ");
		if (!para)
			break;
		argv[argc] = para;
	}
	if (argc < 1 || argc > 4)
		goto end;

	cmd = argv[0][0];
	switch (cmd) {
	case 'r':
	case 'R':
		am_net_read_phyreg(argc, argv);
		break;
	case 'w':
	case 'W':
		am_net_write_phyreg(argc, argv);
		break;
	case 'd':
	case 'D':
		am_net_dump_phyreg();
		break;
#ifdef CONFIG_MESON_ETH_WOL /* FIXED ME */
	case 'p':
		wol_test(c_phy_dev);
		break;
#endif
	case 'e':
		am_net_dump_phy_extended_reg();
		am_net_dump_phy_wol_reg();
		am_net_dump_phy_bist_reg();
		break;
	case 'c':
	case 'C':
		am_net_adc_show();
		break;
	case 't':
	case 'T':
		am_init_tst_mode();

		if (argv[0][1] == 'w' || argv[0][1] == 'W')
			write_tst_cntl_reg(argc, argv);

		if (argv[0][1] == 'r' || argv[0][1] == 'R')
			read_tst_cntl_reg(argc, argv);

		if (argv[0][1] == 'd' || argv[0][1] == 'D')
			tstcntl_dump_phyreg();

		am_close_tst_mode();
		break;
	case 'o':
	case 'O':
		if (argv[0][1] == 'n' || argv[0][1] == 'N') {
			am_net_eye_pattern_on();
			break;
		}
		if (argv[0][1] == 'f' || argv[0][1] == 'F') {
			am_net_eye_pattern_off();
			break;
		}
		if (argv[0][1] == 'd' || argv[0][1] == 'D') {
			am_net_debug_mode();
			break;
		}
		break;
	default:
		goto end;
	}

	return count;

end:
	kfree(buff);
	return 0;
}

static const char *g_macreg_help = {
	"Usage:\n"
	"    echo d > macreg;            //dump ethernet mac reg\n"
	"    echo r reg > macreg;        //read ethernet mac reg\n"
	"    echo w reg val > macreg;    //read ethernet mac reg\n"
};

static ssize_t eth_macreg_help(
	struct class *class,
	struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", g_macreg_help);
}

static ssize_t eth_macreg_func(
	struct class *class, struct class_attribute *attr,
	const char *buf, size_t count)
{
	int argc;
	char *buff, *p, *para;
	char *argv[4];
	char cmd;

	buff = kstrdup(buf, GFP_KERNEL);
	p = buff;
	for (argc = 0; argc < 4; argc++) {
		para = strsep(&p, " ");
		if (!para)
			break;
		argv[argc] = para;
	}
	if (argc < 1 || argc > 4)
		goto end;

	cmd = argv[0][0];
	switch (cmd) {
	case 'r':
	case 'R':
		am_net_read_macreg(argc, argv);
		break;
	case 'w':
	case 'W':
		am_net_write_macreg(argc, argv);
		break;
	case 'd':
	case 'D':
		am_net_dump_macreg();
		break;
	default:
		goto end;
	}

	return count;

end:
	kfree(buff);
	return 0;
}

static ssize_t eth_linkspeed_show(
	struct class *class, struct class_attribute *attr, char *buf)
{
	int ret;
	char buff[100];

	if (c_phy_dev) {
		phy_print_status(c_phy_dev);

		genphy_update_link(c_phy_dev);
		if (c_phy_dev->link)
			strcpy(buff, "link status: link\n");
		else
			strcpy(buff, "link status: unlink\n");
	} else {
		strcpy(buff, "link status: unlink\n");
	}

	ret = sprintf(buf, "%s\n", buff);

	return ret;
}

int auto_cali(void)
{
	unsigned int value;
	int I1, I2, I3, I4, I5;
	int count = 99;
	char problem[20] = {0};
	char path[20] = {0};
	int cali_rise = 0;
	int cali_sel = 0;

	pr_info("auto test cali\n");
	for (cali_sel = 0; cali_sel < 4; cali_sel++) {
		readl(PREG_ETH_REG1);
		strcpy(problem, "no clock delay");
		value = readl(PREG_ETH_REG0) & (~(0x1f << 25));
		writel(value, PREG_ETH_REG0);

		I1 = 0;
		I2 = 0;
		I3 = 0;
		I4 = 0;
		I5 = 0;

		for (cali_rise = 0; cali_rise <= 1; cali_rise++) {
			count = 99;
			value = (readl(PREG_ETH_REG0) | (1 << 25) |
				(cali_rise << 26) | (cali_sel << 27));
			writel(value, PREG_ETH_REG0);
			while (count >= 0) {
				value = readl(PREG_ETH_REG1);
				if ((value >> 15) & 0x1) {
					count--;
					switch (value & 0x1f) {
					case 0x0:
						I1++;
						break;
					case 0x1:
						I2++;
						break;
					case 0x2:
						I3++;
						break;
					case 0x3:
						I4++;
						break;
					case 0x4:
						I5++;
						break;
					}
				}
			}
		pr_info(" I1 = %d; I2 = %d; I3 = %d; I4 = %d; I5 = %d;\n",
			I1, I2, I3, I4, I5);

		if (
			(I1 > 0) && (I2 > 0) && (I3 > 0) &&
			(I4 > 0) && (I5 > 0))
			strcpy(problem, "clock delay");

		pr_info(" RXDATA Line %d have %s problem\n",
			cali_sel, problem);

		strcpy(path, (I2 + I1 + I3) > (I5 + I4 + I3) ?
			"positive" : "opposite");

		if (!strncmp(problem, "clock delay", 11))
			pr_info("Need debug to  delay %s direction\n", path);
		}
	}
	return 0;
}

static int am_net_cali(int argc, char **argv, int gate)
{
	unsigned int value;
	int cali_rise = 0;
	int cali_sel = 0;
	int cali_start = 0;
	int cali_time = 0;
	int ii = 0;
	int r = 0;

	cali_start = gate;
	if (gate == 3) {
		auto_cali();
		return 0;
	}

	if (
		(argc < 4) || (!argv) || (!argv[0]) ||
		(!argv[1]) || (!argv[2]) || (!argv[3])) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	r = kstrtoint(argv[1], 0, &cali_rise);
	r = kstrtoint(argv[2], 0, &cali_sel);
	r = kstrtoint(argv[3], 0, &cali_time);

	readl(PREG_ETH_REG1);
	value = readl(PREG_ETH_REG0) & (~(0x1f << 25));
	writel(value, PREG_ETH_REG0);

	value = readl(PREG_ETH_REG0) | (cali_start << 25) |
		(cali_rise << 26) | (cali_sel << 27);
	writel(value, PREG_ETH_REG0);

	pr_info("rise :%d   sel: %d  time: %d   start:%d  cbus2050 = %x\n",
		cali_rise, cali_sel, cali_time, cali_start,
			readl(PREG_ETH_REG0));
	for (ii = 0; ii < cali_time; ii++) {
		mdelay(100);
		value = readl(PREG_ETH_REG1);
		if ((value >> 15) & 0x1) {
			pr_info
			("value = %x,len = %d,idx = %d,sel=%d,rise = %d\n",
			value, (value >> 5) & 0x1f, (value & 0x1f),
			(value >> 11) & 0x7, (value >> 14) & 0x1);
			ii++;
		}
	}

	return 0;
}

static ssize_t eth_cali_store(
	struct class *class, struct class_attribute *attr,
	const char *buf, size_t count)
{
	int argc;
	char *buff, *p, *para;
	char *argv[5];
	char cmd;

	buff = kstrdup(buf, GFP_KERNEL);
	p = buff;
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M8B) {
		pr_err("Sorry ,this cpu is not support cali!\n");
		goto end;
	}
	for (argc = 0; argc < 5; argc++) {
		para = strsep(&p, " ");
		if (!para)
			break;
		argv[argc] = para;
	}

	if (argc < 1 || argc > 4)
		goto end;

	cmd = argv[0][0];
		switch (cmd) {
		case 'e':
		case 'E':
			am_net_cali(argc, argv, 1);
			break;
		case 'd':
		case 'D':
			am_net_cali(argc, argv, 0);
			break;
		case 'a':
		case 'A':
			am_net_cali(argc, argv, 3);
			break;
		default:
			goto end;
		}
		return count;
end:
	kfree(buff);
	return 0;
}

#define DRIVER_NAME "ethernet"
static struct class *phy_sys_class;
static CLASS_ATTR(phyreg, 0644, eth_phyreg_help, eth_phyreg_func);
static CLASS_ATTR(macreg, 0644, eth_macreg_help, eth_macreg_func);
static CLASS_ATTR(linkspeed, 0444, eth_linkspeed_show, NULL);
static CLASS_ATTR(cali, 0644, NULL, eth_cali_store);

int gmac_create_sysfs(struct phy_device *phydev, void __iomem *ioaddr)
{
	int r;
	int t;
	int ret;

	c_phy_dev  = phydev;
	c_ioaddr = ioaddr;
	dev_set_drvdata(&phydev->mdio.dev, phydev);
	for (t = 0; t < ARRAY_SIZE(phy_reg_attrs); t++) {
		r = device_create_file(&phydev->mdio.dev, &phy_reg_attrs[t]);
		if (r) {
			dev_err(&phydev->mdio.dev, "failed to create sysfs file\n");
			return r;
		}
	}
	phy_sys_class = class_create(THIS_MODULE, DRIVER_NAME);
	ret = class_create_file(phy_sys_class, &class_attr_phyreg);
	ret = class_create_file(phy_sys_class, &class_attr_macreg);
	ret = class_create_file(phy_sys_class, &class_attr_linkspeed);
	ret = class_create_file(phy_sys_class, &class_attr_cali);
	return 0;
}

int gmac_remove_sysfs(struct phy_device *phydev)
{
	int t;

	for (t = 0; t < ARRAY_SIZE(phy_reg_attrs); t++)
		device_remove_file(&phydev->mdio.dev, &phy_reg_attrs[t]);
	class_destroy(phy_sys_class);
	c_phy_dev = NULL;
	return 0;
}
#endif
