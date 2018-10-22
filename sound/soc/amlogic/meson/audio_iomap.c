/*
 * sound/soc/amlogic/meson/audio_iomap.c
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
#undef pr_fmt
#define pr_fmt(fmt) "audio_iomap: " fmt

#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_address.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/audio_iomap.h>

#define DEV_NAME	"meson_snd_iomap"

/*
 * compatible for chipset except txlx/txhd
 * 'cause of sound reg is managed by cbus operation before, so reg is fixed
 * in audin_reg.h and aiu_reg.h, for compatibility, adjust the base in dts
 *
 * in default, if snd_iomap configed in dts, we use iomap, else we use cbus.
 */
static struct audio_iomap v_aml_snd_iomap = {
	.use_iomap = false,
};

static int aml_snd_read(u32 base_type, unsigned int reg, int *val)
{
	int ret;

	if (v_aml_snd_iomap.use_iomap) {
		if (base_type < IO_BASE_MAX) {
			*val = readl(
				(v_aml_snd_iomap.reg_map[base_type]
				+ (reg << 2)));

			return 0;
		}
		ret = -1;
	} else {
		*val = aml_read_cbus(reg);
		if (*val == -1) {
			pr_err("read cbus reg %x error\n", reg);
			return -1;
		}
		ret = 0;
	}

	return ret;
}

static void aml_snd_write(u32 base_type, unsigned int reg, unsigned int val)
{
	if (v_aml_snd_iomap.use_iomap) {
		if (base_type < IO_BASE_MAX) {
			writel(val,
				(v_aml_snd_iomap.reg_map[base_type]
				+ (reg << 2)));

			return;
		}
		pr_err("write snd reg %x error\n", reg);
	} else {
		aml_write_cbus(reg, val);
	}
}

static void aml_snd_update_bits(u32 base_type,
			unsigned int reg, unsigned int mask,
			unsigned int val)
{
	if (v_aml_snd_iomap.use_iomap) {
		if (base_type < IO_BASE_MAX) {
			unsigned int tmp, orig;

			if (aml_snd_read(base_type, reg, &orig) == 0) {
			tmp = orig & ~mask;
			tmp |= val & mask;
			aml_snd_write(base_type, reg, tmp);

			return;
				}
		}
		pr_err("write snd reg %x error\n", reg);
	} else
		aml_cbus_update_bits(reg, mask, val);

}

int aml_audin_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_AUDIN_BASE, reg, &val);

	if (ret) {
		pr_err("read audin reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_audin_read);

void aml_audin_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_AUDIN_BASE, reg, val);
}
EXPORT_SYMBOL(aml_audin_write);

void aml_audin_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_AUDIN_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_audin_update_bits);

int aml_aiu_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_AIU_BASE, reg, &val);

	if (ret) {
		pr_err("read aiu reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_aiu_read);

void aml_aiu_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_AIU_BASE, reg, val);
}
EXPORT_SYMBOL(aml_aiu_write);

void aml_aiu_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_AIU_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_aiu_update_bits);

int aml_eqdrc_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_EQDRC_BASE, reg, &val);

	if (ret) {
		pr_err("read eqdrc reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_eqdrc_read);

void aml_eqdrc_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_EQDRC_BASE, reg, val);
}
EXPORT_SYMBOL(aml_eqdrc_write);

void aml_eqdrc_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_EQDRC_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_eqdrc_update_bits);

int aml_hiu_reset_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_HIU_RESET_BASE, reg, &val);

	if (ret) {
		pr_err("read hiu reset reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_hiu_reset_read);

void aml_hiu_reset_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_HIU_RESET_BASE, reg, val);
}
EXPORT_SYMBOL(aml_hiu_reset_write);

void aml_hiu_reset_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_HIU_RESET_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_hiu_reset_update_bits);

int aml_isa_read(unsigned int reg)
{
	int ret, val = 0;

	ret = aml_snd_read(IO_ISA_BASE, reg, &val);

	if (ret) {
		pr_err("read ISA reg %x error %d\n", reg, ret);
		return -1;
	}
	return val;
}
EXPORT_SYMBOL(aml_isa_read);

void aml_isa_write(unsigned int reg, unsigned int val)
{
	aml_snd_write(IO_ISA_BASE, reg, val);
}
EXPORT_SYMBOL(aml_isa_write);

void aml_isa_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val)
{
	aml_snd_update_bits(IO_ISA_BASE, reg, mask, val);
}
EXPORT_SYMBOL(aml_isa_update_bits);

static int snd_iomap_probe(struct platform_device *pdev)
{
	struct resource res;
	struct device_node *np, *child;
	int i = 0;
	int ret = 0;

	np = pdev->dev.of_node;
	for_each_child_of_node(np, child) {
		if (of_address_to_resource(child, 0, &res)) {
			ret = -1;
			pr_err("%s could not get resource, use in default",
				__func__);
			break;
		}
		v_aml_snd_iomap.reg_map[i] =
			ioremap(res.start, resource_size(&res));
		pr_info("v_aml_snd_iomap.reg_map[%d], reg:%x, size:%x\n",
			i, (u32)res.start, (u32)resource_size(&res));

		if (!v_aml_snd_iomap.use_iomap)
			v_aml_snd_iomap.use_iomap = true;

		i++;
	}
	pr_info("amlogic %s probe done\n", DEV_NAME);

	return ret;
}

static const struct of_device_id snd_iomap_dt_match[] = {
	{ .compatible = "amlogic, meson-snd-iomap" },
	{},
};

static  struct platform_driver snd_iomap_platform_driver = {
	.probe		= snd_iomap_probe,
	.driver		= {
		.owner		    = THIS_MODULE,
		.name		    = DEV_NAME,
		.of_match_table	= snd_iomap_dt_match,
	},
};

int __init meson_snd_iomap_init(void)
{
	int ret;

	ret = platform_driver_register(&snd_iomap_platform_driver);

	return ret;
}
core_initcall(meson_snd_iomap_init);
