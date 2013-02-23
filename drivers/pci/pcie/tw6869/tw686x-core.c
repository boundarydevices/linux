/*
 *
 * Driver for Techwell TW6864/68 based DVR cards
 *
 * (c) 2009-10 liran <jlee@techwellinc.com.cn> [Techwell China]
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
 */

#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kmod.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/div64.h>

#include "tw686x-reg.h"
#include "tw686x.h"
#include "tw686x-device.h"

MODULE_DESCRIPTION("Driver for tw6864/68 based DVR cards");
MODULE_AUTHOR("liran<jli11@intersil.com>");
MODULE_LICENSE("GPL");

unsigned int tw686x_debug;
module_param(tw686x_debug, int, 0644);
MODULE_PARM_DESC(tw686x_debug, "enable debug messages");

unsigned int tw686x_dmamode = TW686X_DMA_MODE_BLOCK;
module_param(tw686x_dmamode, int, 0644);
MODULE_PARM_DESC(tw686x_dmamode, "tw686x dma mode");

unsigned int tw686x_audio = 1;
module_param(tw686x_audio, int, 0644);
MODULE_PARM_DESC(tw686x_audio, "tw686x audio enable");

unsigned int tw686x_restart_timer = 1;
module_param(tw686x_restart_timer, int, 0644);
MODULE_PARM_DESC(tw686x_restart_timer, "tw686x dma restart timer enable");

unsigned int tw686x_bcsi = 0;
module_param(tw686x_bcsi, int, 0644);
MODULE_PARM_DESC(tw686x_bcsi, "tw686x driver for BCSI enable");

//zxx_20120712 added for TW6865
unsigned int tw686x_6865 = 0;
module_param(tw686x_6865, int, 0644);
MODULE_PARM_DESC(tw686x_6865, "tw6865 mode");

static unsigned int tw686x_chipcount = 0;

DEFINE_MUTEX(tw686x_chiplist_lock);
LIST_HEAD(tw686x_chiplist);

void tw686x_dmabuf_free(struct tw686x_chip *chip,
		       struct tw686x_dmabuf *buf)
{
    struct pci_dev *pci = chip->pci;

	if (NULL == buf->cpu)
		return;
	pci_free_consistent(pci, buf->size, buf->cpu, buf->dma);

    dprintk(DPRT_LEVEL0, chip, " dmamem free dma=%lx cpu=%p size=%d\n",
           (unsigned long)buf->dma, buf->cpu, buf->size);

	memset(buf,0,sizeof(*buf));
}

int tw686x_dmabuf_alloc(struct tw686x_chip *chip,
		       struct tw686x_dmabuf *buf,
		       unsigned int size)
{
    struct pci_dev *pci = chip->pci;
	__le32 *cpu;
	dma_addr_t dma = 0;

	if (NULL != buf->cpu && buf->size < size)
		tw686x_dmabuf_free(chip,buf);
	if (NULL == buf->cpu) {
	    size = PAGE_ALIGN(size);
		cpu = pci_alloc_consistent(pci, size, &dma);
		if (NULL == cpu) {
            dprintk(DPRT_LEVEL0, chip, "dmamem alloc failed size=%d\n", size);
			return -ENOMEM;
		}
		buf->cpu  = cpu;
		buf->dma  = dma;
		buf->size = size;
        dprintk(DPRT_LEVEL0, chip, "dmamem alloc dma=%lx cpu=%p size=%d\n",
           (unsigned long)dma, cpu, size);
	}
	memset(buf->cpu,0,buf->size);
	return 0;
}

static void tw686x_shutdown(struct tw686x_chip *chip)
{
	/* Disable Video DMA */
	/* Disable Audio activity */
	/* Disable Interrupts */

    tw686x_dev_uninit( chip );
}

static void tw686x_reset(struct tw686x_chip *chip)
{
	dprintk(1, chip, "%s()\n", __func__);

	tw686x_dev_uninit( chip );
    tw686x_dev_init( chip );
}

static int get_resources(struct tw686x_chip *chip)
{
	if (request_mem_region(pci_resource_start(chip->pci, 0),
			       pci_resource_len(chip->pci, 0),
			       chip->name))
		return 0;

	printk(KERN_ERR "%s: can't get MMIO memory @ 0x%llx\n",
		chip->name, (unsigned long long)pci_resource_start(chip->pci, 0));

	return -EBUSY;
}

static void tw686x_assist_timeout(unsigned long data)
{
	struct tw686x_chip* chip = (struct tw686x_chip*)data;
	int i=0;

	u32  tmp  = tw_read(TW6864_R_DMA_CMD);
	u32  tmp1 = tw_read(TW6864_R_DMA_CHANNEL_ENABLE);
	dprintk(DPRT_LEVEL0, chip, "%s(), %x %x %x\n", __func__, tmp, tmp1, chip->dma_restart);

    if(chip->dma_restart > 0)
    {
        unsigned long flags;
        struct tw686x_vdev* dev;
        struct tw686x_adev* adev;

        tw_write(TW6864_R_DMA_CMD, 0);

        spin_lock_irqsave(&chip->slock, flags);
        for(i=0; i<chip->vid_count; i++) {
            dev = chip->vid_dev[i];
            if(chip->dma_restart & (1<<i)) {
                dev->dma_started = false;
                dev->pb_next = 0;
                dev->pb_curr = 0;
                tw686x_video_start(dev);
            }
        }
        if(CaptureFLV)
        {
            for(i=0; i<chip->aud_count; i++) {
                if(chip->dma_restart & (1<<(TW686X_AUDIO_BEGIN+i))) {
                    adev = chip->aud_dev[i];
                    if(adev->running)
                    {
                        adev->period_idx  = 0;
                        adev->read_offset = 0;
                        adev->substream   = NULL;
                        adev->dma_blk     = 0;
                        tw686x_audio_trigger( adev, 1 );
                    }
                }
            }
        }
        else
        {
            for(i=0; i<chip->aud_count; i++) {
                if(chip->dma_restart & (1<<(TW686X_AUDIO_BEGIN+i))) {
                    adev = chip->aud_dev[i];
                    if(adev->running)
                    {
                        adev->pb_flag = 0;
                        tw686x_dev_run_audio(adev, true);
                    }
                }
            }
        }
        chip->dma_restart = 0;
        spin_unlock_irqrestore(&chip->slock, flags);
    }
#if(0)
    else
    {
        for(i=0; i<chip->vid_count; i++) {
            tw686x_video_status(chip->vid_dev[i]);
        }
    }

	mod_timer(&chip->tm_assist, jiffies+BUFFER_TIMEOUT*2);
#endif
}

static int tw686x_setup(struct tw686x_chip *chip)
{
    struct tw686x_vdev *vdev;
    struct tw686x_adev *adev;
    int i = 0;

	chip->nr = tw686x_chipcount++;
	sprintf(chip->name, "tw6869[%d]", chip->nr);

	mutex_lock(&tw686x_chiplist_lock);
	list_add_tail(&chip->chiplist, &tw686x_chiplist);
	mutex_unlock(&tw686x_chiplist_lock);

	if (get_resources(chip) < 0) {
		printk(KERN_ERR "%s No more PCIe resources for "
		       "subsystem: %04x:%04x\n",
		       chip->name, chip->pci->subsystem_vendor,
		       chip->pci->subsystem_device);

		tw686x_chipcount--;
		return -ENODEV;
	}

	/* PCIe stuff */
	chip->bmmio = ioremap(pci_resource_start(chip->pci, 0),
			     pci_resource_len(chip->pci, 0));

	printk(KERN_INFO "%s: subsystem: %04x:%04x\n",
	       chip->name, chip->pci->subsystem_vendor,
	       chip->pci->subsystem_device);

	//zxx_20120712 added for TW6865
	int nSubDevNum = TW686X_DECODER_COUNT;
	if (tw686x_6865) {
		nSubDevNum = TW686X_DECODER_COUNT / 2;
	}

	chip->vid_count = nSubDevNum;

	/* init hardware */
	tw686x_reset(chip);
	chip->aud_count  = chip->vid_count;

    for( i=0; i<chip->vid_count; i++ ) {
        vdev = kzalloc(sizeof(struct tw686x_vdev), GFP_KERNEL);
        if (NULL == vdev) {
            return -ENOMEM;
        }

        vdev->chip = chip;
        vdev->channel_id = i;
        vdev->slock= &chip->slock;
        chip->vid_dev[i]= vdev;

        if (tw686x_video_register(vdev) < 0) {
            printk(KERN_ERR "%s: %s() Failed to register "
                "video adapters %d\n", chip->name, __func__, i);
            return -ENOMEM;
        }

        if( tw686x_dmabuf_alloc(chip, &vdev->dma_desc[TW686X_DMA_DESC_P], TW686X_DMA_DESC_LEN)<0 ) {
            printk(KERN_INFO "%s: allocate dma p-buffer descriptor memory failed\n", chip->name);
            return -ENOMEM;
        }
        if( tw686x_dmabuf_alloc(chip, &vdev->dma_desc[TW686X_DMA_DESC_B], TW686X_DMA_DESC_LEN)<0 ) {
            printk(KERN_INFO "%s: allocate dma b-buffer descriptor memory failed\n", chip->name);
            return -ENOMEM;
        }
        tw686x_dev_set_pbdesc(vdev);
    }

    if(tw686x_audio) {
        for( i=0; i<chip->aud_count; i++ ) {
            adev = kzalloc(sizeof(struct tw686x_adev), GFP_KERNEL);
            if (NULL == adev) {
                return -ENOMEM;
            }

            adev->chip = chip;
            adev->channel_id = i;
            adev->slock= &chip->slock;
            chip->aud_dev[i]= adev;

            if(CaptureFLV)
            {
                if (tw686x_audio_create(adev) < 0) {
                    printk(KERN_ERR "%s: %s() Failed to create "
                        "audio adapters %d\n", chip->name, __func__, i);
                    kfree( adev );
                    chip->aud_dev[i] = NULL;
                    break;
                }
            }
            else
            {
                if (tw686x_alsa_create(adev) < 0) {
                    printk(KERN_ERR "%s: %s() Failed to create "
                        "audio adapters %d\n", chip->name, __func__, i);
                    kfree( adev );
                    chip->aud_dev[i] = NULL;
                    break;
                }
            }
        }
    }

	if(chip->vid_count > 0) {
		chip->tm_assist.function = tw686x_assist_timeout;
		chip->tm_assist.data = (unsigned long)chip;
		init_timer(&chip->tm_assist);
		//mod_timer(&chip->tm_assist, jiffies+BUFFER_TIMEOUT*2);
	}
	chip->dma_restart = 0;

	return 0;
}

static void tw686x_unregister(struct tw686x_chip *chip)
{
    int i=0;

	release_mem_region(pci_resource_start(chip->pci, 0),
			   pci_resource_len(chip->pci, 0));

	if(chip->vid_count > 0) {
		chip->dma_restart = 0;
		del_timer(&chip->tm_assist);
	}

    for( i=0; i<chip->vid_count; i++ ) {
        if(chip->vid_dev[i]) {
            tw686x_video_unregister( chip->vid_dev[i] );
            tw686x_dmabuf_free( chip, &chip->vid_dev[i]->dma_desc[TW686X_DMA_DESC_P] );
            tw686x_dmabuf_free( chip, &chip->vid_dev[i]->dma_desc[TW686X_DMA_DESC_B] );
            kfree( chip->vid_dev[i] );
            chip->vid_dev[i] = NULL;
        }
    }

    for( i=0; i<chip->aud_count; i++ ) {
        if(chip->aud_dev[i]) {
            if(CaptureFLV)
            {
                tw686x_audio_free( chip->aud_dev[i] );
            }
            else
            {
                tw686x_alsa_free( chip->aud_dev[i] );
            }
            kfree( chip->aud_dev[i] );
            chip->aud_dev[i] = NULL;
        }
    }

	iounmap(chip->bmmio);
}

static irqreturn_t tw686x_irq(int irq, void *chip_id)
{
	struct tw686x_chip *chip = chip_id;
	struct tw686x_vdev *vdev = NULL;
	struct tw686x_adev *adev = NULL;
	int handled = 0;
	int i;
	u32 dma_status, pb_status, dma_error, dma_cmd;

	dma_status = tw_read( TW6864_R_DMA_INT_STATUS );
	pb_status  = tw_read( TW6864_R_DMA_PB_STATUS );
	dma_error  = tw_read( TW6864_R_DMA_INT_ERROR );
	dma_cmd    = tw_read( TW6864_R_DMA_CMD );

    if( (dma_cmd & TW6864_R_DMA_CMD_RESETALL) && (dma_status || dma_error) ) {
        spin_lock(&chip->slock);

        for( i=0; i<chip->vid_count; i++ ) {
            if( (dma_status & TW6864_R_DMA_INT_STATUS_DMA(i))/* || (dma_error & TW6864_R_DMA_VINT_FIFO(i))*/ ) {
                vdev = chip->vid_dev[i];
                tw686x_video_irq( vdev, dma_status, pb_status );
            }
            else if( dma_error & TW6864_R_DMA_VINT_FIFO(i) ) {
                vdev = chip->vid_dev[i];
                tw686x_dev_run( vdev, false );
                //dvprintk(DPRT_LEVEL0, vdev, "irq %x %x\n", dma_status, dma_error);
            }
        }
        for( i=0; i<chip->aud_count; i++ ) {
            if( dma_status & TW6864_R_DMA_INT_STATUS_DMA(i+TW686X_AUDIO_BEGIN) ) {
                adev = chip->aud_dev[i];
#if(CaptureFLV)
                tw686x_audio_irq( adev, dma_status, pb_status );
#else
                tw686x_alsa_irq( adev, dma_status, pb_status );
#endif
            }
        }

        if( dma_status & TW6864_R_DMA_INT_STATUS_TOUT )
            dprintk(DPRT_LEVEL0, chip, "irq timeout %x %x %x\n", dma_status, dma_error, dma_cmd);

        if( dma_status & TW6864_R_DMA_INT_STATUS_TOUT )
        {
            //DMA time out
            tw686x_dev_setdma(chip, 0);
            tw686x_video_resetdma( chip, dma_cmd );
#if(CaptureFLV)
            //tw686x_audio_resetdma( chip, dma_cmd );
#else
            //tw686x_alsa_resetdma( chip, dma_cmd );
#endif
        }

        handled = 1;
        spin_unlock(&chip->slock);
    }

	return IRQ_RETVAL(handled);
}

static int __devinit tw686x_initchip(struct pci_dev *pci_dev,
				     const struct pci_device_id *pci_id)
{
	struct tw686x_chip *chip;
	int err;
	u8  pci_rev,pci_lat;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (NULL == chip)
		return -ENOMEM;

	spin_lock_init(&chip->slock);

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	err = v4l2_device_register(&pci_dev->dev, &chip->v4l2_dev);
	if (err < 0)
		goto fail_free;
#endif

	/* pci init */
	chip->pci = pci_dev;
	if (pci_enable_device(pci_dev)) {
		err = -EIO;
		goto fail_unreg;
	}

	if (tw686x_setup(chip) < 0) {
		err = -EINVAL;
		goto fail_unreg;
	}

	/* print pci info */
	pci_read_config_byte(pci_dev, PCI_CLASS_REVISION, &pci_rev);
	pci_read_config_byte(pci_dev, PCI_LATENCY_TIMER,  &pci_lat);
	printk(KERN_INFO "%s: found at %s, bus: %d, rev: %d, irq: %d, "
	       "latency: %d, mmio: 0x%llx\n", chip->name,
	       pci_name(pci_dev), pci_dev->bus->number, pci_rev, pci_dev->irq, pci_lat,
           (unsigned long long)pci_resource_start(pci_dev, 0));

	pci_set_master(pci_dev);
	if (!pci_dma_supported(pci_dev, 0xffffffff)) {
		printk("%s: Oops: no 32bit PCI DMA ???\n", chip->name);
		err = -EIO;
		goto fail_irq;
	}

	err = request_irq(pci_dev->irq, tw686x_irq,
			  IRQF_SHARED | IRQF_DISABLED, chip->name, chip);
	if (err < 0) {
		printk(KERN_ERR "%s: can't get IRQ %d\n",
		       chip->name, pci_dev->irq);
		goto fail_irq;
	}

#if(LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
	pci_set_drvdata(pci_dev, chip);
#endif

	return 0;

fail_irq:
	tw686x_unregister(chip);
fail_unreg:
#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	v4l2_device_unregister(&chip->v4l2_dev);
fail_free:
#endif
	kfree(chip);
	return err;
}

static void __devexit tw686x_finichip(struct pci_dev *pci_dev)
{
#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	struct v4l2_device *v4l2_dev = pci_get_drvdata(pci_dev);
	struct tw686x_chip *chip = to_tw686x(v4l2_dev);
#else
	struct tw686x_chip *chip = pci_get_drvdata(pci_dev);
#endif

	tw686x_shutdown(chip);

	pci_disable_device(pci_dev);
#if(LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
	pci_set_drvdata(pci_dev, NULL);
#endif

	/* unregister stuff */
	free_irq(pci_dev->irq, chip);

	mutex_lock(&tw686x_chiplist_lock);
	list_del(&chip->chiplist);
	mutex_unlock(&tw686x_chiplist_lock);

	tw686x_unregister(chip);
#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	v4l2_device_unregister(v4l2_dev);
#endif
	kfree(chip);
}

static struct pci_device_id tw686x_pci_tbl[] = {
	{
		/* TW6869 */
		.vendor       = 0x1797,
		.device       = 0x6869,
		.subvendor    = PCI_ANY_ID,
		.subdevice    = PCI_ANY_ID,
	}, {
		/* --- end of list --- */
	}
};
MODULE_DEVICE_TABLE(pci, tw686x_pci_tbl);

static struct pci_driver tw686x_pci_driver = {
	.name     = "tw6869",
	.id_table = tw686x_pci_tbl,
	.probe    = tw686x_initchip,
	.remove   = __devexit_p(tw686x_finichip),
	/* TODO */
	.suspend  = NULL,
	.resume   = NULL,
};

static int tw686x_init(void)
{
	printk(KERN_INFO "tw6869 driver version %d.%d.%d loaded\n",
	       (TW686X_VERSION_CODE >> 16) & 0xff,
	       (TW686X_VERSION_CODE >>  8) & 0xff,
	       TW686X_VERSION_CODE & 0xff);
    printk(KERN_INFO "tw6869 driver dma_mode=%x, debug=%x\n", tw686x_dmamode, tw686x_debug);

	return pci_register_driver(&tw686x_pci_driver);
}

static void tw686x_fini(void)
{
	pci_unregister_driver(&tw686x_pci_driver);

	printk(KERN_INFO "tw6869 driver version %d.%d.%d unloaded\n",
	       (TW686X_VERSION_CODE >> 16) & 0xff,
	       (TW686X_VERSION_CODE >>  8) & 0xff,
	       TW686X_VERSION_CODE & 0xff);
}

module_init(tw686x_init);
module_exit(tw686x_fini);
