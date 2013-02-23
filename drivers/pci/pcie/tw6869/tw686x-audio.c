/*
 *
 * Driver for Intersil|Techwell TW6869 based DVR cards
 *
 * (c) 2011-12 liran <jli11@intersil.com> [Intersil|Techwell China]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Thanks to yiliang for variable audio packet length and more audio
 *  formats support.
 */

#include <linux/module.h>
#include "tw686x.h"
#include "tw686x-device.h"
#include "tw686x-reg.h"

#define TW686X_AUDIO_PERIOD_SIZE    4096

MODULE_DESCRIPTION("audio driver module for tw6869 based DVR cards");
MODULE_AUTHOR("liran <jli11@intersil.com>");
MODULE_LICENSE("GPL");

static int tw686x_audio_buffer_init(struct tw686x_adev *dev)
{
	int err;

	BUG_ON(!dev->bufsize);

	videobuf_dma_init(&dev->dma);
	err = videobuf_dma_init_kernel(&dev->dma, PCI_DMA_FROMDEVICE,
				       (dev->bufsize + PAGE_SIZE) >> PAGE_SHIFT);
	return err;
}

/*
 * DMA buffer release
 *
 *   Called after closing the device, during snd_card_tw686x_capture_close
 *
 */

static int tw686x_audio_buffer_free(struct tw686x_adev *dev)
{
	BUG_ON(!dev->blksize);

	videobuf_dma_free(&dev->dma);

	dev->blocks  = 0;
	dev->blksize = 0;
	dev->bufsize = 0;

	return 0;
}

//static
int tw686x_audio_trigger(struct tw686x_adev *dev, int cmd)
{
	unsigned int dma_addr;
	struct scatterlist *list;

    daprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, cmd);

    if(cmd)
    {
        dev->pb_flag    = 0;

        list = dev->dma.sglist + dev->dma_blk;

        dma_addr = cpu_to_le32(sg_dma_address(list) - list->offset);
        tw686x_dev_set_adma_buffer (dev, dma_addr, 0);

        dev->dma_blk++;
        if(dev->dma_blk >= dev->blocks)
        {
            dev->dma_blk = 0;
        }

        list = dev->dma.sglist + dev->dma_blk;
        dma_addr = cpu_to_le32(sg_dma_address(list) - list->offset);
        tw686x_dev_set_adma_buffer (dev, dma_addr, 1);

        dev->dma_blk++;
        if(dev->dma_blk >= dev->blocks)
        {
            dev->dma_blk = 0;
        }
    }

    /* start dma */
    dev->substream = (struct snd_pcm_substream*)(NULL+cmd);
    tw686x_dev_run_audio(dev, cmd>0);

	return 0;
}

static void tw686x_audio_advance_one_period(struct tw686x_adev *dev)
{
    dev->dma_blk++;
    if(dev->dma_blk >= dev->blocks)
        dev->dma_blk = 0;

    dev->period_idx++;
    if(dev->period_idx >= dev->blocks)
    {
        tw686x_audio_trigger(dev, 0);
    }
}

void tw686x_audio_irq(struct tw686x_adev *dev, u32 dma_status, u32 pb_status)
{
    int pb = (pb_status>>(dev->channel_id+TW686X_AUDIO_BEGIN)) & 1;
	unsigned int dma_addr;
	struct scatterlist *list;

    daprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, pb);

    /* next block addr */

    /* update status & wake waiting readers */
    tw686x_audio_advance_one_period(dev);

    list = dev->dma.sglist + dev->dma_blk;
    dma_addr = cpu_to_le32(sg_dma_address(list) - list->offset);

    if (dev->pb_flag != pb)
    {
        daprintk(1, dev, "pb flag mismatch\n");

        tw686x_dev_set_adma_buffer (dev, dma_addr, dev->pb_flag);
        tw686x_audio_advance_one_period(dev);
        dev->pb_flag = 1 - dev->pb_flag;

        list = dev->dma.sglist + dev->dma_blk;
        dma_addr = cpu_to_le32(sg_dma_address(list) - list->offset);

        //snd_pcm_period_elapsed(dev->substream);
    }

    tw686x_dev_set_adma_buffer (dev, dma_addr, dev->pb_flag);
    dev->pb_flag = 1 - dev->pb_flag;

    //snd_pcm_period_elapsed(dev->substream);
}

int tw686x_audio_param(struct tw686x_adev *dev, struct tw686x_aparam* ap)
{
	unsigned long flags;
    daprintk(DPRT_LEVEL0, dev, "%s(%d %d %d %d)\n", __func__, ap->sample_rate, ap->sample_bits, ap->channels, ap->blksize);

	spin_lock_irqsave(dev->slock, flags);

    dev->period_idx  = 0;
    dev->read_offset = 0;
    dev->substream   = NULL;
    dev->dma_blk     = 0;

    if(ap->blksize < 0)
    {
        tw686x_audio_trigger(dev, 0);
    }
    else
    {
        ap->blksize = min(ap->blksize, TW686X_AUDIO_PERIOD_SIZE);
        tw686x_dev_set_audio(dev->chip, ap->sample_rate, ap->sample_bits, ap->channels, ap->blksize);
        dev->blksize = ap->blksize;
    }

	spin_unlock_irqrestore(dev->slock, flags);

	return 0;
}

int tw686x_audio_data(struct tw686x_adev *dev, struct tw686x_adata *ad)
{
	unsigned long flags;
	struct scatterlist *list;

	spin_lock_irqsave(dev->slock, flags);

    if(dev->period_idx > 0)
    {
        list = dev->dma.sglist + dev->read_offset;
        ad->len = min(ad->len, (int)dev->blksize);
        memcpy(ad->data,  sg_virt(list), ad->len);

        dev->period_idx--;
        dev->read_offset ++;
        if (dev->read_offset >= dev->blocks)
            dev->read_offset = 0;
    }
    else
    {
        ad->len = 0;
    }
    if((dev->period_idx<(dev->blocks-1)) && (dev->substream==NULL))
    {
        tw686x_audio_trigger(dev, 1);
    }

	spin_unlock_irqrestore(dev->slock, flags);

	return 0;
}

int tw686x_audio_resetdma(struct tw686x_chip *chip, u32 dma_cmd)
{
#if(0)
    struct tw686x_adev *dev = NULL;
	int i=0;

	for(i=0; i<chip->aud_count; i++) {
		if(dma_cmd & (1<<(TW686X_AUDIO_BEGIN+i))) {
            dev = chip->aud_dev[i];
            dev->period_idx  = 0;
            dev->read_offset = 0;
            dev->substream   = NULL;
            dev->dma_blk     = 0;
            tw686x_audio_trigger( dev, 1 );
		}
	}
#endif
    return 0;
}

int tw686x_audio_create(struct tw686x_adev *dev)
{
	unsigned int period_size, periods;
	int err;

	daprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	mutex_init(&dev->lock);

	period_size = TW686X_AUDIO_PERIOD_SIZE; //固定为4096
	periods = 4;        //最小为2

    daprintk(DPRT_LEVEL0, dev, "%s(bufsize=%d)\n", __func__, period_size * periods);

	dev->blocks  = periods;
	dev->blksize = period_size;
	dev->bufsize = period_size * periods;
	dev->period_idx  = 0;
	dev->read_offset = 0;
	dev->substream   = NULL;
	dev->card = NULL;

	err = tw686x_audio_buffer_init(dev);
	if (0 != err) {
		dev->blocks  = 0;
		dev->blksize = 0;
		dev->bufsize = 0;
		return err;
	}

#if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
	if (0 != (err = videobuf_dma_map(&dev->chip->pci->dev, &dev->dma))) {
#else
	if (0 != (err = videobuf_sg_dma_map(&dev->chip->pci->dev, &dev->dma))) {
#endif
		tw686x_audio_buffer_free(dev);
		return err;
	}

    daprintk(DPRT_LEVEL0, dev, "%s_%d: period_size %d, periods %d, sglen %d.\n",
         __func__,
         dev->channel_id,
         period_size,
         periods,
         dev->dma.sglen);

    return 0;
}

int tw686x_audio_free(struct tw686x_adev *dev)
{
	if (dev->substream != NULL) {
#if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
		videobuf_dma_unmap(&dev->chip->pci->dev, &dev->dma);
#else
		videobuf_sg_dma_unmap(&dev->chip->pci->dev, &dev->dma);
#endif
		tw686x_audio_buffer_free(dev);
		dev->substream = NULL;
	}

	return 0;
}
