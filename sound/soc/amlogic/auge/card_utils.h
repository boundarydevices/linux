/*
 * sound/soc/amlogic/auge/card_utils.h
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

#ifndef __AML_CARD_CORE_H
#define __AML_CARD_CORE_H

#include <sound/soc.h>
#include "audio_utils.h"

struct aml_dai {
	const char *name;
	unsigned int sysclk;
	int slots;
	int slot_width;
	unsigned int tx_slot_mask;
	unsigned int rx_slot_mask;
	struct clk *clk;
};

int aml_card_parse_daifmt(struct device *dev,
				struct device_node *node,
				struct device_node *codec,
				char *prefix,
				unsigned int *retfmt);
__printf(3, 4)
int aml_card_set_dailink_name(struct device *dev,
				struct snd_soc_dai_link *dai_link,
				const char *fmt, ...);
int aml_card_parse_card_name(struct snd_soc_card *card,
				char *prefix);

#define aml_card_parse_clk_cpu(node, dai_link, aml_dai)		\
	aml_card_parse_clk(node, dai_link->cpu_of_node, aml_dai)
#define aml_card_parse_clk_codec(node, dai_link, aml_dai)		\
	aml_card_parse_clk(node, dai_link->codec_of_node, aml_dai)
int aml_card_parse_clk(struct device_node *node,
				struct device_node *dai_of_node,
				struct aml_dai *aml_dai);

#define aml_card_parse_cpu(node, dai_link,				\
			list_name, cells_name, is_single_link)	\
	aml_card_parse_dai(node, &dai_link->cpu_of_node,		\
		&dai_link->cpu_dai_name, list_name, cells_name, is_single_link)
#define aml_card_parse_codec(node, dai_link, list_name, cells_name)	\
	aml_card_parse_dai(node, &dai_link->codec_of_node,		\
		&dai_link->codec_dai_name, list_name, cells_name, NULL)
#define aml_card_parse_platform(node, dai_link, list_name, cells_name)	\
	aml_card_parse_dai(node, &dai_link->platform_of_node,		\
		NULL, list_name, cells_name, NULL)
int aml_card_parse_dai(struct device_node *node,
				struct device_node **endpoint_np,
				const char **dai_name,
				const char *list_name,
				const char *cells_name,
				int *is_single_links);
int aml_card_parse_codec_confs(struct device_node *codec_np,
				struct snd_soc_card *card);

int aml_card_init_dai(struct snd_soc_dai *dai,
				struct aml_dai *aml_dai, bool cont_clk);

int aml_card_canonicalize_dailink(struct snd_soc_dai_link *dai_link);
void aml_card_canonicalize_cpu(struct snd_soc_dai_link *dai_link,
				int is_single_links);

int aml_card_clean_reference(struct snd_soc_card *card);

extern int aml_card_add_controls(struct snd_soc_card *card);

#endif /* __AML_CARD_CORE_H */
