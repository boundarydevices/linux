// SPDX-License-Identifier: GPL-2.0
/*
 * mtk-soundcard-driver.c  --  Mediatek soundcard driver common
 *
 * Copyright (c) 2022 Baylibre SAS.
 * Author: Nicolas Belin <nbelin@baylibre.com>
 */

#include <linux/of.h>
#include <sound/soc.h>
#include <linux/module.h>

#include "mtk-soundcard-driver.h"

int set_card_codec_info(struct snd_soc_card *card)
{
	struct snd_soc_dai_link_component *dai_link_codecs, *dlc;
	struct device_node *dl_node, *c_node;
	struct device *dev = card->dev;
	struct of_phandle_args args;
	const char *dai_link_name;
	int num_codecs;
	int ret, i;

	/* Loop over all the dai link sub nodes*/
	for_each_available_child_of_node(dev->of_node, dl_node) {
		if (of_property_read_string(dl_node, "dai-link-name",
					    &dai_link_name))
			return -EINVAL;

		num_codecs = of_get_child_count(dl_node);

		dev_dbg(dev, "dai_link_name %s, num_codecs %d\n", dai_link_name, num_codecs);

		/* Allocate the snd_soc_dai_link_component array that will be
		 * used to dynamically add the list of codecs to the static
		 * snd_soc_dai_link array.
		 */
		dai_link_codecs = devm_kcalloc(dev, num_codecs,
					       sizeof(*dai_link_codecs),
					       GFP_KERNEL);
		if (!dai_link_codecs)
			return -ENOMEM;

		dlc = dai_link_codecs;

		/* Loop over all the codec sub nodes for this dai link */
		for_each_child_of_node(dl_node, c_node) {
			/* Retrieve the node and the dai_name that are used
			 * by the soundcard.
			 */
			ret = of_parse_phandle_with_args(c_node, "sound-dai",
							 "#sound-dai-cells", 0,
							 &args);
			if (ret) {
				if (ret != -EPROBE_DEFER)
					dev_err(dev,
						"can't parse dai %d\n", ret);
				return ret;
			}
			dlc->of_node = args.np;
			ret =  snd_soc_get_dai_name(&args, &dlc->dai_name);
			if (ret) {
				of_node_put(c_node);
				return ret;
			}

			dev_dbg(dev, "dlc->dai_name %s\n", dlc->dai_name);

			dlc++;
		}

		/* Update the snd_soc_dai_link static array with the codecs
		 * we have just found.
		 */
		for (i = 0; i < card->num_links; i++) {
			if (!strcmp(dai_link_name, card->dai_link[i].name)) {
				card->dai_link[i].num_codecs = num_codecs;
				card->dai_link[i].codecs = dai_link_codecs;
				break;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(set_card_codec_info);

MODULE_DESCRIPTION("Mediatek soundcard driver common");
MODULE_AUTHOR("Nicolas Belin <nbelin@baylibre.com>");
MODULE_LICENSE("GPL v2");

