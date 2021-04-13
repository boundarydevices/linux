/*
 * mt8365-afe-debug.c  --  Mediatek 8365 audio debugfs
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Jia Zeng <jia.zeng@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt8365-afe-debug.h"
#include "mt8365-reg.h"
#include "mt8365-afe-utils.h"
#include "mt8365-afe-common.h"
#include "../common/mtk-base-afe.h"
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/device.h>


#ifdef CONFIG_DEBUG_FS

struct mt8365_afe_debug_fs {
	char *fs_name;
	const struct file_operations *fops;
};

struct afe_dump_reg_attr {
	uint32_t offset;
	char *name;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct afe_dump_reg_attr afe_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON1),
	DUMP_REG_ENTRY(AUDIO_TOP_CON2),
	DUMP_REG_ENTRY(AUDIO_TOP_CON3),
	DUMP_REG_ENTRY(AFE_DAC_CON0),
	DUMP_REG_ENTRY(AFE_DAC_CON1),
	DUMP_REG_ENTRY(AFE_TDM_CON1),
	DUMP_REG_ENTRY(AFE_TDM_CON2),
	DUMP_REG_ENTRY(AFE_TDM_IN_CON1),
	DUMP_REG_ENTRY(AFE_CM1_CON0),
	DUMP_REG_ENTRY(AFE_CM1_CON1),
	DUMP_REG_ENTRY(AFE_CM1_CON2),
	DUMP_REG_ENTRY(AFE_CM1_CON3),
	DUMP_REG_ENTRY(AFE_CM1_CON4),
	DUMP_REG_ENTRY(AFE_CM2_CON0),
	DUMP_REG_ENTRY(AFE_CM2_CON1),
	DUMP_REG_ENTRY(AFE_CM2_CON2),
	DUMP_REG_ENTRY(AFE_CM2_CON3),
	DUMP_REG_ENTRY(AFE_CM2_CON4),
	DUMP_REG_ENTRY(AFE_DMIC0_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC1_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC2_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC3_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_UL_DL_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_TOP_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON1),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_CFG0),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_SYNCWORD_CFG),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_RX_CFG0),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_RX_CFG1),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_RX_CFG2),
	DUMP_REG_ENTRY(AFE_AUD_PAD_TOP),
	DUMP_REG_ENTRY(AFE_I2S_CON),
	DUMP_REG_ENTRY(AFE_I2S_CON1),
	DUMP_REG_ENTRY(AFE_I2S_CON2),
	DUMP_REG_ENTRY(AFE_I2S_CON3),
	DUMP_REG_ENTRY(PCM_INTF_CON1),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_MON0),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_MON1),
	DUMP_REG_ENTRY(AFE_ADDA_UL_SRC_MON0),
	DUMP_REG_ENTRY(AFE_ADDA_UL_SRC_MON1),
};

static const struct afe_dump_reg_attr memif_dump_regs[] = {
	DUMP_REG_ENTRY(AFE_DL1_BASE),
	DUMP_REG_ENTRY(AFE_DL1_CUR),
	DUMP_REG_ENTRY(AFE_DL1_END),
	DUMP_REG_ENTRY(AFE_DL2_BASE),
	DUMP_REG_ENTRY(AFE_DL2_CUR),
	DUMP_REG_ENTRY(AFE_DL2_END),
	DUMP_REG_ENTRY(AFE_HDMI_OUT_BASE),
	DUMP_REG_ENTRY(AFE_HDMI_OUT_CUR),
	DUMP_REG_ENTRY(AFE_HDMI_OUT_END),
	DUMP_REG_ENTRY(AFE_VUL_BASE),
	DUMP_REG_ENTRY(AFE_VUL_CUR),
	DUMP_REG_ENTRY(AFE_VUL_END),
	DUMP_REG_ENTRY(AFE_AWB_BASE),
	DUMP_REG_ENTRY(AFE_AWB_CUR),
	DUMP_REG_ENTRY(AFE_AWB_END),
	DUMP_REG_ENTRY(AFE_VUL3_BASE),
	DUMP_REG_ENTRY(AFE_VUL3_CUR),
	DUMP_REG_ENTRY(AFE_VUL3_END),
	DUMP_REG_ENTRY(AFE_VUL_D2_BASE),
	DUMP_REG_ENTRY(AFE_VUL_D2_CUR),
	DUMP_REG_ENTRY(AFE_VUL_D2_END),
	DUMP_REG_ENTRY(AFE_HDMI_IN_2CH_BASE),
	DUMP_REG_ENTRY(AFE_HDMI_IN_2CH_CUR),
	DUMP_REG_ENTRY(AFE_HDMI_IN_2CH_END),
	DUMP_REG_ENTRY(AFE_MEMIF_PBUF_SIZE),
	DUMP_REG_ENTRY(AFE_MEMIF_PBUF2_SIZE),
	DUMP_REG_ENTRY(AFE_MEMIF_MSB),
	DUMP_REG_ENTRY(AFE_HDMI_OUT_CON0),
	DUMP_REG_ENTRY(AFE_HDMI_IN_2CH_CON0),
};

static const struct afe_dump_reg_attr irq_dump_regs[] = {
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT1),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT2),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT3),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT4),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT5),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT7),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT8),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT10),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT11),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CNT12),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CON),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CON2),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CLR),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_STATUS),
};

static const struct afe_dump_reg_attr conn_dump_regs[] = {
	DUMP_REG_ENTRY(AFE_CONN0),
	DUMP_REG_ENTRY(AFE_CONN1),
	DUMP_REG_ENTRY(AFE_CONN2),
	DUMP_REG_ENTRY(AFE_CONN3),
	DUMP_REG_ENTRY(AFE_CONN4),
	DUMP_REG_ENTRY(AFE_CONN5),
	DUMP_REG_ENTRY(AFE_CONN6),
	DUMP_REG_ENTRY(AFE_CONN7),
	DUMP_REG_ENTRY(AFE_CONN8),
	DUMP_REG_ENTRY(AFE_CONN9),
	DUMP_REG_ENTRY(AFE_CONN10),
	DUMP_REG_ENTRY(AFE_CONN11),
	DUMP_REG_ENTRY(AFE_CONN12),
	DUMP_REG_ENTRY(AFE_CONN13),
	DUMP_REG_ENTRY(AFE_CONN14),
	DUMP_REG_ENTRY(AFE_CONN15),
	DUMP_REG_ENTRY(AFE_CONN14),
	DUMP_REG_ENTRY(AFE_CONN15),
	DUMP_REG_ENTRY(AFE_CONN16),
	DUMP_REG_ENTRY(AFE_CONN17),
	DUMP_REG_ENTRY(AFE_CONN18),
	DUMP_REG_ENTRY(AFE_CONN19),
	DUMP_REG_ENTRY(AFE_CONN20),
	DUMP_REG_ENTRY(AFE_CONN21),
	DUMP_REG_ENTRY(AFE_CONN22),
	DUMP_REG_ENTRY(AFE_CONN23),
	DUMP_REG_ENTRY(AFE_CONN24),
	DUMP_REG_ENTRY(AFE_CONN25),
	DUMP_REG_ENTRY(AFE_CONN26),
	DUMP_REG_ENTRY(AFE_CONN27),
	DUMP_REG_ENTRY(AFE_CONN28),
	DUMP_REG_ENTRY(AFE_CONN29),
	DUMP_REG_ENTRY(AFE_CONN30),
	DUMP_REG_ENTRY(AFE_CONN31),
	DUMP_REG_ENTRY(AFE_CONN32),
	DUMP_REG_ENTRY(AFE_CONN33),
	DUMP_REG_ENTRY(AFE_CONN34),
	DUMP_REG_ENTRY(AFE_CONN35),
	DUMP_REG_ENTRY(AFE_CONN36),
	DUMP_REG_ENTRY(AFE_CONN37),
	DUMP_REG_ENTRY(AFE_CONN38),
	DUMP_REG_ENTRY(AFE_CONN39),
	DUMP_REG_ENTRY(AFE_CONN40),
	DUMP_REG_ENTRY(AFE_CONN41),
	DUMP_REG_ENTRY(AFE_CONN42),
	DUMP_REG_ENTRY(AFE_CONN43),
	DUMP_REG_ENTRY(AFE_CONN44),
	DUMP_REG_ENTRY(AFE_CONN45),
	DUMP_REG_ENTRY(AFE_CONN46),
	DUMP_REG_ENTRY(AFE_CONN47),
	DUMP_REG_ENTRY(AFE_CONN48),
	DUMP_REG_ENTRY(AFE_CONN49),
	DUMP_REG_ENTRY(AFE_CONN50),
	DUMP_REG_ENTRY(AFE_CONN51),
	DUMP_REG_ENTRY(AFE_CONN52),
	DUMP_REG_ENTRY(AFE_CONN53),
	DUMP_REG_ENTRY(AFE_CONN0_1),
	DUMP_REG_ENTRY(AFE_CONN1_1),
	DUMP_REG_ENTRY(AFE_CONN2_1),
	DUMP_REG_ENTRY(AFE_CONN3_1),
	DUMP_REG_ENTRY(AFE_CONN4_1),
	DUMP_REG_ENTRY(AFE_CONN5_1),
	DUMP_REG_ENTRY(AFE_CONN6_1),
	DUMP_REG_ENTRY(AFE_CONN7_1),
	DUMP_REG_ENTRY(AFE_CONN8_1),
	DUMP_REG_ENTRY(AFE_CONN9_1),
	DUMP_REG_ENTRY(AFE_CONN10_1),
	DUMP_REG_ENTRY(AFE_CONN11_1),
	DUMP_REG_ENTRY(AFE_CONN12_1),
	DUMP_REG_ENTRY(AFE_CONN13_1),
	DUMP_REG_ENTRY(AFE_CONN26_1),
	DUMP_REG_ENTRY(AFE_CONN27_1),
	DUMP_REG_ENTRY(AFE_CONN28_1),
	DUMP_REG_ENTRY(AFE_CONN29_1),
	DUMP_REG_ENTRY(AFE_CONN30_1),
	DUMP_REG_ENTRY(AFE_CONN31_1),
	DUMP_REG_ENTRY(AFE_CONN32_1),
	DUMP_REG_ENTRY(AFE_CONN33_1),
	DUMP_REG_ENTRY(AFE_CONN34_1),
	DUMP_REG_ENTRY(AFE_CONN35_1),
	DUMP_REG_ENTRY(AFE_CONN36_1),
	DUMP_REG_ENTRY(AFE_CONN37_1),
	DUMP_REG_ENTRY(AFE_CONN38_1),
	DUMP_REG_ENTRY(AFE_CONN39_1),
	DUMP_REG_ENTRY(AFE_CONN40_1),
	DUMP_REG_ENTRY(AFE_CONN41_1),
	DUMP_REG_ENTRY(AFE_CONN42_1),
	DUMP_REG_ENTRY(AFE_CONN43_1),
	DUMP_REG_ENTRY(AFE_CONN44_1),
	DUMP_REG_ENTRY(AFE_CONN45_1),
	DUMP_REG_ENTRY(AFE_CONN46_1),
	DUMP_REG_ENTRY(AFE_CONN47_1),
	DUMP_REG_ENTRY(AFE_CONN48_1),
	DUMP_REG_ENTRY(AFE_CONN49_1),
	DUMP_REG_ENTRY(AFE_CONN50_1),
	DUMP_REG_ENTRY(AFE_CONN51_1),
	DUMP_REG_ENTRY(AFE_CONN52_1),
	DUMP_REG_ENTRY(AFE_CONN53_1),
	DUMP_REG_ENTRY(AFE_CONN_24BIT),
	DUMP_REG_ENTRY(AFE_CONN_24BIT_1),
	DUMP_REG_ENTRY(AFE_CM2_CONN0),
	DUMP_REG_ENTRY(AFE_CM2_CONN1),
	DUMP_REG_ENTRY(AFE_CM2_CONN2),
	DUMP_REG_ENTRY(AFE_HDMI_CONN0),
	DUMP_REG_ENTRY(AFE_HDMI_CONN1),
	DUMP_REG_ENTRY(AFE_CONN_TDMIN_CON),
};

static const struct afe_dump_reg_attr dbg_dump_regs[] = {
	DUMP_REG_ENTRY(AFE_SGEN_CON0),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON_TDM),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON_TDM_IN),
};

static ssize_t mt8365_afe_dump_registers(char __user *user_buf,
					 size_t count,
					 loff_t *pos,
					 struct mtk_base_afe *afe,
					 const struct afe_dump_reg_attr *regs,
					 size_t regs_len)
{
	ssize_t ret, i;
	char *buf;
	unsigned int reg_value;
	int n = 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mt8365_afe_enable_main_clk(afe);

	for (i = 0; i < regs_len; i++) {
		if (regmap_read(afe->regmap, regs[i].offset, &reg_value))
			n += scnprintf(buf + n, count - n, "%s = N/A\n",
				       regs[i].name);
		else
			n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
				       regs[i].name, reg_value);
	}

	mt8365_afe_disable_main_clk(afe);

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);
	kfree(buf);

	return ret;
}

static ssize_t mt8365_afe_read_file(struct file *file,
				    char __user *user_buf,
				    size_t count,
				    loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8365_afe_dump_registers(user_buf, count, pos, afe,
					afe_dump_regs,
					ARRAY_SIZE(afe_dump_regs));

	return ret;
}

static ssize_t mt8365_afe_write_file(struct file *file,
				     const char __user *user_buf,
				     size_t count,
				     loff_t *pos)
{
	char buf[64];
	size_t buf_size;
	char *start = buf;
	char *reg_str;
	char *value_str;
	const char delim[] = " ,";
	unsigned long reg, value;
	struct mtk_base_afe *afe = file->private_data;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;

	reg_str = strsep(&start, delim);
	if (!reg_str || !strlen(reg_str))
		return -EINVAL;

	value_str = strsep(&start, delim);
	if (!value_str || !strlen(value_str))
		return -EINVAL;

	if (kstrtoul(reg_str, 16, &reg))
		return -EINVAL;

	if (kstrtoul(value_str, 16, &value))
		return -EINVAL;

	mt8365_afe_enable_main_clk(afe);

	regmap_write(afe->regmap, reg, value);

	mt8365_afe_disable_main_clk(afe);

	return buf_size;
}

static ssize_t mt8365_afe_memif_read_file(struct file *file,
					  char __user *user_buf,
					  size_t count,
					  loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8365_afe_dump_registers(user_buf, count, pos, afe,
					memif_dump_regs,
					ARRAY_SIZE(memif_dump_regs));

	return ret;
}

static ssize_t mt8365_afe_irq_read_file(struct file *file,
					char __user *user_buf,
					size_t count,
					loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8365_afe_dump_registers(user_buf, count, pos, afe,
					irq_dump_regs,
					ARRAY_SIZE(irq_dump_regs));

	return ret;
}

static ssize_t mt8365_afe_conn_read_file(struct file *file,
					 char __user *user_buf,
					 size_t count,
					 loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8365_afe_dump_registers(user_buf, count, pos, afe,
					conn_dump_regs,
					ARRAY_SIZE(conn_dump_regs));

	return ret;
}

static ssize_t mt8365_afe_dbg_read_file(struct file *file,
					char __user *user_buf,
					size_t count,
					loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8365_afe_dump_registers(user_buf, count, pos, afe,
					dbg_dump_regs,
					ARRAY_SIZE(dbg_dump_regs));

	return ret;
}

static const struct file_operations mt8365_afe_fops = {
	.open = simple_open,
	.read = mt8365_afe_read_file,
	.write = mt8365_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8365_afe_memif_fops = {
	.open = simple_open,
	.read = mt8365_afe_memif_read_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8365_afe_irq_fops = {
	.open = simple_open,
	.read = mt8365_afe_irq_read_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8365_afe_conn_fops = {
	.open = simple_open,
	.read = mt8365_afe_conn_read_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8365_afe_dbg_fops = {
	.open = simple_open,
	.read = mt8365_afe_dbg_read_file,
	.llseek = default_llseek,
};

static const
struct mt8365_afe_debug_fs afe_debug_fs[MT8365_AFE_DEBUGFS_NUM] = {
	{"mtksocaudio", &mt8365_afe_fops},
	{"mtksocaudiomemif", &mt8365_afe_memif_fops},
	{"mtksocaudioirq", &mt8365_afe_irq_fops},
	{"mtksocaudioconn", &mt8365_afe_conn_fops},
	{"mtksocaudiodbg", &mt8365_afe_dbg_fops},
};

#endif

void mt8365_afe_init_debugfs(struct mtk_base_afe *afe)
{
#ifdef CONFIG_DEBUG_FS
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	int i;

	for (i = 0; i < ARRAY_SIZE(afe_debug_fs); i++) {
		afe_priv->debugfs_dentry[i] =
			debugfs_create_file(afe_debug_fs[i].fs_name,
				0644, NULL, afe, afe_debug_fs[i].fops);
		if (!afe_priv->debugfs_dentry[i])
			dev_warn(afe->dev, "%s create %s debugfs failed\n",
				 __func__, afe_debug_fs[i].fs_name);
	}
#endif
}

void mt8365_afe_cleanup_debugfs(struct mtk_base_afe *afe)
{
#ifdef CONFIG_DEBUG_FS
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	int i;

	if (!afe_priv)
		return;

	for (i = 0; i < MT8365_AFE_DEBUGFS_NUM; i++)
		debugfs_remove(afe_priv->debugfs_dentry[i]);
#endif
}
