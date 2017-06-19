/*
 * drivers/amlogic/media/common/codec_mm/configs/configs_test.c
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

#include <linux/amlogic/media/codec_mm/configs.h>

static struct mconfig_node media;
static struct mconfig_node vdec;
static struct mconfig_node decoder;
static struct mconfig_node parser;
static struct mconfig_node codec;
static struct mconfig_node fast;
static struct mconfig_node fast2;


static  struct mconfig vdec_configs[] = {
	MC_I32("vdec", 1),
	MC_I32("vdec1", 1),
	MC_I32("vdec2", 2),
	MC_I32("vdec3", 3),
	MC_I32("vdec4", 4),

};

static struct mconfig decoder_configs[] = {
	MC_I32("264", 1),
	MC_I32("265", 1),
	MC_U32("vp9", 2),
	MC_U32("vc1", 3),
	MC_U32("rmvb", 4),
};



static struct mconfig parser_configs[] = {
	MC_I32("ts", 1),
	MC_I32("es", 1),
	MC_I32("demux", 2),
	MC_U32("rm", 3),
	MC_U32("ps", 4),
};

static struct mconfig vdec_profile[] = {
	MC_CSTR("265", "compress,di,c,mmu"),
	MC_CSTR("264",  "compress,di,c,4k"),
	MC_CSTR("263",  "compress,di,c"),
	MC_CSTR("vc1",  "compress,di,c"),
	MC_CSTR("rm",  "compress,di,di,ps"),
};
static int a, b, c, d, e;
struct mconfig fast_profile[] = {
	MC_PI32("265", &a),
	MC_PI32("264",  &b),
	MC_PI32("263",  &c),
	MC_PI32("vc1",  &d),
	MC_PI32("rm",  &e),
};

int dump_set(const char *trigger, int id, const char *buf, int size)
{
	pr_err("trigger-->[%s]\n", buf);
	return size;
}


struct mconfig fast2_profile[] = {
	MC_FUN("trigger", NULL, &dump_set),
};






static int config_test(void)
{
	static int init;

	if (init > 0)
		return 0;
	init++;
	configs_init_new_node(&media, "debug", CONFIG_FOR_RW);
	configs_register_node(NULL, &media);

	configs_init_new_node(&vdec, "vdec", CONFIG_FOR_RW);
	configs_register_node(&media, &vdec);
	REG_CONFIGS(&vdec, vdec_configs);

	configs_init_new_node(&decoder, "decoder", CONFIG_FOR_RW);
	REG_CONFIGS(&decoder, decoder_configs);
	configs_register_node(&media, &decoder);

	configs_init_new_node(&parser, "parser", CONFIG_FOR_W);
	REG_CONFIGS(&parser, parser_configs);
	configs_register_node(&media, &parser);

	configs_init_new_node(&codec, "codec", CONFIG_FOR_R);
	REG_CONFIGS(&codec, vdec_profile);
	configs_register_node(&media, &codec);

	configs_init_new_node(&fast, "fast", CONFIG_FOR_RW);
	REG_CONFIGS(&fast, fast_profile);
	configs_register_node(&media, &fast);

	configs_init_new_node(&fast2, "trigger", CONFIG_FOR_W);
	REG_CONFIGS(&fast2, fast2_profile);
	configs_register_node(&media, &fast2);

	/* configs_register_configs(); */
	return 0;
}
int config_dump(void *buf, int size)
{
	config_test();
	b++;
	c = b+3;
	a++;
	d += 2;
	return configs_list_nodes(NULL, buf, size,
		LIST_MODE_FULL_CMDVAL_ALL);
}
int configs_config_setstr(const char *buf)
{
	int ret;

	config_test();
	pr_info("-----------start configs_config_setstr\n\n");
	ret = configs_set_path_valonpath(buf);
	pr_info("-----------end configs_config_setstr\n\n");
	return ret;
}

