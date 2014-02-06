/*
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
#define DEBUG
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include "TW68.h"
#include "TW68_defines.h"


MODULE_DESCRIPTION("alsa driver module for tw68 PCIe capture chip");
MODULE_AUTHOR("Simon Xu");
MODULE_LICENSE("GPL");

/*
 * Main chip structure
 */
struct snd_card_tw68 {
	struct snd_card *card;
	spinlock_t mixer_lock;

	struct pci_dev *pci;
	struct TW68_dev *dev;

	struct snd_pcm  *TW68_pcm;
	struct snd_pcm_substream *substream[10];
	spinlock_t lock;
	u32 audio_dma_size;
	u32 audio_rate;
	u32 audio_frame_bits;
	u32 audio_lock_mask;
	u32 last_audio_PB;
	u8 period_insert[8];
};


// static struct snd_card *snd_TW68_cards[SNDRV_CARDS];

static long TW68_audio_nPCM = 0;

/*
 * ALSA hardware capabilities definition
 */

static struct snd_pcm_hardware snd_card_tw68_pcm_hw =
{
	.info =     (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S8,
	.rates = SNDRV_PCM_RATE_8000_48000,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 1,
	.buffer_bytes_max = AUDIO_CH_BUF_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = 4096,
	.periods_min = 4,
	.periods_max = 256,
};


/*
 * TW68 audio DMA IRQ handler
 *
 *   Called whenever we get an TW68 audio interrupt
 *   Handles shifting between the 2 buffers, manages the read counters,
 *   and notifies ALSA when periods elapse
 *
 */

void TW68_alsa_irq(struct TW68_dev *dev, u32 dma_status, u32 pb_status)
{
	u32 dma_base = dev->m_AudioBuffer.dma;
	struct snd_card_tw68 *ctw = (struct snd_card_tw68 *)dev->card->private_data;
	u32 mask = ((dev->videoDMA_ID & dma_status) & 0xff00) >> 8;
	u32 change = pb_status ^ ctw->last_audio_PB;

	//	pr_debug("%s:  card pcm %p  dma_status %x  pb_status %x \n", __func__,
	//		ctw->TW68_pcm, dma_status, pb_status);
	while (mask) {
		int k = __fls(mask);
		u32 dma_ch = dma_base + AUDIO_CH_BUF_SIZE * k;
		int period = ctw->period_insert[k];
		u32 reg_addr = DMA_CH8_CONFIG_P + k * 2;
		struct snd_pcm_substream *substream = ctw->substream[k];
		u32 buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
		u32 smask = (1 << (k + 8));
		u32 offset = ctw->audio_dma_size * period;

		mask &= ~(1 << k);
		if (!(ctw->last_audio_PB & smask))
			reg_addr++;
		reg_writel(reg_addr, dma_ch + offset);
		period++;
		offset += ctw->audio_dma_size;
		if (offset >= buffer_bytes) {
			period = 0;
			offset = 0;
		}

		if (!(change & smask)) {
			reg_addr ^= 1;
			reg_writel(reg_addr, dma_ch + offset);
			period++;
			offset += ctw->audio_dma_size;
			if (offset >= buffer_bytes)
				period = 0;
			pr_debug("%s: 2 periods elapsed\n", __func__);
		}
		ctw->period_insert[k] = period;
		ctw->last_audio_PB ^= change & smask;
		snd_pcm_period_elapsed(substream);
	}
}


/*
 * ALSA capture trigger
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called whenever a capture is started or stopped. Must be defined,
 *
 *
 */

static int snd_card_TW68_capture_trigger(struct snd_pcm_substream *substream, int cmd)
{
	// DMA start bit based on ss id
	u32 mask = (1 << (substream->number + 8));
	u32 dwReg;
	u32 dwRegF;
	struct snd_card_tw68 *ctw = snd_pcm_substream_chip(substream);
	struct TW68_dev *dev = ctw->dev;
	int start;
	unsigned long flags;

	if (!ctw) {
		pr_err("%s: can't find device struct.\n", __func__);
		return -ENODEV;
	}

	//pr_debug("%s: cmd=%d ctw 0x%p\n", __func__, cmd, ctw);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		start = 1;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		start = 0;
		break;

	default:
		pr_debug("%s: cmd %d not supported\n", __func__, cmd);
		return -EINVAL;
	}
	spin_lock_irqsave(&dev->slock, flags);
	dwReg = reg_readl(DMA_CHANNEL_ENABLE);
	dwRegF = reg_readl(DMA_CMD);
	if (start) {
		dev->videoDMA_ID |= mask;
		dev->videoCap_ID |= mask;
		dwReg |= mask;
		dwRegF |= mask | (1<<31);
	} else {
		dev->videoDMA_ID &= ~mask;
		dev->videoCap_ID &= ~mask;
		dwReg &= ~mask;
		dwRegF &= ~mask;
	}
	reg_writel(DMA_CHANNEL_ENABLE, dwReg);
	reg_writel(DMA_CMD, dwRegF);
	spin_unlock_irqrestore(&dev->slock, flags);
//	pr_debug("%s: cmd %X  DMA_CHANNEL_ENABLE 0x%x   0x%x\n", __func__, cmd, dwReg, dwRegF);
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
 *  Audio sample rate bits setup
 */

static int snd_card_TW68_capture_prepare(struct snd_pcm_substream * substream)
{
	int k = substream->number;
	u32 dma_ch;
	u32 pb_status;
	u32 mask = (1 << (k + 8));
	u32 sr = substream->runtime->rate;
	u32 frame_bits = substream->runtime->frame_bits;
	u32 audio_dma_size;
	struct snd_card_tw68 *ctw = snd_pcm_substream_chip(substream);
	struct TW68_dev *dev;
	__u32 __iomem *chbase;
	u32 reg_addr = DMA_CH8_CONFIG_P + k * 2;

	if (!ctw) {
		pr_err("%s: can't find device struct.\n", __func__);
		return -ENODEV;
	}
	dev = ctw->dev;
	chbase = dev->lmmio + ((k & 4) * 0x40);
	writel(readl(chbase + POWER_DOWN_CTRL) & ~0x90, chbase + POWER_DOWN_CTRL);
	audio_dma_size = snd_pcm_lib_period_bytes(substream);
	ctw->audio_lock_mask &= ~mask;
	if (((ctw->audio_dma_size == audio_dma_size) &&
			(ctw->audio_rate == sr) &&
			(ctw->audio_frame_bits == frame_bits)) ||
			!ctw->audio_lock_mask) {
		u64 q;
		u32 ctrl1, ctrl2, ctrl3;

		ctw->audio_dma_size = audio_dma_size;
		ctw->audio_rate = sr;
		ctw->audio_frame_bits = frame_bits;
		ctw->audio_lock_mask |= mask;

		q = (u64)125000000 << 16;
		do_div(q, sr);
		ctrl2 = (u32)q;
		ctrl1 = (ctw->audio_dma_size << 19) | (((ctrl2 >> 16) & 0x3fff) << 5) | (1 << 0);
		ctrl3 = reg_readl(AUDIO_CTRL3);
		if (frame_bits == 8)
			ctrl3 |= 0x100;
		else
			ctrl3 &= ~0x100;
		reg_writel(AUDIO_CTRL1, ctrl1);
		reg_writel(AUDIO_CTRL2, ctrl2);
		reg_writel(AUDIO_CTRL3, ctrl3);
		pr_debug("%s: AUDIO_CTRL1: 0x%x 0x%x\n", __func__, reg_readl(AUDIO_CTRL1), ctrl1);
		pr_debug("%s: AUDIO_CTRL2: 0x%x 0x%x\n", __func__, reg_readl(AUDIO_CTRL2), ctrl2);
		pr_debug("%s: AUDIO_CTRL3: 0x%x 0x%x\n", __func__, reg_readl(AUDIO_CTRL3), ctrl3);
		pr_debug("%s: audio_dma_size=0x%x lock=0x%x\n",
			__func__, ctw->audio_dma_size, ctw->audio_lock_mask);
	} else {
		pr_err("%s: can't change size=%x %x, rate=%d %d frame_bits %d %d lock=%x\n",
			__func__, ctw->audio_dma_size, audio_dma_size,
			ctw->audio_rate, sr,
			ctw->audio_frame_bits, frame_bits, ctw->audio_lock_mask);
		return -EINVAL;
	}

	dma_ch = dev->m_AudioBuffer.dma + AUDIO_CH_BUF_SIZE * k;
	pb_status = reg_readl(DMA_PB_STATUS) & mask;
	ctw->last_audio_PB &= ~mask;
	ctw->last_audio_PB |= pb_status;
	if (!pb_status)
		reg_addr++;
	reg_writel(reg_addr, dma_ch);
	reg_addr ^= 1;
	reg_writel(reg_addr, dma_ch + ctw->audio_dma_size);
	ctw->period_insert[k] = 2;

	writel(0xc0, chbase + AUDIO_DET_PERIOD);
	writel(0, chbase + AUDIO_DET_THRESHOLD1);
	writel(0, chbase + AUDIO_DET_THRESHOLD2);
	pr_debug("%s: dev %p  substream->runtime->rate %d\n", __func__, dev, sr);
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

static snd_pcm_uframes_t snd_card_TW68_capture_pointer(
		struct snd_pcm_substream * substream)
{
	struct snd_card_tw68 *ctw = snd_pcm_substream_chip(substream);
	int period = ctw->period_insert[substream->number];
	snd_pcm_uframes_t frames;

	period -= 2;
	if (period < 0)
		period += substream->runtime->periods;
	frames = period * ctw->audio_dma_size;
	if (ctw->audio_frame_bits != 8)
		frames >>= 1;
	return frames;
}

/*
 * ALSA hardware params
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called on initialization, right before the PCM preparation
 *
 */

static int snd_card_TW68_hw_params(struct snd_pcm_substream * substream,
				      struct snd_pcm_hw_params * hw_params)
{
	pr_debug("%s\n", __func__);
	return 0;
}

/*
 * ALSA hardware release
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called after closing the device, but before snd_card_TW68_capture_close
 *   It stops the DMA audio and releases the buffers.
 *
 */

static int snd_card_TW68_hw_free(struct snd_pcm_substream * substream)
{
	pr_debug("%s\n", __func__);
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

static int snd_card_TW68_capture_close(struct snd_pcm_substream * substream)
{
	int k = substream->number;
	u32 mask = (1 << (k + 8));
	struct snd_card_tw68 *ctw = snd_pcm_substream_chip(substream);

	pr_debug("%s\n", __func__);
	ctw->audio_lock_mask &= ~mask;
	return 0;
}

/*
 * ALSA capture start
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called when opening the device. It creates and populates the PCM structure
 *
 */

static int snd_card_TW68_capture_open(struct snd_pcm_substream * substream)
{
	int k = substream->number;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_card_tw68 *ctw = snd_pcm_substream_chip(substream);
	struct TW68_dev *dev;
	int err;

	if (!ctw) {
		pr_err("%s: can't find device struct\n", __func__);
		return -ENODEV;
	}
	dev = ctw->dev;

	runtime->hw = snd_card_tw68_pcm_hw;
	runtime->dma_area = (u8 *)dev->m_AudioBuffer.cpu + k * AUDIO_CH_BUF_SIZE;
	runtime->dma_bytes = AUDIO_CH_BUF_SIZE;

	err = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		return err;

	/* Align to 4 bytes */
	err = snd_pcm_hw_constraint_step(runtime, 0,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 4);
	if (err < 0)
		return err;
	pr_debug("%s: substream %p  N:%d  %s rate %d  \n", __func__,
		   substream, substream->number,   substream->name,  runtime->rate );

	return 0;
}


/*
 * ALSA capture callbacks definition
 */

static struct snd_pcm_ops snd_card_TW68_capture_ops = {
	.open =	snd_card_TW68_capture_open,
	.close = snd_card_TW68_capture_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_card_TW68_hw_params,
	.hw_free = snd_card_TW68_hw_free,
	.prepare = snd_card_TW68_capture_prepare,
	.trigger = snd_card_TW68_capture_trigger,
	.pointer = snd_card_TW68_capture_pointer,
	//.page = snd_card_TW68_page,
};

/* 7 bits, 0 : .5, 127 : 2.484375, n : .5 + n/64 */
static int gain_info(struct snd_kcontrol *ctl,
			    struct snd_ctl_elem_info *info)
{
	info->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	info->count = 1;
	info->value.integer.min = 0;
	info->value.integer.max = 127;
	return 0;
}

static int gain_get(struct snd_kcontrol *ctl,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct TW68_dev *dev = ctl->private_data;
	int k = snd_ctl_get_ioffidx(ctl, &ucontrol->id);
	__u32 __iomem *chbase = dev->lmmio + ((k & 4) * 0x40);
	u32 gain;

	gain = readl(chbase + AUDIO_GAIN_CH0 + (k & 3));
	ucontrol->value.integer.value[0] = gain;
	pr_debug("%s: gain[%d]=%d\n", __func__, k, gain);
	return 0;
}

static int gain_put(struct snd_kcontrol *ctl,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct TW68_dev *dev = ctl->private_data;
	int k = snd_ctl_get_ioffidx(ctl, &ucontrol->id);
	__u32 __iomem *chbase = dev->lmmio + ((k & 4) * 0x40);
	u32 gain;

	gain = ucontrol->value.integer.value[0];
	if (gain < 0 || gain > 127)
		return -EINVAL;
	pr_debug("%s: gain[%d]=%d\n", __func__, k, gain);
	writel(gain, chbase + AUDIO_GAIN_CH0 + (k & 3));
	return 0;
}

static const struct snd_kcontrol_new gain_control = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Mic Capture Volume",
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info = gain_info,
	.get = gain_get,
	.put = gain_put,
	.count = AUDIO_NCH,
};

/*
 * ALSA PCM setup
 *
 *   Called when initializing the board. Sets up the name and hooks up
 *  the callbacks
 *
 */

static int snd_card_TW68_pcm_reg(struct snd_card_tw68 *ctw, long idevice)
{
	struct snd_pcm *pcm;
	struct snd_pcm_substream *ss;
	int err, i;
	struct snd_kcontrol *ctl;

	pr_debug("%s\n", __func__);
	if ((err = snd_pcm_new(ctw->card, "TW6869 PCM", idevice, 0, AUDIO_NCH, &pcm)) < 0)   //0, 1
		return err;

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_card_TW68_capture_ops);

	pcm->private_data = ctw;
	pcm->info_flags = 0;
	strcpy(pcm->id, "TW68 PCM");
	strcpy(pcm->name, "TW68 Analog Audio Capture");

	ctw->TW68_pcm = pcm;

	for (i = 0, ss = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
			ss; ss = ss->next, i++) {
		sprintf(ss->name, "TW68 #%d Audio In ", i);
		ctw->substream[i] = ss;
		pr_debug("%s: substream[%d] %p\n", __func__, i, ctw->substream[i]);
	}

	ctl = snd_ctl_new1(&gain_control, ctw->dev);
	err = snd_ctl_add(ctw->card, ctl);
	return err;
}


static void snd_TW68_free(struct snd_card * card)
{
	struct snd_card_tw68 *ctw = (struct snd_card_tw68*)card->private_data;

	pr_debug("%s: ctw %p\n", __func__, ctw);
}

/*
 * ALSA initialization
 *
 *   Called by the init routine, once for each TW68 device present,
 *  it creates the basic structures and registers the ALSA devices
 *
 */

int TW68_alsa_create(struct TW68_dev *dev)
{

	struct snd_card *card = NULL;
	struct snd_card_tw68 *ctw;
	static struct snd_device_ops ops = { NULL };

	int err;

	pr_debug("%s\n", __func__);

	if(TW68_audio_nPCM > (SNDRV_CARDS-2))
		return -ENODEV;


	err = snd_card_create(SNDRV_DEFAULT_IDX1, "TW68 SoundCard", THIS_MODULE,
			      sizeof(struct snd_card_tw68), &card);
	if (err < 0)
		return err;

	strcpy(card->driver, "TW6869 audio");
	strcpy(card->shortname, "TW68 PCM");
	sprintf(card->longname, "%s at 0x%p irq %d",
		dev->name, dev->bmmio, dev->pci->irq);

	snd_card_set_dev(card, &dev->pci->dev);

	err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, dev, &ops);
	if (err < 0)
		return err;

	card->private_free = snd_TW68_free;
	ctw = (struct snd_card_tw68 *)card->private_data;
	ctw->dev = dev;
	ctw->card= card;
	ctw->last_audio_PB = 0xffff00;

	dev->card = card;

	spin_lock_init(&ctw->lock);

	pr_debug("alsa: %s registered as card %s  ctw %p  err :%d \n", card->longname, card->shortname, ctw, err);

	if ((err = snd_card_TW68_pcm_reg(ctw, 0)) < 0)
		goto __nodev;

	///snd_card_set_dev(card, dev);


	if (err < 0) {
		pr_debug("xxxxxx alsa: register TW68 card fail  :%d \n", err);
		return err;
	}
	pr_debug("alsa: %s registered as PCM card %s  :%d \n",card->longname, card->shortname, err);

	if ((err = snd_card_register(card)) == 0) {
		TW68_audio_nPCM++;
		return 0;
	}
	pr_debug("xxxxxx alsa: %s registered as card %s  :%d \n",card->longname, card->shortname, err);
__nodev:
	pr_debug("xxxxxx alsa: register TW68 card fail  :%d \n", err);
	snd_card_free(card);
	return err;
}

int TW68_alsa_free(struct TW68_dev *dev)
{
	if(dev->card) {
		snd_card_free(dev->card);
		dev->card = NULL;
	}
	TW68_audio_nPCM--;
	return 1;
}
