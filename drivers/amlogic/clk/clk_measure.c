/*
 * drivers/amlogic/clk/clk_measure.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>

#undef pr_fmt
#define pr_fmt(fmt) "clkmsr: " fmt

void __iomem *msr_clk_reg0;
void __iomem *msr_clk_reg2;
void __iomem *msr_clk_reg3;

#define CLKMSR_DEVICE_NAME	"clkmsr"
unsigned int clk_msr_index = 0xff;

static unsigned int gxbb_clk_util_clk_msr(unsigned int clk_mux)
{
	unsigned int  msr;
	unsigned int regval = 0;
	unsigned int val;

	writel_relaxed(0, msr_clk_reg0);
    /* Set the measurement gate to 50uS */
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~0xFFFF)) | (64-1);
	writel_relaxed(val, msr_clk_reg0);
    /* Disable continuous measurement */
    /* disable interrupts */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~((1<<18)|(1<<17)));
	writel_relaxed(val, msr_clk_reg0);
	val = readl_relaxed(msr_clk_reg0);
	val = (val & (~(0x7f<<20))) | (clk_mux<<20)|(1<<19)|(1<<16);
	writel_relaxed(val, msr_clk_reg0);
    /* Wait for the measurement to be done */
	do {
		regval = readl_relaxed(msr_clk_reg0);
	} while (regval & (1 << 31));
    /* disable measuring */
	val = readl_relaxed(msr_clk_reg0);
	val = val & (~(1<<16));
	msr = (readl_relaxed(msr_clk_reg2)+31)&0x000FFFFF;
    /* Return value in MHz*measured_val */
	return (msr>>6)*1000000;

}

int gxl_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[82] = "Cts_ge2d_clk       ",
		[81] = "Cts_vapbclk        ",
		[80] = "Rng_ring_osc_clk[3]",
		[79] = "Rng_ring_osc_clk[2]",
		[78] = "Rng_ring_osc_clk[1]",
		[77] = "Rng_ring_osc_clk[0]",
		[76] = "cts_aoclk_int      ",
		[75] = "cts_aoclkx2_int    ",
		[74] = "0                  ",
		[73] = "cts_pwm_C_clk      ",
		[72] = "cts_pwm_D_clk      ",
		[71] = "cts_pwm_E_clk      ",
		[70] = "cts_pwm_F_clk      ",
		[69] = "0                  ",
		[68] = "0                  ",
		[67] = "0                  ",
		[66] = "cts_vid_lock_clk   ",
		[65] = "0                  ",
		[64] = "0                  ",
		[63] = "0                  ",
		[62] = "cts_hevc_clk       ",
		[61] = "gpio_clk_msr       ",
		[60] = "alt_32k_clk        ",
		[59] = "cts_hcodec_clk     ",
		[58] = "0                  ",
		[57] = "0					",
		[56] = "0					",
		[55] = "vid_pll_div_clk_out	",
		[54] = "0					",
		[53] = "Sd_emmc_clk_A		",
		[52] = "Sd_emmc_clk_B		",
		[51] = "Cts_nand_core_clk	",
		[50] = "Mp3_clk_out			",
		[49] = "mp2_clk_out			",
		[48] = "mp1_clk_out			",
		[47] = "ddr_dpll_pt_clk		",
		[46] = "cts_vpu_clk			",
		[45] = "cts_pwm_A_clk		",
		[44] = "cts_pwm_B_clk		",
		[43] = "fclk_div5			",
		[42] = "mp0_clk_out			",
		[41] = "eth_rx_clk_or_clk_rmii",
		[40] = "cts_pcm_mclk			",
		[39] = "cts_pcm_sclk			",
		[38] = "cts_vdin_meas_clk			",
		[37] = "cts_clk_i958			",
		[36] = "cts_hdmi_tx_pixel_clk ",
		[35] = "cts_mali_clk			",
		[34] = "0					",
		[33] = "0					",
		[32] = "cts_vdec_clk			",
		[31] = "MPLL_CLK_TEST_OUT	",
		[30] = "0					",
		[29] = "0					",
		[28] = "cts_sar_adc_clk					   ",
		[27] = "0					   ",
		[26] = "sc_clk_int			   ",
		[25] = "0					   ",
		[24] = "0					   ",
		[23] = "HDMI_CLK_TODIG		   ",
		[22] = "eth_phy_ref_clk		   ",
		[21] = "i2s_clk_in_src0		   ",
		[20] = "rtc_osc_clk_out		   ",
		[19] = "cts_hdmitx_sys_clk	   ",
		[18] = "sys_cpu_clk_div16		   ",
		[17] = "sys_pll_div16					   ",
		[16] = "cts_FEC_CLK_2		   ",
		[15] = "cts_FEC_CLK_1		   ",
		[14] = "cts_FEC_CLK_0		   ",
		[13] = "cts_amclk			   ",
		[12] = "Cts_pdm_clk			   ",
		[11] = "rgmii_tx_clk_to_phy	   ",
		[10] = "cts_vdac_clk			   ",
		[9] = "cts_encl_clk			  ",
		[8] = "cts_encp_clk			  ",
		[7] = "clk81					  ",
		[6] = "cts_enci_clk			  ",
		[5] = "0						  ",
		[4] = "gp0_pll_clk			  ",
		[3] = "A53_ring_osc_clk		  ",
		[2] = "am_ring_osc_clk_out_ee[2]",
		[1] = "am_ring_osc_clk_out_ee[1]",
		[0] = "am_ring_osc_clk_out_ee[0]",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, gxbb_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", gxbb_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;
	return 0;
}
int gxm_clk_measure(struct seq_file *s, void *what, unsigned int index)
{
	static const char * const clk_table[] = {
		[82] = "Cts_ge2d_clk       ",
		[81] = "Cts_vapbclk        ",
		[80] = "Rng_ring_osc_clk[3]",
		[79] = "Rng_ring_osc_clk[2]",
		[78] = "Rng_ring_osc_clk[1]",
		[77] = "Rng_ring_osc_clk[0]",
		[76] = "cts_aoclk_int      ",
		[75] = "cts_aoclkx2_int    ",
		[74] = "0                  ",
		[73] = "cts_pwm_C_clk      ",
		[72] = "cts_pwm_D_clk      ",
		[71] = "cts_pwm_E_clk      ",
		[70] = "cts_pwm_F_clk      ",
		[69] = "0                  ",
		[68] = "0                  ",
		[67] = "0                  ",
		[66] = "cts_vid_lock_clk   ",
		[65] = "0                  ",
		[64] = "0                  ",
		[63] = "0                  ",
		[62] = "cts_hevc_clk       ",
		[61] = "gpio_clk_msr       ",
		[60] = "alt_32k_clk        ",
		[59] = "cts_hcodec_clk     ",
		[58] = "cts_wave420l_bclk                  ",
		[57] = "cts_wave420l_cclk	",
		[56] = "cts_cci_clk			",
		[55] = "vid_pll_div_clk_out	",
		[54] = "0					",
		[53] = "Sd_emmc_clk_A		",
		[52] = "Sd_emmc_clk_B		",
		[51] = "Cts_nand_core_clk	",
		[50] = "Mp3_clk_out			",
		[49] = "mp2_clk_out			",
		[48] = "mp1_clk_out			",
		[47] = "ddr_dpll_pt_clk		",
		[46] = "cts_vpu_clk			",
		[45] = "cts_pwm_A_clk		",
		[44] = "cts_pwm_B_clk		",
		[43] = "fclk_div5			",
		[42] = "mp0_clk_out			",
		[41] = "eth_rx_clk_or_clk_rmii",
		[40] = "cts_pcm_mclk			",
		[39] = "cts_pcm_sclk			",
		[38] = "cts_vdin_meas_clk			",
		[37] = "cts_clk_i958			",
		[36] = "cts_hdmi_tx_pixel_clk ",
		[35] = "cts_mali_clk			",
		[34] = "0					",
		[33] = "0					",
		[32] = "cts_vdec_clk			",
		[31] = "MPLL_CLK_TEST_OUT	",
		[30] = "0					",
		[29] = "0					",
		[28] = "cts_sar_adc_clk					   ",
		[27] = "0					   ",
		[26] = "sc_clk_int			   ",
		[25] = "0					   ",
		[24] = "sys_cpu1_clk_div16			",
		[23] = "HDMI_CLK_TODIG		   ",
		[22] = "eth_phy_ref_clk		   ",
		[21] = "i2s_clk_in_src0		   ",
		[20] = "rtc_osc_clk_out		   ",
		[19] = "cts_hdmitx_sys_clk	   ",
		[18] = "sys_cpu_clk_div16		   ",
		[17] = "sys_pll_div16					   ",
		[16] = "cts_FEC_CLK_2		   ",
		[15] = "cts_FEC_CLK_1		   ",
		[14] = "cts_FEC_CLK_0		   ",
		[13] = "cts_amclk			   ",
		[12] = "Cts_pdm_clk			   ",
		[11] = "mac_eth_tx_clk	   ",
		[10] = "cts_vdac_clk			   ",
		[9] = "cts_encl_clk			  ",
		[8] = "cts_encp_clk			  ",
		[7] = "clk81					  ",
		[6] = "cts_enci_clk			  ",
		[5] = "0						  ",
		[4] = "gp0_pll_clk			  ",
		[3] = "A53_ring_osc_clk		  ",
		[2] = "am_ring_osc_clk_out_ee[2]",
		[1] = "am_ring_osc_clk_out_ee[1]",
		[0] = "am_ring_osc_clk_out_ee[0]",
	};
	int  i;
	int len = sizeof(clk_table)/sizeof(char *);

	if (index  == 0xff) {
		for (i = 0; i < len; i++)
			seq_printf(s, "[%2d][%10d]%s\n",
				   i, gxbb_clk_util_clk_msr(i),
					clk_table[i]);
		return 0;
	}
	seq_printf(s, "[%10d]%s\n", gxbb_clk_util_clk_msr(index),
		   clk_table[index]);
	clk_msr_index = 0xff;
	return 0;
}

int  meson_clk_measure(unsigned int clk_mux)
{
	int clk_val;

	switch (get_cpu_type()) {
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
		clk_val = gxbb_clk_util_clk_msr(clk_mux);
	break;
	default:
		pr_info("Unsupported chip clk measure\n");
		clk_val = 0;
	break;
	}
	return clk_val;

}
EXPORT_SYMBOL(meson_clk_measure);

static int dump_clk(struct seq_file *s, void *what)
{
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXL)
		gxl_clk_measure(s, what, clk_msr_index);
	else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXM)
		gxm_clk_measure(s, what, clk_msr_index);
	return 0;
}

static ssize_t clkmsr_write(struct file *file, const char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	/* ret = sscanf(buf, "%i", &clk_msr_index); */
	ret = kstrtouint(buf, 0, &clk_msr_index);
	switch (ret) {
	case 1:
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static int clkmsr_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_clk, inode->i_private);
}

static const struct file_operations clkmsr_file_ops = {
	.open		= clkmsr_open,
	.read		= seq_read,
	.write		= clkmsr_write,
	.llseek		= seq_lseek,
};

static int aml_clkmsr_probe(struct platform_device *pdev)
{
	static struct dentry *debugfs_root;
	struct device_node *np;

	np = pdev->dev.of_node;
	debugfs_root = debugfs_create_dir("aml_clkmsr", NULL);
	if (IS_ERR(debugfs_root) || !debugfs_root) {
		pr_warn("failed to create debugfs directory\n");
		debugfs_root = NULL;
		return -1;
	}

	debugfs_create_file("clkmsr", S_IFREG | 0444,
			    debugfs_root, NULL, &clkmsr_file_ops);

	msr_clk_reg0 = of_iomap(np, 0);
	msr_clk_reg2 = of_iomap(np, 1);
	pr_info("Gxl msr_clk_reg0=%p,msr_clk_reg2=%p\n",
		msr_clk_reg0, msr_clk_reg2);
	return 0;
}

static const struct of_device_id meson_clkmsr_dt_match[] = {
	{	.compatible = "amlogic, gxl_measure",
	},
	{},
};

static struct platform_driver aml_clkmsr_driver = {
	.probe = aml_clkmsr_probe,
	.driver = {
		.name = CLKMSR_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = meson_clkmsr_dt_match,
	},
};

static int __init aml_clkmsr_init(void)
{
	int ret = -1;

	ret = platform_driver_register(&aml_clkmsr_driver);

	if (ret != 0) {
		pr_err("clkmsr:failed to register driver, error %d\n", ret);
		return -ENODEV;
	}
	pr_info("clkmsr: driver init\n");

	return ret;
}

static void __exit aml_clkmsr_exit(void)
{
	platform_driver_unregister(&aml_clkmsr_driver);
}

module_init(aml_clkmsr_init);
module_exit(aml_clkmsr_exit);

MODULE_DESCRIPTION("Amlogic clkmsr module");
MODULE_LICENSE("GPL");

