// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ELE Random Number Generator Driver NXP's Platforms
 *
 * Copyright 2023 NXP
 */

#include "ele_common.h"
#include "ele_fw_api.h"

struct ele_trng {
	struct hwrng rng;
	struct device *dev;
};

int ele_trng_init(struct device *dev)
{
	struct ele_trng *trng;
	int ret;

	trng = devm_kzalloc(dev, sizeof(*trng), GFP_KERNEL);
	if (!trng)
		return -ENOMEM;

	trng->dev         = dev;
	trng->rng.name    = "ele-trng";
	trng->rng.read    = ele_get_hwrng;
	trng->rng.priv    = (unsigned long)trng;
	trng->rng.quality = 1024;

	dev_dbg(dev, "registering ele-trng\n");

	ret = devm_hwrng_register(dev, &trng->rng);
	if (ret)
		return ret;

	dev_info(dev, "Successfully registered ele-trng\n");
	return 0;
}

int ele_get_hwrng(struct hwrng *rng,
		  void *data, size_t len, bool wait)
{
	struct ele_trng *trng = (struct ele_trng *)rng->priv;

	return ele_get_random(trng->dev, data, len);
}
