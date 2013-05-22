/*
 * fsl_asrc.h - ALSA ASRC interface
 *
 * Copyright (C) 2013 Freescale Semiconductor, Inc.  This file is licensed
 * under the terms of the GNU General Public License version 2.  This
 * program is licensed "as is" without any warranty of any kind, whether
 * express or implied.
 */

#ifndef _FSL_ASRC_P2P_H
#define _FSL_ASRC_P2P_H

#include <linux/mxc_asrc.h>
#include "imx-pcm.h"

struct imx_asrc_p2p {
	int output_rate;
	int output_width;
	struct asrc_p2p_ops asrc_ops;

	struct imx_pcm_dma_params dma_params_rx;
	struct imx_pcm_dma_params dma_params_tx;

	enum asrc_pair_index asrc_index;
	struct dma_async_tx_descriptor  *asrc_p2p_desc;
	struct dma_chan                 *asrc_p2p_dma_chan;
	struct imx_dma_data              asrc_p2p_dma_data;

	char name[32];
};

#endif
