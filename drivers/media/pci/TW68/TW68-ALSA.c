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

#define audio_nCH		8
#define DMA_page		4096
#define MAX_BUFFER		(DMA_page * 4 *audio_nCH)


/*
 * TW PCM structure
 */
typedef struct snd_card_TW68_pcm {

	struct TW68_dev *dev;
	spinlock_t lock;
	struct snd_pcm_substream *substream;
} snd_card_TW68_pcm_t;

/*
 * Main chip structure
 */
typedef struct snd_card_TW68 {
	struct snd_card *card;
	spinlock_t mixer_lock;

	struct pci_dev *pci;
	struct TW68_dev *dev;

	void   *audio_ringbuffer;
	struct snd_pcm  *TW68_pcm;
	struct snd_pcm_substream *substream[10];
	spinlock_t lock;
} snd_card_TW68_t;


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
	.formats = SNDRV_PCM_FMTBIT_S16_LE , //| SNDRV_PCM_FMTBIT_S8,
	.rates = SNDRV_PCM_RATE_8000_48000,        //SNDRV_PCM_RATE_32000,   //
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 1,
	.buffer_bytes_max =	4*DMA_page, //4096,
	.period_bytes_min =	256, //256,
	.period_bytes_max =	4096, //1024,
	.periods_min      = 4,
	.periods_max      = 8,
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

	struct snd_pcm_runtime *runtime;
	int k;

	u32 audio_irq = (dev->videoDMA_ID & dma_status & 0xff00) >>8;
	u32 audio_PB =0;
	static u32 last_audio_PB =0xFFFF;

	struct TW68_pgtable p_Audio = dev ->m_AudioBuffer;
        snd_card_TW68_t *card_TW68 = (snd_card_TW68_t *) dev->card->private_data;

	u8*	Audioptr = (u8*)p_Audio.cpu;
	u8* AudioVB = card_TW68->audio_ringbuffer;

//	pr_debug("%s()  card_TW68 pcm %p  dma_status %x  pb_status %x \n", __func__,
//		    card_TW68->TW68_pcm, dma_status, pb_status);

	if (audio_irq) {
		audio_PB = (pb_status >>8)& audio_irq;
		///pr_debug(" Audio irq:%d   PB: %X \n", audio_irq,  audio_PB);
		for (k = 0; k < 8; k++) {
			if (audio_irq & (1<<k)) {
				runtime = card_TW68->substream[k]->runtime;
				/*
				pr_debug(" TW68_alsa_irq  Audio CH:%d   aPB: %X   RUNTIME HWptr 0x%p, 0x%X, jf 0x%x Pdz 0x%x 0x%x  BFz 0x%x  DMA  %X  %X  %X\n",
						k,  ((audio_PB >>k)& 0x1),
						runtime->hw_ptr_base, runtime->hw_ptr_interrupt, runtime->hw_ptr_jiffies,
						runtime->period_size, runtime->periods, runtime->buffer_size,
						runtime->dma_area, runtime->dma_addr, runtime->dma_bytes
						);
				*/
				if (((last_audio_PB >>k)& 0x1) ^ ((audio_PB >>k)& 0x1)) {
					if(((audio_PB >>k)& 0x1) ==0) {
						last_audio_PB &= ~(1 <<k);
						runtime->hw_ptr_base = 0;
						runtime->status->hw_ptr = 0;
						runtime->hw_ptr_interrupt = 0;
						runtime->control->appl_ptr =0;
						AudioVB +=  PAGE_SIZE *2 *k;
						Audioptr+=  PAGE_SIZE *2 *k;
						memcpy(AudioVB, Audioptr, PAGE_SIZE);
					} else {
						last_audio_PB |= (1 <<k);
						runtime->hw_ptr_base = runtime->period_size;
						runtime->status->hw_ptr = 0;
						runtime->hw_ptr_interrupt = 0;
						runtime->control->appl_ptr = runtime->hw_ptr_base;
						AudioVB +=  (PAGE_SIZE *2 *k + PAGE_SIZE);
						Audioptr+=  (PAGE_SIZE *2 *k + PAGE_SIZE);
						memcpy(AudioVB, Audioptr, PAGE_SIZE);
					}
					snd_pcm_period_elapsed(card_TW68->substream[k]);   // call pointer
				}
				/*
				long avail = snd_pcm_capture_avail(runtime);

				pr_debug("TW68_alsa_irq:   card 0x%p,  substream 0x%p, runtime BD 0%d  hw_ptr 0x%p, runtime->control->appl_ptr 0x%p,  cap avail 0x%X\n",
				card_TW68, card_TW68->substream[k], runtime->boundary, runtime->status->hw_ptr, runtime->control->appl_ptr, avail); ///runtime->xfer_align
			*/
			}
		}
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
	int nId = substream->number;

	u32		dwReg=0;
	u32	dwRegF=0;
	//u32		dwREGA=0;
	int		ret =0;
	long		avail =0;
	snd_card_TW68_t *card_TW68 = snd_pcm_substream_chip(substream);
	struct TW68_dev *dev;
	struct snd_pcm_runtime *runtime = substream->runtime;
	dev = card_TW68 ->dev;



	if (!card_TW68) {
		pr_err("%s: can't find device struct.\n", __func__);
		return -ENODEV;
	}

	avail = snd_pcm_capture_avail(runtime);

	//pr_debug("snd_card_TW68_capture_trigger:   card 0x%p,   runtime BD 0%d  hw_ptr 0x%p, runtime->control->appl_ptr 0x%p,  Cap avail xfer_align 0x%X\n",
	//		card_TW68,  runtime->boundary, runtime->status->hw_ptr, runtime->control->appl_ptr, avail);  ///runtime->xfer_align);


	//spin_lock(&pcm->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		dev->videoDMA_ID |= (1 << (nId +8));
		dev->videoCap_ID |= (1 << (nId +8));

		dwReg = reg_readl(DMA_CHANNEL_ENABLE);
		dwReg |= (1<<(nId + 8));
		reg_writel(DMA_CHANNEL_ENABLE, dwReg);
		dwReg = reg_readl(DMA_CHANNEL_ENABLE);

		dwRegF = reg_readl(DMA_CMD);
		dwRegF |= (1<< (nId+8));
		dwRegF |= (1<<31);
		reg_writel(DMA_CMD, dwRegF);
		dwRegF = reg_readl(DMA_CMD);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		dev->videoDMA_ID &= ~(1 << (nId +8));
		dev->videoCap_ID &= ~(1 << (nId +8));

		dwReg = reg_readl(DMA_CHANNEL_ENABLE);
		dwReg &= ~(1<<(nId + 8));
		reg_writel(DMA_CHANNEL_ENABLE, dwReg);
		dwReg = reg_readl(DMA_CHANNEL_ENABLE);

		dwRegF = reg_readl(DMA_CMD);
		dwRegF &= ~(1<< (nId+8));
		//dwRegF |= (1<<31);
		reg_writel(DMA_CMD, dwRegF);
		dwRegF = reg_readl(DMA_CMD);
		break;

	default:
		ret = -EINVAL;
	}

	//spin_unlock(&pcm->lock);
	pr_debug("%s()   cmd %X  ret %X   DMA_CHANNEL_ENABLE 0x%X   0x%X\n", __func__, cmd, ret, dwReg, dwRegF);
	return ret;
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
	int nId = substream->number;
	u32 dmaP;
	u32 dmaB;
	u32 dwREG = 0x30;  ///0x38;
	u32 dwREGA;

	u32 Currenrt_SAMPLE_RATE = substream->runtime->rate;


	//struct snd_pcm_runtime *runtime = substream->runtime;
	//snd_card_TW68_pcm_t *pcm;
	snd_card_TW68_t *card_TW68 = snd_pcm_substream_chip(substream);
	struct TW68_dev *dev;

	if (!card_TW68) {
		pr_err("%s: can't find device struct.\n", __func__);
		return -ENODEV;
	}
	dev = card_TW68 ->dev;

	dmaP = dev->m_AudioBuffer.dma + (DMA_page <<1)* nId;
	dmaB = dev->m_AudioBuffer.dma + (DMA_page <<1)* nId + DMA_page;

	reg_writel(DMA_CH8_CONFIG_P + nId*2, dmaP );
	reg_writel(DMA_CH8_CONFIG_B + nId*2, dmaB );


	if (nId <4) {
		reg_writel(AUDIO_GAIN_0 + nId, dwREG);
		dwREG = reg_readl(AUDIO_GAIN_0 + nId);
	} else {
		// internal decoders
		reg_writel(AUDIO_GAIN_0 + 0x100 + nId, dwREG);
		dwREG = reg_readl(AUDIO_GAIN_0 + 0x100 + nId);
	}

	///Currenrt_SAMPLE_RATE = pNewFormat->WaveFormatEx.nSamplesPerSec;
	//dwREG = (u32)(((u64)(125000000)  <<16)/(Currenrt_SAMPLE_RATE ));
	dwREG = ((125000000 <<5)/(Currenrt_SAMPLE_RATE >>2 )) <<9;
	reg_writel(AUDIO_CTRL2, dwREG);
	dwREGA = reg_readl(AUDIO_CTRL2);
	pr_debug("%s: AUDIO_CTRL2: 0x%x         0x%X\n", __func__, dwREGA, dwREG);
	pr_debug("%s: dev %p  substream->runtime->rate %d  dwREG: 0x%X\n", __func__, dev, Currenrt_SAMPLE_RATE, dwREG);
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
snd_card_TW68_capture_pointer(struct snd_pcm_substream * substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	//snd_card_TW68_t *card_TW68 = snd_pcm_substream_chip(substream);
	// 4096 byte /2
	return DMA_page *8 /runtime->frame_bits;
}



static void snd_card_TW68_runtime_free(struct snd_pcm_runtime *runtime)
{
	snd_card_TW68_pcm_t *pcm = runtime->private_data;
	//struct TW68_dev *dev = pcm->dev;

	pr_debug("%s()\n", __func__);
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

static int snd_card_TW68_hw_params(struct snd_pcm_substream * substream,
				      struct snd_pcm_hw_params * hw_params)
{
	///int err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
	int err =0;
	pr_debug("%s()  substream->runtime->rate %d  err:0x%X\n", __func__, substream->runtime->rate, err);
	// return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
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
	pr_debug("%s()\n", __func__);
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
	pr_debug("%s()\n", __func__);
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
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_card_TW68_pcm_t *pcm;
	snd_card_TW68_t *card_TW68 = snd_pcm_substream_chip(substream);
	struct TW68_dev *dev;
	int err;

	if (!card_TW68) {
		pr_err("%s: can't find device struct\n", __func__);
		return -ENODEV;
	}
	dev = card_TW68 ->dev;

	pcm = kzalloc(sizeof(*pcm), GFP_KERNEL);
	if (pcm == NULL)
		return -ENOMEM;

	pcm->dev=dev;

	spin_lock_init(&pcm->lock);

	pcm->substream = substream;
	runtime->private_data = pcm;
	runtime->private_free = snd_card_TW68_runtime_free;
	runtime->hw = snd_card_tw68_pcm_hw;
	err = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		return err;

	pr_debug("%s()  substream %p  N:%d  %s rate %d  \n", __func__,
		   substream, substream->number,   substream->name,  runtime->rate );

	return 0;
}


/*
 * ALSA capture start
 *
 *   - One of the ALSA capture callbacks.
 *
 *   Called when pcm data ready for user
 *
 */

static int snd_TW68_pcm_copy(struct snd_pcm_substream *ss, int channel,
			     snd_pcm_uframes_t pos, void __user *dst,
			     snd_pcm_uframes_t count)
{
	//struct snd_card_TW68_pcm *pcm = snd_pcm_substream_chip(ss);
	//int err, i;
	int nId = ss->number;
	long avail =0;
	struct snd_pcm_runtime *runtime = ss->runtime;

	snd_card_TW68_t *card_TW68 = snd_pcm_substream_chip(ss);
	struct TW68_dev *dev = card_TW68 ->dev;

	struct TW68_pgtable p_Audio = dev ->m_AudioBuffer;
	u8*	Audioptr = (u8*)p_Audio.cpu + (nId * (PAGE_SIZE <<1));
	u8* AudioVB = card_TW68->audio_ringbuffer + (nId * (PAGE_SIZE <<1));

	if (runtime->hw_ptr_base) {
		Audioptr += DMA_page;
		AudioVB += DMA_page;
	}
	/*
	pr_debug("%s()  nId:%d;  count:%X %d  AP 0X%p ", __func__, nId, count, count, Audioptr);

	pr_debug("%s() dev 0x%p A 0x%X  k:%d  ss:%p  BF:%d  P:%d S:%d F:%d  \n", __func__, dev,  p_Audio.size, ss->number, ss,
		runtime->buffer_size, runtime->period_size, runtime->sample_bits, runtime->frame_bits  );
	*/
	avail = snd_pcm_capture_avail(runtime);
	//pr_debug("  card 0x%p,  TW68_pcm 0x%p  runtime BD 0%d  hw_ptr b0x%p, s0x%p, runtime->control->appl_ptr 0x%p,  Cap avail xfer_align 0x%X\n",
	//		card_TW68, pcm, runtime->boundary, runtime->hw_ptr_base, runtime->status->hw_ptr, runtime->control->appl_ptr, avail);  ///runtime->xfer_align);

	AudioVB = Audioptr + DMA_page - (avail* runtime->frame_bits/8);

	if (copy_to_user_fromio(dst, AudioVB, count*runtime->frame_bits/8)) {
		pr_debug(" copy from io to user fail xxxx \n");
		return -EFAULT;
	}
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
	.copy =	 snd_TW68_pcm_copy,
	//.page = snd_card_TW68_page,
};

/*
 * ALSA PCM setup
 *
 *   Called when initializing the board. Sets up the name and hooks up
 *  the callbacks
 *
 */

static int snd_card_TW68_pcm_reg(snd_card_TW68_t *card_TW68, long idevice)
{
	struct snd_pcm *pcm;
	struct snd_pcm_substream *ss;
	int err, i;

	pr_debug("%s()\n", __func__);
	if ((err = snd_pcm_new(card_TW68->card, "TW6869 PCM", idevice, 0, audio_nCH, &pcm)) < 0)   //0, 1
		return err;

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_card_TW68_capture_ops);

	pcm->private_data = card_TW68;
	pcm->info_flags = 0;
	strcpy(pcm->id, "TW68 PCM");
	strcpy(pcm->name, "TW68 Analog Audio Capture");

	card_TW68->TW68_pcm = pcm;
	for (i = 0, ss = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
			ss; ss = ss->next, i++) {
		sprintf(ss->name, "TW68 #%d Audio In ", i);
		card_TW68->substream[i] = ss;
		pr_debug("%s() substream[%d] %p\n", __func__, i, card_TW68->substream[i]);
	}
	return 0;
}


static void snd_TW68_free(struct snd_card * card)
{
	snd_card_TW68_t *card_TW68 = (snd_card_TW68_t*)card->private_data;

	if (card_TW68->audio_ringbuffer)
		vfree(card_TW68->audio_ringbuffer);
	pr_debug("%s: card_TW68 %p\n", __func__, card_TW68);
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
	snd_card_TW68_t *card_TW68;
	static struct snd_device_ops ops = { NULL };

	int err;

	pr_debug("%s\n", __func__);

	if(TW68_audio_nPCM > (SNDRV_CARDS-2))
		return -ENODEV;


	err = snd_card_create(SNDRV_DEFAULT_IDX1, "TW68 SoundCard", THIS_MODULE,
			      sizeof(snd_card_TW68_t), &card);
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
	card_TW68 = (snd_card_TW68_t *) card->private_data;
	card_TW68->dev = dev;
	card_TW68->card= card;

	dev->card = card;

	spin_lock_init(&card_TW68->lock);

	pr_debug("alsa: %s registered as card %s  card_TW68 %p  err :%d \n",card->longname, card->shortname, card_TW68, err);

	if ((err = snd_card_TW68_pcm_reg(card_TW68, 0)) < 0)
		goto __nodev;

	///snd_card_set_dev(card, dev);


	if (err < 0) {
		pr_debug("xxxxxx alsa: register TW68 card fail  :%d \n", err);
		return err;
	}
	pr_debug("alsa: %s registered as PCM card %s  :%d \n",card->longname, card->shortname, err);

	if ((err = snd_card_register(card)) == 0) {
		card_TW68->audio_ringbuffer =  vmalloc(MAX_BUFFER);
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
