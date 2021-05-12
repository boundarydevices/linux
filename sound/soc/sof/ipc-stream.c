// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license.  When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Authors: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>

/* Intel-specific SOF IPC code */

#include <linux/device.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/types.h>

#include <sound/pcm.h>
#include <sound/sof/stream.h>

#include "ops.h"
#include "sof-audio.h"
#include "sof-priv.h"

struct sof_pcm_stream {
	size_t posn_offset;
};

/* Mailbox-based Intel IPC implementation */
void sof_ipc_msg_data(struct snd_sof_dev *sdev,
		      struct snd_sof_pcm_stream *sps,
		      void *p, size_t sz)
{
	if (!sps || !sdev->stream_box.size) {
		sof_mailbox_read(sdev, sdev->dsp_box.offset, p, sz);
	} else {
		struct snd_pcm_substream *substream = sps->substream;
		struct snd_compr_stream *cstream = sps->cstream;
		struct sof_pcm_stream *pstream;
		struct sof_compr_stream *sstream;
		size_t posn_offset;

		if (substream) {
			pstream = substream->runtime->private_data;
			posn_offset = pstream->posn_offset;
		} else {
			sstream = cstream->runtime->private_data;
			posn_offset = sstream->posn_offset;
		}
		/* The stream might already be closed */
		if (pstream || sstream)
			sof_mailbox_read(sdev, posn_offset, p, sz);
	}
}
EXPORT_SYMBOL(sof_ipc_msg_data);

int sof_ipc_pcm_params(struct snd_sof_dev *sdev,
		       struct snd_pcm_substream *substream,
		       const struct sof_ipc_pcm_params_reply *reply)
{
	struct sof_pcm_stream *stream = substream->runtime->private_data;
	size_t posn_offset = reply->posn_offset;

	/* check if offset is overflow or it is not aligned */
	if (posn_offset > sdev->stream_box.size ||
	    posn_offset % sizeof(struct sof_ipc_stream_posn) != 0)
		return -EINVAL;

	stream->posn_offset = sdev->stream_box.offset + posn_offset;

	dev_dbg(sdev->dev, "pcm: stream dir %d, posn mailbox offset is %zu",
		substream->stream, stream->posn_offset);

	return 0;
}
EXPORT_SYMBOL(sof_ipc_pcm_params);

int sof_stream_pcm_open(struct snd_sof_dev *sdev,
			struct snd_pcm_substream *substream)
{
	struct sof_pcm_stream *stream = kmalloc(sizeof(*stream), GFP_KERNEL);

	if (!stream)
		return -ENOMEM;

	/* binding pcm substream to hda stream */
	substream->runtime->private_data = stream;

	return 0;
}
EXPORT_SYMBOL(sof_stream_pcm_open);

int sof_stream_pcm_close(struct snd_sof_dev *sdev,
		    struct snd_pcm_substream *substream)
{
	struct sof_pcm_stream *stream = substream->runtime->private_data;

	substream->runtime->private_data = NULL;
	kfree(stream);

	return 0;
}
EXPORT_SYMBOL(sof_stream_pcm_close);

MODULE_LICENSE("Dual BSD/GPL");
