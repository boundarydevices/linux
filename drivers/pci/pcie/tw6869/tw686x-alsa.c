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

#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <linux/module.h>

#include "tw686x.h"
#include "tw686x-device.h"
#include "tw686x-reg.h"

MODULE_DESCRIPTION("alsa driver module for tw6869 based DVR cards");
MODULE_AUTHOR("liran <jli11@intersil.com>");
MODULE_LICENSE("GPL");

/*
 * PCM structure
 */

typedef struct snd_card_tw686x_pcm {
	struct tw686x_adev *dev;

	spinlock_t lock;

	struct snd_pcm_substream *substream;
} snd_card_tw686x_pcm_t;


/*
 * tw686x audio DMA IRQ handler
 *
 *   Called whenever we get an tw686x audio interrupt
 *   Handles shifting between the 2 buffers, manages the read counters,
 *   and notifies ALSA when periods elapse
 *
 */

static void tw686x_alsa_advance_one_period(struct tw686x_adev *dev)
{
  dev->read_offset = dev->period_idx * dev->blksize;
  dev->period_idx ++;
  if (dev->period_idx == dev->blocks)
    dev->period_idx = 0;

  // configure the next DMA buffer
  // period_idx - 1, just transferred,
  // period_idx,     being transferred
  // period_idx + 1, next buffer
  dev->dma_blk += dev->blksize;
  if (dev->period_idx + 1 == dev->blocks)
    dev->dma_blk -= dev->bufsize;
}

void tw686x_alsa_irq(struct tw686x_adev *dev, u32 dma_status, u32 pb_status)
{
  int pb = (pb_status>>(dev->channel_id+TW686X_AUDIO_BEGIN)) & 1;

  //daprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, pb);

  /* next block addr */

  if(dev->running)
  {
      /* update status & wake waiting readers */
      tw686x_alsa_advance_one_period(dev);

      if (dev->pb_flag != pb)
      {
        daprintk(1, dev, "pb flag mismatch %d\n", pb);

        tw686x_dev_set_adma_buffer (dev, dev->dma_blk, dev->pb_flag);
        tw686x_alsa_advance_one_period(dev);
        dev->pb_flag = 1 - dev->pb_flag;

        snd_pcm_period_elapsed(dev->substream);
      }

      tw686x_dev_set_adma_buffer (dev, dev->dma_blk, dev->pb_flag);
      dev->pb_flag = 1 - dev->pb_flag;

      snd_pcm_period_elapsed(dev->substream);
  }
}

int tw686x_alsa_resetdma(struct tw686x_chip *chip, u32 dma_cmd)
{
    struct tw686x_adev *dev = NULL;
	int i=0;

	for(i=0; i<chip->aud_count; i++) {
		if(dma_cmd & (1<<(TW686X_AUDIO_BEGIN+i))) {
            dev = chip->aud_dev[i];
            dev->pb_flag = 0;
            tw686x_dev_run_audio( dev, true );
		}
	}

    return 0;
}

/*
 * ALSA capture trigger
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called whenever a capture is started or stopped. Must be defined,
 *   but there's nothing we want to do here
 *
 */

static int snd_card_tw686x_capture_trigger(struct snd_pcm_substream * substream,
					  int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_card_tw686x_pcm_t *pcm = runtime->private_data;
	struct tw686x_adev *dev=pcm->dev;
	int err = 0;
    unsigned long flags;

    daprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, (cmd == SNDRV_PCM_TRIGGER_START));

//liran for test	spin_lock(dev->slock);      //!!! this lock may cause dead lock
    spin_lock_irqsave(dev->slock, flags);
	if (cmd == SNDRV_PCM_TRIGGER_START) {
		/* start dma */
		dev->pb_flag = 0;

        tw686x_dev_set_audio(dev->chip, runtime->rate, runtime->sample_bits, runtime->channels, dev->blksize);

		tw686x_dev_run_audio(dev, true);
		dev->running = true;
	} else if (cmd == SNDRV_PCM_TRIGGER_STOP) {
		/* stop dma */
		tw686x_dev_run_audio(dev, false);
		dev->running = false;
	} else {
		err = -EINVAL;
	}
    spin_unlock_irqrestore(dev->slock, flags);
//liran for test	spin_unlock(dev->slock);

	return err;
}

/*
 * DMA buffer initialization
 *
 *   Uses V4L functions to initialize the DMA. Shouldn't be necessary in
 *  ALSA, but I was unable to use ALSA's own DMA, and had to force the
 *  usage of V4L's
 *
 *   - Copied verbatim from saa7134-oss.
 *
 */

static int dsp_buffer_init(struct tw686x_adev *dev)
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

static int dsp_buffer_free(struct tw686x_adev *dev)
{
	BUG_ON(!dev->blksize);

	videobuf_dma_free(&dev->dma);

	dev->blocks  = 0;
	dev->blksize = 0;
	dev->bufsize = 0;

	return 0;
}

/*
 * ALSA PCM preparation
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called right after the capture device is opened, this function configures
 *  the buffer using the previously defined functions, allocates the memory,
 *  sets up the hardware registers, and then starts the DMA. When this function
 *  returns, the audio should be flowing.
 *
 */

static int snd_card_tw686x_capture_prepare(struct snd_pcm_substream * substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_card_tw686x_t *card_tw686x = snd_pcm_substream_chip(substream);
	struct tw686x_adev *dev = card_tw686x->dev;
	snd_card_tw686x_pcm_t *pcm = runtime->private_data;

    daprintk(DPRT_LEVEL0, dev, "%s(%d, %d, %d)\n", __func__, runtime->rate, runtime->channels, runtime->sample_bits);

	pcm->dev->substream = substream;

    //liran:: moved this to trigger function, to avoid the audio format changed by system when some channels are capturing.
    //tw686x_dev_set_audio(dev->chip, runtime->rate, runtime->sample_bits, runtime->channels, dev->blksize);

	dev->rate = runtime->rate;

	return 0;
}

/*
 * ALSA pointer fetching
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called whenever a period elapses, it must return the current hardware
 *  position of the buffer.
 *   Also resets the read counter used to prevent overruns
 *
 */

static snd_pcm_uframes_t
snd_card_tw686x_capture_pointer(struct snd_pcm_substream * substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_card_tw686x_pcm_t *pcm = runtime->private_data;
	struct tw686x_adev *dev=pcm->dev;

    //daprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, dev->read_offset);

	return bytes_to_frames(runtime, dev->read_offset);
}

/*
 * ALSA hardware capabilities definition
 */

static struct snd_pcm_hardware snd_card_tw686x_capture =
{
	.info =     (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID),
  .formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S8,
  .rates = SNDRV_PCM_RATE_KNOT,
  .rate_min = 8000,
  .rate_max = 48000,
  .channels_min = 1,
  .channels_max = 1,
  .buffer_bytes_max =	4096,
  .period_bytes_min =	256,
  .period_bytes_max =	1024,
  .periods_min      = 4,
  .periods_max      = 8,
};

static void snd_card_tw686x_runtime_free(struct snd_pcm_runtime *runtime)
{
	snd_card_tw686x_pcm_t *pcm = runtime->private_data;
	struct tw686x_adev *dev = pcm->dev;

    daprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	kfree(pcm);
}


/*
 * ALSA hardware params
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called on initialization, right before the PCM preparation
 *
 */

static int snd_card_tw686x_hw_params(struct snd_pcm_substream * substream,
				      struct snd_pcm_hw_params * hw_params)
{
	snd_card_tw686x_t *card_tw686x = snd_pcm_substream_chip(substream);
	struct tw686x_adev *dev = card_tw686x->dev;
	unsigned int period_size, periods;
	struct scatterlist *list;
	int err;

	period_size = params_period_bytes(hw_params);
	periods = params_periods(hw_params);

	if (period_size < 0x100 || period_size > 0x10000)
		return -EINVAL;
	if (periods < 2)
		return -EINVAL;
	if (period_size * periods > 1024 * 1024)
		return -EINVAL;

    daprintk(DPRT_LEVEL0, dev, "%s(bufsize=%d)\n", __func__, period_size * periods);

	if (dev->blocks == periods &&
	    dev->blksize == period_size)
		return 0;

	/* release the old buffer */
	if (substream->runtime->dma_area) {
#if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
		videobuf_dma_unmap(&dev->chip->pci->dev, &dev->dma);
#else
		videobuf_sg_dma_unmap(&dev->chip->pci->dev, &dev->dma);
#endif
		dsp_buffer_free(dev);
		substream->runtime->dma_area = NULL;
	}

    dev->pb_flag    = 0;
    dev->period_idx = 0;
	dev->blocks  = periods;
	dev->blksize = period_size;
	dev->bufsize = period_size * periods;
	dev->running = false;

	err = dsp_buffer_init(dev);
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
		dsp_buffer_free(dev);
		return err;
	}

    list = dev->dma.sglist;

    daprintk(DPRT_LEVEL0, dev, "%s_%d: period_size %d, periods %d, sglen %d.\n",
         __func__,
         dev->channel_id,
         period_size,
         periods,
         dev->dma.sglen);

    dev->dma_blk = cpu_to_le32(sg_dma_address(list) - list->offset);
    tw686x_dev_set_adma_buffer (dev, dev->dma_blk, 0);

    dev->dma_blk += dev->blksize;
    tw686x_dev_set_adma_buffer (dev, dev->dma_blk, 1);

	/* I should be able to use runtime->dma_addr in the control
	   byte, but it doesn't work. So I allocate the DMA using the
	   V4L functions, and force ALSA to use that as the DMA area */

#if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
	substream->runtime->dma_area = dev->dma.vaddr;
#else
	substream->runtime->dma_area = dev->dma.vmalloc;
#endif
	substream->runtime->dma_bytes = dev->bufsize;
	substream->runtime->dma_addr = 0;

	return 0;
}

/*
 * ALSA hardware release
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called after closing the device, but before snd_card_tw686x_capture_close
 *   It stops the DMA audio and releases the buffers.
 *
 */

static int snd_card_tw686x_hw_free(struct snd_pcm_substream * substream)
{
	snd_card_tw686x_t *card_tw686x = snd_pcm_substream_chip(substream);
	struct tw686x_adev *dev;

	dev = card_tw686x->dev;

    daprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	if (substream->runtime->dma_area) {
#if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
		videobuf_dma_unmap(&dev->chip->pci->dev, &dev->dma);
#else
		videobuf_sg_dma_unmap(&dev->chip->pci->dev, &dev->dma);
#endif
		dsp_buffer_free(dev);
		substream->runtime->dma_area = NULL;
	}

	return 0;
}

/*
 * ALSA capture finish
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called after closing the device.
 *
 */

static int snd_card_tw686x_capture_close(struct snd_pcm_substream * substream)
{
	snd_card_tw686x_t *card_tw686x = snd_pcm_substream_chip(substream);
	struct tw686x_adev *dev = card_tw686x->dev;

    daprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	if (card_tw686x->mute_was_on) {
	}
	return 0;
}

/*
 * ALSA capture start
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called when opening the device. It creates and populates the PCM
 *  structure
 *
 */

static int snd_card_tw686x_capture_open(struct snd_pcm_substream * substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_card_tw686x_pcm_t *pcm;
	snd_card_tw686x_t *card_tw686x = snd_pcm_substream_chip(substream);
	struct tw686x_adev *dev;
	int err;

	if (!card_tw686x) {
		printk(KERN_ERR "BUG: tw686x can't find device struct."
				" Can't proceed with open\n");
		return -ENODEV;
	}
	dev = card_tw686x->dev;

	daprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	mutex_lock(&dev->lock);

	dev->read_offset = 0;

	mutex_unlock(&dev->lock);

	pcm = kzalloc(sizeof(*pcm), GFP_KERNEL);
	if (pcm == NULL)
		return -ENOMEM;

	pcm->dev=dev;

	spin_lock_init(&pcm->lock);

	pcm->substream = substream;
	runtime->private_data = pcm;
	runtime->private_free = snd_card_tw686x_runtime_free;
	runtime->hw = snd_card_tw686x_capture;

	err = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		return err;

	err = snd_pcm_hw_constraint_step(runtime, 0,
						SNDRV_PCM_HW_PARAM_PERIODS, 2);
	if (err < 0)
		return err;

	return 0;
}

/*
 * page callback (needed for mmap)
 */

static struct page *snd_card_tw686x_page(struct snd_pcm_substream *substream,
					unsigned long offset)
{
	void *pageptr = substream->runtime->dma_area + offset;

	snd_card_tw686x_t *card_tw686x = snd_pcm_substream_chip(substream);
	struct tw686x_adev *dev = card_tw686x->dev;

	daprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	return vmalloc_to_page(pageptr);
}

/*
 * ALSA capture callbacks definition
 */

static struct snd_pcm_ops snd_card_tw686x_capture_ops = {
	.open =			snd_card_tw686x_capture_open,
	.close =		snd_card_tw686x_capture_close,
	.ioctl =		snd_pcm_lib_ioctl,
	.hw_params =	snd_card_tw686x_hw_params,
	.hw_free =		snd_card_tw686x_hw_free,
	.prepare =		snd_card_tw686x_capture_prepare,
	.trigger =		snd_card_tw686x_capture_trigger,
	.pointer =		snd_card_tw686x_capture_pointer,
	.page =			snd_card_tw686x_page,
};

/*
 * ALSA PCM setup
 *
 *   Called when initializing the board. Sets up the name and hooks up
 *  the callbacks
 *
 */

static int snd_card_tw686x_pcm(snd_card_tw686x_t *card_tw686x, long device)
{
	struct snd_pcm *pcm;
	int err;

	daprintk(DPRT_LEVEL0, card_tw686x->dev, "%s()\n", __func__);

	if ((err = snd_pcm_new(card_tw686x->card, "TW6869 PCM", device, 0, 1, &pcm)) < 0)
		return err;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_card_tw686x_capture_ops);
	pcm->private_data = card_tw686x;
	pcm->info_flags = 0;
	strcpy(pcm->name, "TW6869 PCM");

	return 0;
}

static void snd_tw686x_free(struct snd_card * card)
{
    snd_card_tw686x_t *card_tw686x = (snd_card_tw686x_t*)card->private_data;
    struct tw686x_adev* dev = card_tw686x->dev;

    daprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);
}

/*
 * ALSA initialization
 *
 *   Called by the init routine, once for each tw686x device present,
 *  it creates the basic structures and registers the ALSA devices
 *
 */

int tw686x_alsa_create(struct tw686x_adev *dev)
{

	struct snd_card *card = NULL;
	snd_card_tw686x_t *card_tw686x;
	int err;

	daprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	err = snd_card_create(-1, NULL, THIS_MODULE,
			      sizeof(snd_card_tw686x_t), &card);
	if (err < 0)
		return err;
#else
	card = snd_card_new(-2, NULL, THIS_MODULE, sizeof(snd_card_tw686x_t));
	if (card == NULL)
		return -ENOMEM;
#endif

	strcpy(card->driver, "TW6869");

	/* Card "creation" */

	card->private_free = snd_tw686x_free;
	card_tw686x = (snd_card_tw686x_t *) card->private_data;

	spin_lock_init(&card_tw686x->lock);

	card_tw686x->dev = dev;
	card_tw686x->card= card;
	dev->card = card_tw686x;

	mutex_init(&dev->lock);

	if ((err = snd_card_tw686x_pcm(card_tw686x, 0)) < 0)
		goto __nodev;

	snd_card_set_dev(card, &dev->chip->pci->dev);

	/* End of "creation" */

	strcpy(card->shortname, "TW6869");
	sprintf(card->longname, "%s at 0x%p irq %d",
		dev->chip->name, dev->chip->bmmio, dev->chip->pci->irq);

	daprintk(1, dev, "alsa: %s registered as card %d\n",card->longname,dev->channel_id);

	err = snd_card_register(card);
	if (!err)
		return 0;

__nodev:
	snd_card_free(card);
	return err;
}

int tw686x_alsa_free(struct tw686x_adev *dev)
{
    if(dev->card) {
        snd_card_free(dev->card->card);
        dev->card = NULL;
    }

	return 1;
}
