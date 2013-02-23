/*
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
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/list.h>
#include <linux/pagemap.h>
#include <linux/module.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>

#include "tw686x.h"
#include "tw686x-device.h"
#include "tw686x-reg.h"

MODULE_DESCRIPTION("v4l2 driver module for tw686x based DVR cards");
MODULE_AUTHOR("liran <jli11@intersil.com>");
MODULE_LICENSE("GPL");

static int tw686x_vidcount = 0;

/* ------------------------------------------------------------------- */
/* static data                                                         */

#define FORMAT_FLAGS_PACKED       0x01

static struct tw686x_fmt formats[] = {
    {
		.name     = "4:2:2, packed, YUYV",
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	}, {
		.name     = "4:2:2, packed, UYVY",
		.fourcc   = V4L2_PIX_FMT_UYVY,
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	}, {
		.name     = "15 bpp RGB, le",
		.fourcc   = V4L2_PIX_FMT_RGB555,
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	}, {
		.name     = "16 bpp RGB, le",
		.fourcc   = V4L2_PIX_FMT_RGB565,
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	}, {
		.name     = "4:1:1, packed, Y41P",
		.fourcc   = V4L2_PIX_FMT_Y41P,
		.depth    = 12,
		.flags    = FORMAT_FLAGS_PACKED,
	},
};

static const struct v4l2_queryctrl no_ctrl = {
	.name  = "42",
	.flags = V4L2_CTRL_FLAG_DISABLED,
};
static const struct v4l2_queryctrl video_ctrls[] = {
	/* --- video --- */
	{
		.id            = V4L2_CID_BRIGHTNESS,
		.name          = "Brightness",
		.minimum	   = -128,
		.maximum	   = 127,
		.step		   = 1,
		.default_value = 20,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_CONTRAST,
		.name          = "Contrast",
		.minimum       = 0,
		.maximum       = 255,
		.step          = 1,
		.default_value = 100,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_SATURATION,
		.name          = "Saturation",
		.minimum       = 0,
		.maximum       = 255,
		.step          = 1,
		.default_value = 128,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_HUE,
		.name          = "Hue",
		.minimum       = -128,
		.maximum       = 127,
		.step          = 1,
		.default_value = 0,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},
};
static const unsigned int CTRLS = ARRAY_SIZE(video_ctrls);

static const struct v4l2_queryctrl* ctrl_by_id(unsigned int id)
{
	unsigned int i;

	for (i = 0; i < CTRLS; i++)
		if (video_ctrls[i].id == id)
			return video_ctrls+i;
	return NULL;
}

static struct tw686x_fmt *format_by_fourcc(unsigned int fourcc)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(formats); i++)
		if (formats[i].fourcc == fourcc)
			return formats+i;

	printk(KERN_ERR "%s(0x%08x) NOT FOUND\n", __func__, fourcc);
	return NULL;
}

/* ----------------------------------------------------------------------- */
/* resource management                                                     */

static int res_get(struct tw686x_vdev *dev, struct tw686x_fh *fh, unsigned int bit)
{
	if (fh->resources & bit)
		/* have it already allocated */
		return 1;

	/* is it free? */
	mutex_lock(&dev->lock);
	if (dev->resources & bit) {
		/* no, someone else uses it */
		mutex_unlock(&dev->lock);
		return 0;
	}
	/* it's free, grab it */
	fh->resources  |= bit;
	dev->resources |= bit;
	dvprintk(DPRT_LEVEL0, dev, "res: get %d\n",bit);
	mutex_unlock(&dev->lock);

	return 1;
}

static int res_check(struct tw686x_fh *fh, unsigned int bit)
{
	return (fh->resources & bit);
}

static int res_locked(struct tw686x_vdev *dev, unsigned int bit)
{
	return (dev->resources & bit);
}

static
void res_free(struct tw686x_vdev *dev, struct tw686x_fh *fh, unsigned int bits)
{
	BUG_ON((fh->resources & bits) != bits);

	mutex_lock(&dev->lock);
	fh->resources  &= ~bits;
	dev->resources &= ~bits;
	dvprintk(DPRT_LEVEL0, dev, "res: put %d\n",bits);
	mutex_unlock(&dev->lock);
}

void tw686x_video_start(struct tw686x_vdev* dev)
{
	struct tw686x_dmaqueue *q = &dev->video_q;
	struct tw686x_buf *buf;
	struct list_head *item, *item_temp;
	bool   b_active = false;

 	//dvprintk(1, dev, "%s()\n", __func__);

    if( !res_locked(dev, RESOURCE_VIDEO) ) {
        mod_timer(&q->timeout, jiffies+BUFFER_TIMEOUT);  //主要目的是在没有中断产生时，停止采集后释放所有buffer，否则可能streamoff死等
        return;
    }

    list_for_each(item, &q->active) {
        buf = list_entry(item, struct tw686x_buf, active);
        b_active = IS_TW686X_DMA_MODE_BLOCK ?
                   (dev->pb_next < dev->buf_needs) : (dev->pb_next<=buf->sg_ofo) ;
        if( b_active ) {
            buf->activate( dev, buf, dev->pb_curr, true );
        }
        dev->pb_next++;
    }

    list_for_each(item, &q->queued) {
        buf = list_entry(item, struct tw686x_buf, vb.queue);
        b_active = IS_TW686X_DMA_MODE_BLOCK ?
                   (dev->pb_next < dev->buf_needs) : (dev->pb_next<=buf->sg_ofo) ;
        if( b_active ) {
        		item_temp = item;
        		item = item->prev;
        		list_del(item_temp);
           buf->activate( dev, buf, dev->pb_curr, false );
        }
        dev->pb_next++;
    }

    if(dev->pb_next > 0) {
        tw686x_dev_run( dev, true );
        dev->dma_started = true;
    }
}

static void tw686x_video_restart_timeout(unsigned long data)
{
	struct tw686x_vdev* dev = (struct tw686x_vdev*)data;
	unsigned long flags;

 	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	spin_lock_irqsave(dev->slock, flags);
	dev->dma_started = false;
	dev->pb_next = 0;
	dev->pb_curr = 0;
	tw686x_video_start(dev);
	spin_unlock_irqrestore(dev->slock, flags);
}

int tw686x_video_restart(struct tw686x_vdev *dev, bool b_framerate)
{
 	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

//	assert_spin_locked(dev->slock);

	tw686x_dev_run( dev, false );
	dev->dma_started = false;
	dev->pb_next = 0;
	dev->pb_curr = 0;

	if(b_framerate) {
	    tw686x_dev_set_framerate(dev, dev->framerate, dev->field);
	}

	if(tw686x_restart_timer) {
		mod_timer(&dev->tm_restart, jiffies+(BUFFER_TIMEOUT/50));
	}
	else {
		tw686x_video_start(dev);
	}

   return 0;
}

static int tw686x_set_tvnorm(struct tw686x_vdev *dev, v4l2_std_id norm)
{
	dvprintk(DPRT_LEVEL0, dev, "%s(norm = 0x%08x) name: [%s]\n",
		__func__,
		(unsigned int)norm,
		v4l2_norm_to_name(norm));

	mutex_lock(&dev->lock);
	dev->tvnorm = norm;
    tw686x_dev_set_standard( dev, norm, true );

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	call_all(dev->chip, core, s_std, norm);
#endif
	mutex_unlock(&dev->lock);

	return 0;
}

static int tw686x_buffer_next(struct tw686x_vdev *dev,
			 struct tw686x_dmaqueue *q, int dma_pb)
{
	struct tw686x_buf *buf;
	int    ret = -EINVAL;

    dvprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, dev->pb_next);

//	assert_spin_locked(dev->slock);
	if (!list_empty(&q->queued)) {
		/* activate next one from queue */
		buf = list_entry(q->queued.next,struct tw686x_buf,vb.queue);
		list_del(&buf->vb.queue);
		ret = buf->activate(dev, buf, dma_pb, false);
	}
	else {
		dvprintk(DPRT_LEVEL0, dev, "buffer_next %p\n",NULL);
	}

	return ret;
}

static bool tw686x_buffer_wakeup(struct tw686x_vdev *dev, struct tw686x_dmaqueue *q, u32 state)
{
    struct tw686x_buf *buf;
    bool ret = false;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, state);

//	assert_spin_locked(dev->slock);
    if( !list_empty(&q->active) ) {
        /* finish current buffer */
        buf = list_entry(q->active.next, struct tw686x_buf, active);
        if( IS_TW686X_DMA_MODE_BLOCK || (state==VIDEOBUF_ERROR) ) {
            buf->vb.state = state;
            do_gettimeofday(&buf->vb.ts);
            list_del(&buf->active);
            wake_up(&buf->vb.done);
            dev->pb_next--;
            ret = true;
        }
        else {
            buf->sg_bytes_to -= buf->sg_bytes_pb;
            if( buf->sg_bytes_to>buf->sg_bytes_pb ) {
                tw686x_dev_set_dmadesc( dev, dev->pb_curr, buf );
            }
            else if( buf->sg_bytes_to<=0 ) {
                buf->vb.state = state;
                do_gettimeofday(&buf->vb.ts);
                list_del(&buf->active);
                wake_up(&buf->vb.done);
                dev->pb_next--;
                if( !list_empty(&q->active) ) {
                    /* finish current buffer */
                    //notice!!! 这个地方与buffer_active里边同时开启的话，会造成dma descriptor重复填充
                    //buf = list_entry(q->active.next, struct tw686x_buf, active);
                    //tw686x_dev_set_dmadesc( dev, dev->pb_curr, buf );
                }
                else {
                    ret = true;
                }
            }
            else {
                ret = true;
            }
        }
        dvprintk(DPRT_LEVEL0, dev, "wake up buffer %d, %d\n", buf->vb.i, buf->vb.state);
    }
    if( list_empty(&q->active) ) {
        del_timer(&q->timeout);
    }

    return ret;
}

static int tw686x_buffer_cancel(struct tw686x_vdev *dev, struct tw686x_dmaqueue *q, u32 state)
{
	struct tw686x_buf *buf;
	int    ret = -EINVAL;

    dvprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, dev->pb_next);

//	assert_spin_locked(dev->slock);
    if( !list_empty(&q->queued) ) {
        /* finish current buffer */
        buf = list_entry(q->queued.next, struct tw686x_buf, vb.queue);
        buf->vb.state = state;
        do_gettimeofday(&buf->vb.ts);
        list_del(&buf->vb.queue);
        wake_up(&buf->vb.done);
        dev->pb_next--;
        ret = true;
    }

	return ret;
}

static void tw686x_buffer_timeout(unsigned long data)
{
	struct tw686x_vdev *dev = (struct tw686x_vdev *)data;
	struct tw686x_dmaqueue *q = &dev->video_q;
	unsigned long flags;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	spin_lock_irqsave(dev->slock, flags);

#if(1)
    if( !res_locked(dev, RESOURCE_VIDEO) ) {
        while (!list_empty(&q->active)) {
            tw686x_buffer_wakeup( dev, q, VIDEOBUF_ERROR );
        }
        while (!list_empty(&q->queued)) {
            tw686x_buffer_cancel( dev, q, VIDEOBUF_ERROR );
        }
    }
    else {
        tw686x_video_restart( dev, false );
    }
#else
{
    int pb_status = dev->vstatus;
    if( !res_locked(dev, RESOURCE_VIDEO) ) {
        while (!list_empty(&q->active)) {
            tw686x_buffer_wakeup( dev, q, VIDEOBUF_ERROR );
        }
        while (!list_empty(&q->queued)) {
            tw686x_buffer_cancel( dev, q, VIDEOBUF_ERROR );
        }
        pb_status = 0;
    }
    else {
        if( !IS_TW686X_DMA_MODE_BLOCK ) {
            dev->pb_curr = pb_status;
            pb_status = !pb_status;
        }
        if( tw686x_buffer_wakeup(dev, &dev->video_q, VIDEOBUF_DONE) &&
            (dev->pb_next>=dev->buf_needs) ) {
            tw686x_buffer_next(dev, &dev->video_q, dev->pb_curr);
        }
        else if( dev->pb_next<=0 ) {
            tw686x_dev_run( dev, false );
            dev->dma_started = false;
            dev->pb_next = 0;
            dev->pb_curr = 0;
        }
        mod_timer(&q->timeout, jiffies+(BUFFER_TIMEOUT/20));
    }
    dev->vstatus = pb_status;
}
#endif

	spin_unlock_irqrestore(dev->slock, flags);
}

static int buffer_activate(struct tw686x_vdev *dev, struct tw686x_buf *buf,
        int dma_pb, bool queued)
{
	struct tw686x_dmaqueue *q = &dev->video_q;

	dvprintk(DPRT_LEVEL0, dev, "activate buf=%d\n", buf->vb.i);

	buf->vb.state = VIDEOBUF_ACTIVE;
	if(!queued) {
        list_add_tail(&buf->active, &q->active);
	}

    if( IS_TW686X_DMA_MODE_BLOCK ) {
        tw686x_dev_set_dmabuf(dev, dma_pb, buf);
    }
    else {
        buf->sg = NULL;
        buf->sg_offset = 0;
        buf->sg_bytes_to = 0;
        //if((dev->pb_next==0) || (buf->sg_ofo==0))     //notice!!! 这里可能造成dmadesc重复填充，需要再仔细斟酌
        tw686x_dev_set_dmadesc(dev, dma_pb, buf);
    }
    dev->pb_curr = (dev->pb_curr + 1) % dev->buf_needs;
	mod_timer(&q->timeout, jiffies+BUFFER_TIMEOUT);

	return 0;
}

static void buffer_release(struct videobuf_queue *vq,
	struct videobuf_buffer *vb)
{
	struct videobuf_dmabuf *dma;
	struct tw686x_fh       *fh  = vq->priv_data;
	struct tw686x_vdev     *dev = fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d) state=%d\n", __func__, vb->i, vb->state);

	BUG_ON(in_interrupt());

#if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
	videobuf_waiton(vq, vb, 0, 0);
#else
	videobuf_waiton(vb, 0, 0);
#endif

    if( IS_TW686X_DMA_MODE_BLOCK ) {
        videobuf_dma_contig_free_isl(vq, vb);
    }
    else {
        dma = videobuf_to_dma(vb);
#if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
        videobuf_dma_unmap(vq->dev, dma);
#else
        videobuf_dma_unmap(vq, dma);
#endif
        videobuf_dma_free(dma);
    }

	vb->state = VIDEOBUF_NEEDS_INIT;
}

static int buffer_setup(struct videobuf_queue *vq, unsigned int *count,
	unsigned int *size)
{
	struct tw686x_fh    *fh = vq->priv_data;
	struct tw686x_vdev   *dev  = fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, *count);

	*size = (fh->fmt->depth*fh->width*fh->height) >> 3;

	if (!*count || (*count > TW686X_BUFFER_COUNT)) {
		*count = TW686X_BUFFER_COUNT;
	}

	return 0;
}

static int buffer_prepare(struct videobuf_queue *q, struct videobuf_buffer *vb,
	       enum v4l2_field field)
{
	struct tw686x_fh  *fh  = q->priv_data;
	struct tw686x_vdev *dev = fh->dev;
	struct tw686x_buf *buf= container_of(vb, struct tw686x_buf, vb);
	int rc;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, vb->i);

	BUG_ON(NULL == fh->fmt);

	if (fh->width  < TW686X_MIN_WIDTH || fh->width  > norm_maxw(dev->tvnorm) ||
	    fh->height < TW686X_MIN_HEIGHT || fh->height > norm_maxh(dev->tvnorm))
		return -EINVAL;

	buf->vb.size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if(buf->vb.size <= TW686X_SG_PBLEN) {
        buf->sg_ofo = 1;
        buf->sg_bytes_pb = buf->vb.size;
	}
	else {
        buf->sg_ofo = 0;
	    buf->sg_bytes_pb = buf->vb.size / ((buf->vb.size<=TW686X_SG_PBLEN*2) ? 2 : 4);
	}
	if (0 != buf->vb.baddr  &&  buf->vb.bsize < buf->vb.size)
		return -EINVAL;

	if (buf->fmt       != fh->fmt    ||
	    buf->vb.width  != fh->width  ||
	    buf->vb.height != fh->height ||
	    buf->vb.field  != field) {
		buf->fmt       = fh->fmt;
		buf->vb.width  = fh->width;
		buf->vb.height = fh->height;
		buf->vb.field  = field;
		buf->vb.state  = VIDEOBUF_NEEDS_INIT;
	}

	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
		rc = videobuf_iolock(q, &buf->vb, NULL);
		if (0 != rc)
			goto fail;
	}

	dvprintk(DPRT_LEVEL0, dev, "[%p/%d] buffer_prepare - %dx%d %dbpp \"%s\"\n",
		buf, buf->vb.i,
		fh->width, fh->height, fh->fmt->depth, fh->fmt->name);

	buf->vb.state = VIDEOBUF_PREPARED;
	buf->activate = buffer_activate;

	return 0;

 fail:
	buffer_release(q, vb);
	return rc;
}

static void buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct tw686x_buf      *buf = container_of(vb, struct tw686x_buf, vb);
	struct tw686x_fh       *fh  = vq->priv_data;
	struct tw686x_vdev      *dev= fh->dev;
	struct tw686x_dmaqueue *q   = &dev->video_q;
	bool   b_active = false;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, vb->i);

//	assert_spin_locked(dev->slock);

    b_active = IS_TW686X_DMA_MODE_BLOCK ?
               (dev->pb_next < dev->buf_needs) : (dev->pb_next<=buf->sg_ofo) ;
    if( b_active ) {
        buf->activate( dev, buf, dev->pb_curr, false );
        if( !dev->dma_started ) {
            tw686x_dev_run( dev, true );
            dev->dma_started = true;
        }
    }
    else {
        list_add_tail(&buf->vb.queue, &q->queued);
        buf->vb.state = VIDEOBUF_QUEUED;
    }
	dev->pb_next++;
}

static struct videobuf_queue_ops tw686x_video_qops = {
	.buf_setup    = buffer_setup,
	.buf_prepare  = buffer_prepare,
	.buf_queue    = buffer_queue,
	.buf_release  = buffer_release,
};

static int video_open(struct file *file)
{
	struct tw686x_chip *chip;
	struct tw686x_vdev *h=video_drvdata(file);
    struct tw686x_vdev *dev = NULL;
	struct tw686x_fh *fh;
	struct list_head *list;
	enum v4l2_buf_type type = 0;
	int  i = 0;

	mutex_lock(&tw686x_chiplist_lock);
	list_for_each(list, &tw686x_chiplist) {
		chip = list_entry(list, struct tw686x_chip, chiplist);
        for( i=0; i<chip->vid_count; i++ ) {
            if (chip->vid_dev[i] == h) {
                dev  = h;
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                break;
            }
        }
	}
    mutex_unlock(&tw686x_chiplist_lock);
	if (NULL == dev) {
		return -ENODEV;
	}
    chip = dev->chip;

	dvprintk(DPRT_LEVEL0, dev, "%s() minor=%d type=%s\n", __func__,
		dev->video_dev->minor, v4l2_type_names[type]);

	/* allocate + initialize per filehandle data */
	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (NULL == fh) {
		return -ENOMEM;
	}
	file->private_data = fh;
	fh->dev      = dev;
	fh->type     = type;
	fh->width    = TW686X_DEFAULT_WIDTH;
	fh->height   = TW686X_DEFAULT_HEIGHT;
	fh->fmt      = format_by_fourcc(V4L2_PIX_FMT_YUYV);
	fh->bytesperline = fh->width*2;
	fh->ctl_bright  = 0x00;
	fh->ctl_contrast= 0x64;
	fh->ctl_hue     = 0x00;
	fh->ctl_saturation = 0x80;
	//v4l2_prio_open(&dev->prio,&fh->prio);

 #if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
    if( IS_TW686X_DMA_MODE_BLOCK ) {
        videobuf_queue_dma_contig_init_isl(&fh->vidq, &tw686x_video_qops, &chip->pci->dev,
                    dev->slock,
                    V4L2_BUF_TYPE_VIDEO_CAPTURE,
                    V4L2_FIELD_INTERLACED,
                    sizeof(struct tw686x_buf),
                    fh, NULL);
    }
    else {
       videobuf_queue_sg_init(&fh->vidq, &tw686x_video_qops,
                    &chip->pci->dev, dev->slock,
                    V4L2_BUF_TYPE_VIDEO_CAPTURE,
                    V4L2_FIELD_INTERLACED,
                    sizeof(struct tw686x_buf),
                    fh, NULL);
    }
#else
    if( IS_TW686X_DMA_MODE_BLOCK ) {
        videobuf_queue_dma_contig_init_isl(&fh->vidq, &tw686x_video_qops, &chip->pci->dev,
                    dev->slock,
                    V4L2_BUF_TYPE_VIDEO_CAPTURE,
                    V4L2_FIELD_INTERLACED,
                    sizeof(struct tw686x_buf),
                    fh);
    }
    else {
       videobuf_queue_sg_init(&fh->vidq, &tw686x_video_qops,
                    &chip->pci->dev, dev->slock,
                    V4L2_BUF_TYPE_VIDEO_CAPTURE,
                    V4L2_FIELD_INTERLACED,
                    sizeof(struct tw686x_buf),
                    fh);
    }
#endif

	return 0;
}

static ssize_t video_read(struct file *file, char __user *data,
	size_t count, loff_t *ppos)
{
	struct tw686x_fh  *fh = file->private_data;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

    if (res_locked(dev,RESOURCE_VIDEO))
        return -EBUSY;

    return videobuf_read_one(&fh->vidq, data, count, ppos,
                 file->f_flags & O_NONBLOCK);
}

static unsigned int video_poll(struct file *file,
	struct poll_table_struct *wait)
{
	struct tw686x_fh  *fh = file->private_data;
	struct tw686x_buf *buf= NULL;
	unsigned int rc = POLLERR;
	struct tw686x_vdev *dev= fh->dev;

	mutex_lock(&fh->vidq.vb_lock);

	if (res_check(fh, RESOURCE_VIDEO)) {
		/* streaming capture */
		if (list_empty(&fh->vidq.stream))
			goto done;
		buf = list_entry(fh->vidq.stream.next,
			struct tw686x_buf, vb.stream);
	} else {
		/* read() capture */
		buf = (struct tw686x_buf *)fh->vidq.read_buf;
		if (NULL == buf)
			goto done;
	}

	poll_wait(file, &buf->vb.done, wait);
	if (buf->vb.state == VIDEOBUF_DONE ||
	    buf->vb.state == VIDEOBUF_ERROR)
		rc =  POLLIN|POLLRDNORM;
	else
		rc = 0;

done:
	mutex_unlock(&fh->vidq.vb_lock);
	dvprintk(DPRT_LEVEL0, dev, "%s() return %d\n", __func__, rc);
	return rc;
}

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
static int video_release(struct file *file)
#else
static int video_release(struct inode *inode, struct file *file)
#endif
{
	struct tw686x_fh *fh = file->private_data;
	struct tw686x_vdev *dev = fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	/* stop video capture */
	if (res_check(fh, RESOURCE_VIDEO)) {
		res_free(dev,fh,RESOURCE_VIDEO);
		tw686x_dev_run(dev, false);
		videobuf_streamoff(&fh->vidq);
	}

	if (fh->vidq.read_buf) {
		buffer_release(&fh->vidq, fh->vidq.read_buf);
		kfree(fh->vidq.read_buf);
	}

	videobuf_mmap_free(&fh->vidq);
	file->private_data = NULL;
	kfree(fh);

	return 0;
}

static int video_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct tw686x_fh  *fh = file->private_data;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

    return videobuf_mmap_mapper(&fh->vidq, vma);
}

/* ------------------------------------------------------------------ */
/* VIDEO IOCTLS                                                       */

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
	struct v4l2_format *f)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	f->fmt.pix.width        = fh->width;
	f->fmt.pix.height       = fh->height;
	f->fmt.pix.field        = fh->vidq.field;
	f->fmt.pix.pixelformat  = fh->fmt->fourcc;
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
	struct v4l2_format *f)
{
	struct tw686x_fh  *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;
	struct tw686x_fmt *fmt;
	enum v4l2_field   field;
	unsigned int      maxw, maxh;

	dvprintk(DPRT_LEVEL0, dev, "%s(fourcc=0x%x, field=%d, %dx%d)\n", __func__,
            f->fmt.pix.pixelformat, f->fmt.pix.field, f->fmt.pix.width, f->fmt.pix.height);

	fmt = format_by_fourcc(f->fmt.pix.pixelformat);
	if (NULL == fmt)
		return -EINVAL;

	field = f->fmt.pix.field;
	maxw  = norm_maxw(dev->tvnorm);
	maxh  = norm_maxh(dev->tvnorm);
	if(f->fmt.pix.width == -1) {
	    f->fmt.pix.width = fh->width;
	}
	if(f->fmt.pix.height == -1) {
	    f->fmt.pix.height = fh->height;
	}

	if (V4L2_FIELD_ANY == field) {
		field = (f->fmt.pix.height > maxh/2)
			? V4L2_FIELD_INTERLACED
			: V4L2_FIELD_BOTTOM;
	}

	switch (field) {
	case V4L2_FIELD_TOP:
	case V4L2_FIELD_BOTTOM:
		maxh = maxh / 2;
		break;
	case V4L2_FIELD_INTERLACED:
		break;
	default:
		return -EINVAL;
	}

	f->fmt.pix.field = field;
    f->fmt.pix.width &= ~0x0F;
    f->fmt.pix.height&= ~0x0F;

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
    v4l_bound_align_image(&f->fmt.pix.width, TW686X_MIN_WIDTH, maxw, 2,
                  &f->fmt.pix.height, TW686X_MIN_HEIGHT, maxh, 0, 0);
#else
    if (f->fmt.pix.height < TW686X_MIN_HEIGHT)
        f->fmt.pix.height = TW686X_MIN_HEIGHT;
    if (f->fmt.pix.height > maxh)
        f->fmt.pix.height = maxh;
    if (f->fmt.pix.width < TW686X_MIN_WIDTH)
        f->fmt.pix.width = TW686X_MIN_WIDTH;
    if (f->fmt.pix.width > maxw)
        f->fmt.pix.width = maxw;
    f->fmt.pix.width &= ~0x03;
#endif
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
	struct v4l2_format *f)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;
	int err;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	err = vidioc_try_fmt_vid_cap(file, priv, f);
	if (0 != err)
		return err;

	fh->fmt        = format_by_fourcc(f->fmt.pix.pixelformat);
	fh->width      = f->fmt.pix.width;
	fh->height     = f->fmt.pix.height;
	fh->bytesperline = f->fmt.pix.bytesperline;
	fh->vidq.field = f->fmt.pix.field;
	dev->field     = fh->vidq.field;
	if( IS_TW686X_DMA_MODE_BLOCK ) {
        dev->buf_needs = (V4L2_FIELD_HAS_BOTH(dev->field) ? 2 : 4);
	}
    else {
        dev->buf_needs = 2;
	}

	dvprintk(2, dev, "%s() width=%d height=%d field=%d\n", __func__,
		fh->width, fh->height, fh->vidq.field);

    tw686x_dev_set_pixelformat( dev, f->fmt.pix.pixelformat );

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
	call_all(dev->chip, video, s_fmt, f);
#endif
#endif

	return 0;
}

static int vidioc_querycap(struct file *file, void  *priv,
	struct v4l2_capability *cap)
{
	struct tw686x_vdev *dev  = ((struct tw686x_fh *)priv)->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	sprintf(cap->driver, "%s", "tw6869");
	sprintf(cap->card, "%s", "TW6869 PC-DVR");
	sprintf(cap->bus_info, "PCIe:%s", pci_name(dev->chip->pci));
	cap->version = TW686X_VERSION_CODE;
	cap->capabilities =
		V4L2_CAP_VIDEO_CAPTURE |
		V4L2_CAP_READWRITE     |
		V4L2_CAP_STREAMING;

	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
	struct v4l2_fmtdesc *f)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, f->index);

	if (unlikely(f->index >= ARRAY_SIZE(formats)))
		return -EINVAL;

	strlcpy(f->description, formats[f->index].name,
		sizeof(f->description));
	f->pixelformat = formats[f->index].fourcc;

	return 0;
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv,
	struct video_mbuf *mbuf)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	return 0;
}
#endif

static int vidioc_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *p)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	return videobuf_reqbufs(&fh->vidq, p);
}

static int vidioc_querybuf(struct file *file, void *priv,
	struct v4l2_buffer *p)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	return videobuf_querybuf(&fh->vidq, p);
}

static int vidioc_qbuf(struct file *file, void *priv,
	struct v4l2_buffer *p)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	return videobuf_qbuf(&fh->vidq, p);
}

static int vidioc_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *p)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

    return videobuf_dqbuf(&fh->vidq, p,
                file->f_flags & O_NONBLOCK);
}

static int vidioc_streamon(struct file *file, void *priv,
	enum v4l2_buf_type i)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(1, dev, "%s()\n", __func__);

	if (unlikely(fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE))
		return -EINVAL;
	if (unlikely(i != fh->type))
		return -EINVAL;
	if (unlikely(!res_get(dev, fh, RESOURCE_VIDEO)))
		return -EBUSY;

    dev->pb_curr = 0;
    dev->pb_next = 0;
    dev->dma_started = false;
    dev->chip->daemon_nr = 0;
    dev->chip->daemon_tor= 0;
    do_gettimeofday( &dev->chip->daemon_tv );
    if( dev->framerate==0 ) {
        dev->framerate = TW686X_MAX_FRAMERATE(dev->tvnorm);
    }

    tw686x_dev_set_dma( dev, fh->width, fh->height, (fh->width*fh->fmt->depth)>>3 );
    tw686x_dev_set_framerate( dev, dev->framerate, fh->vidq.field );

    return videobuf_streamon(&fh->vidq);
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(1, dev, "%s(%d)\n", __func__, fh->vidq.streaming);

    if (!res_check(fh, RESOURCE_VIDEO))
        return 0;

	res_free(dev, fh, RESOURCE_VIDEO);

	if( IS_TW686X_DMA_MODE_BLOCK && !fh->vidq.streaming) {
	    return 0;
	}

	return  videobuf_streamoff(&fh->vidq);
}

static int vidioc_g_std(struct file *file, void *priv, v4l2_std_id *tvnorms)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev = fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	*tvnorms = dev->tvnorm;

	return 0;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id* id)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev = fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s(%08x)\n", __func__, (u32)*id);

	tw686x_set_tvnorm(dev, *id);
	if( dev->framerate==0 ) {
	    dev->framerate = TW686X_MAX_FRAMERATE(*id);
	}

	return 0;
}

static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *i)
{
	unsigned int n;
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;
	u32    vstatus = 0;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d-%d)\n", __func__, i->index, dev->input);

   n = i->index;
	if(tw686x_bcsi) {
		n -= 1;
	}

	if(dev->channel_id<TW686X_DECODER_COUNT) {
        if (n >= TW6864_MUX_COUNT)
            return -EINVAL;
	}
	else {
        return -EINVAL;
	}

   memset(i, 0, sizeof(*i));
	i->index = n;
	if(tw686x_bcsi) {
		i->index += 1;
	}
	i->type  = V4L2_INPUT_TYPE_CAMERA;
	sprintf(i->name, "Composite%d", n);
	i->std = TW686X_NORMS;
	if(n == dev->input) {
		vstatus = dev->vstatus;
		if (0 != (vstatus & BIT(7)))
		    i->status |= V4L2_IN_ST_NO_SIGNAL;
		if (0 == (vstatus & BIT(6)))
		    i->status |= V4L2_IN_ST_NO_H_LOCK;
		if (0 != (vstatus & BIT(1)))
		    i->status |= V4L2_IN_ST_NO_COLOR;
	}

	return 0;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

    *i = dev->input;

	dvprintk(DPRT_LEVEL0, dev, "%s() returns %d\n", __func__, *i);

	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	dvprintk(DPRT_LEVEL0, dev, "%s(%d)\n", __func__, i);

    if( dev->channel_id<TW686X_DECODER_COUNT ) {
        if (i >= TW6864_MUX_COUNT) {
            dvprintk(1, dev, "%s(%d) -EINVAL\n", __func__, i);
            return -EINVAL;
        }
    }
    else if (i >= TW2865_MUX_COUNT) {
        dvprintk(1, dev, "%s(%d) -EINVAL\n", __func__, i);
        return -EINVAL;
    }

	mutex_lock(&dev->lock);
	dev->input = i;
	tw686x_dev_set_mux(dev, i);
	mutex_unlock(&dev->lock);

	return 0;
}

static int vidioc_queryctrl(struct file *file, void *priv,
				struct v4l2_queryctrl *c)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;
	const struct v4l2_queryctrl *ctrl;

	dvprintk(DPRT_LEVEL0, dev, "%s()\n", __func__);

	if ((c->id <  V4L2_CID_BASE ||
	     c->id >= V4L2_CID_LASTP1))
		return -EINVAL;
	ctrl = ctrl_by_id(c->id);
	*c = (NULL != ctrl) ? *ctrl : no_ctrl;

	return 0;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
				struct v4l2_control *c)
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;
	const struct v4l2_queryctrl* ctrl;

	ctrl = ctrl_by_id(c->id);
	if (NULL == ctrl)
		return -EINVAL;
	switch (c->id) {
	case V4L2_CID_BRIGHTNESS:
		c->value = fh->ctl_bright;
		break;
	case V4L2_CID_HUE:
		c->value = fh->ctl_hue;
		break;
	case V4L2_CID_CONTRAST:
		c->value = fh->ctl_contrast;
		break;
	case V4L2_CID_SATURATION:
		c->value = fh->ctl_saturation;
		break;
	default:
		return -EINVAL;
	}

	dvprintk(DPRT_LEVEL0, dev, "%s(id=%x) return %d\n", __func__, c->id, c->value);

	return 0;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *c)
{
	const struct v4l2_queryctrl* ctrl;
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;

	mutex_lock(&dev->lock);
	ctrl = ctrl_by_id(c->id);
	if (NULL == ctrl) {
        mutex_unlock(&dev->lock);
	    return -EINVAL;
	}

	dvprintk(DPRT_LEVEL0, dev, "set_control name=%s val=%d\n",ctrl->name,c->value);

	switch (ctrl->type) {
	case V4L2_CTRL_TYPE_BOOLEAN:
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER:
		if (c->value < ctrl->minimum)
			c->value = ctrl->minimum;
		if (c->value > ctrl->maximum)
			c->value = ctrl->maximum;
		break;
	default:
		/* nothing */;
	};
	switch (c->id) {
	case V4L2_CID_BRIGHTNESS:
		fh->ctl_bright = c->value;
		tw686x_dev_set_bright( dev, fh->ctl_bright );
		break;
	case V4L2_CID_HUE:
		fh->ctl_hue = c->value;
		tw686x_dev_set_hue( dev, fh->ctl_hue );
		break;
	case V4L2_CID_CONTRAST:
		fh->ctl_contrast = c->value;
		tw686x_dev_set_contrast( dev, fh->ctl_contrast );
		break;
	case V4L2_CID_SATURATION:
		fh->ctl_saturation = c->value;
		tw686x_dev_set_saturation( dev, fh->ctl_saturation );
		break;
	default:
        break;
	}
	mutex_unlock(&dev->lock);

	return 0;
}

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
static int vidioc_g_register(struct file *file, void *priv,
				struct v4l2_dbg_register *reg)
#else
static int vidioc_g_register(struct file *file, void *priv,
				struct v4l2_register *reg)
#endif
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;
	struct tw686x_chip *chip = dev->chip;

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	if (!v4l2_chip_match_host(&reg->match))
		return -EINVAL;
#else
	if (!v4l2_chip_match_host(reg->match_type, reg->match_chip))
		return -EINVAL;
#endif

    if( reg->reg >= 0x800 ) {
        return -EINVAL;
    }

    reg->reg &= ~0x03;

#ifdef CONFIG_VIDEO_ADV_DEBUG
	call_all(dev, core, g_register, reg);
#else
    reg->val = tw_read( reg->reg );
#endif

	//dvprintk(1, dev, "%s(0x%08x) = 0x%08x\n", __func__, (u32)reg->reg, (u32)reg->val);

	return 0;
}

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
static int vidioc_s_register(struct file *file, void *priv,
				struct v4l2_dbg_register *reg)
#else
static int vidioc_s_register(struct file *file, void *priv,
				struct v4l2_register *reg)
#endif
{
	struct tw686x_fh *fh   = priv;
	struct tw686x_vdev *dev= fh->dev;
	struct tw686x_chip *chip = dev->chip;

	//dvprintk(1, dev, "%s(0x%08x, 0x%08x)\n", __func__, (u32)reg->reg, (u32)reg->val);

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	if (!v4l2_chip_match_host(&reg->match))
		return -EINVAL;
#else
	if (!v4l2_chip_match_host(reg->match_type, reg->match_chip))
		return -EINVAL;
#endif

    if( reg->reg >= 0x800 ) {
        return -EINVAL;
    }

    reg->reg &= ~0x03;

#ifdef CONFIG_VIDEO_ADV_DEBUG
	call_all(dev, core, s_register, reg);
#else
    tw_write( reg->reg, reg->val );
#endif

	return 0;
}

/* For other private ioctls */
#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,38))
static long vidioc_tw686x(struct file *file, void *priv,
					bool valid_prio, int cmd, void *arg)
#elif(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
static long vidioc_tw686x(struct file *file, void *priv,
					int cmd, void *arg)
#else
static int vidioc_tw686x(struct file *file, void *priv,
					int cmd, void *arg)
#endif
{
	struct tw686x_fh *fh = priv;
	struct tw686x_vdev *dev= fh->dev;
	long ret = -EINVAL;

	//dvprintk(1, dev, "%s()\n", __func__);

	switch( cmd ) {
	case TW686X_S_REGISTER:
		ret = vidioc_s_register( file, priv, arg );
		break;
	case TW686X_G_REGISTER:
		ret = vidioc_g_register( file, priv, arg );
		break;
	case TW686X_S_FRAMERATE:
		dev->framerate = *(long*)arg;
		tw686x_dev_set_framerate( dev, dev->framerate, fh->vidq.field );
		ret = 0;
		break;
	case TW686X_G_FRAMERATE:
		*(long*)arg = dev->framerate;
		ret = 0;
		break;
	case TW686X_G_SIGNAL:
		*(long*)arg = tw686x_dev_get_signal( dev );
		ret = 0;
		break;
#if(CaptureFLV)
    case TW686X_S_AUDIO:
        ret = tw686x_audio_param( dev->chip->aud_dev[dev->channel_id], (struct tw686x_aparam*)arg);
        break;
    case TW686X_G_AUDIO:
        ret = tw686x_audio_data( dev->chip->aud_dev[dev->channel_id], (struct tw686x_adata*)arg);
        break;
#endif
	default:
		break;
	}

	return ret;
}

int tw686x_video_resetdma(struct tw686x_chip *chip, u32 dma_cmd)
{
#if(TW6869_RESETDMA)
    chip->dma_restart = dma_cmd;
	mod_timer(&chip->tm_assist, jiffies+(BUFFER_TIMEOUT/25));
#else
    struct tw686x_vdev *dev = NULL;
    int i=0;

    for(i=0; i<chip->vid_count; i++) {
        if(dma_cmd & (1<<i)) {
            dev = chip->vid_dev[i];
            tw686x_video_restart( dev, false );
        }
    }
#endif

    return 0;
}

int tw686x_video_daemon(struct tw686x_chip *chip, u32 dma_error)
{
    int i, should_dropdown = 0;
    struct tw686x_vdev *dev = NULL;
    struct timeval tv_now;
    long long tv_diff = 0;
    long ms_diff = 0;

    do_gettimeofday( &tv_now );
    tv_diff = 1000000 * ( tv_now.tv_sec - chip->daemon_tv.tv_sec ) +
              tv_now.tv_usec - chip->daemon_tv.tv_usec;
    ms_diff = (long)tv_diff;
    ms_diff/= 1000;

    for( i=0; i<chip->vid_count; i++ ) {
        if( dma_error&TW6864_R_DMA_VINT_FIFO(i) ) {
            chip->daemon_nr++;
        }
    }

    if( chip->daemon_nr >= TW686X_DAEMON_THRESHOLD ) {
        chip->daemon_tor++;
        chip->daemon_nr = 0;
    }

    if(ms_diff >= TW686X_DAEMON_PERIOD) {
        should_dropdown = (chip->daemon_tor >= TW686X_DAEMON_TOLERANCE);
        chip->daemon_tv = tv_now;
        chip->daemon_tor= 0;;
        chip->daemon_nr = 0;
    }

    if( !should_dropdown ) {
        for( i=0; i<chip->vid_count; i++ ) {
            dev = chip->vid_dev[i];
            if( dma_error&TW6864_R_DMA_VINT_FIFO(i) ) {
                tw686x_video_restart( dev, false );
            }
        }
    }
    else {
        for( i=0; i<chip->vid_count; i++ ) {
            dev = chip->vid_dev[i];
            if(dev->framerate > TW686X_DAEMON_MIN) {
                dev->framerate--;
                tw686x_video_restart( dev, true );
            }
            else if( dma_error&TW6864_R_DMA_VINT_FIFO(i) ) {
                tw686x_video_restart( dev, false );
            }
        }
    }

    return 0;
}

/* ----------------------------------------------------------- */

int tw686x_video_irq(struct tw686x_vdev *dev, u32 dma_status, u32 pb_status)
{
/*liran for debug
    struct tw686x_chip *chip = dev->chip;

    dvprintk(DPRT_LEVEL0, dev, "irq buf=0x%08x\n", ibuf==0 ?
        tw_read(TW6864_R_BDMA(TW6864_R_BDMA_ADDR_P_0, dev->channel_id)) :
        tw_read(TW6864_R_BDMA(TW6864_R_BDMA_ADDR_B_0, dev->channel_id)));
*/

 	if( !res_locked(dev, RESOURCE_VIDEO) ) {
		tw686x_dev_run( dev, false );
		while (!list_empty(&dev->video_q.active)) {
			tw686x_buffer_wakeup( dev, &dev->video_q, VIDEOBUF_ERROR );
		}
        while (!list_empty(&dev->video_q.queued)) {
            tw686x_buffer_cancel( dev, &dev->video_q, VIDEOBUF_ERROR );
        }
	}
	else {
		if( !IS_TW686X_DMA_MODE_BLOCK ) {
			dev->pb_curr = (pb_status >> dev->channel_id) & 1;
		}
		if( tw686x_buffer_wakeup(dev, &dev->video_q, VIDEOBUF_DONE) ) {
		}
	   	if(dev->pb_next>0) {
			tw686x_buffer_next(dev, &dev->video_q, dev->pb_curr);
		}
		else {
			tw686x_dev_run( dev, false );
			dev->dma_started = false;
			dev->pb_next = 0;
			dev->pb_curr = 0;
		}
	}

	return 0;
}

int tw686x_video_status(struct tw686x_vdev *dev)
{
	dev->vstatus = tw686x_dev_get_decoderstatus(dev);

	return 0;
}

/* ----------------------------------------------------------- */
/* exported stuff                                              */

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
static const struct v4l2_file_operations video_fops = {
	.owner	       = THIS_MODULE,
	.open	       = video_open,
	.release       = video_release,
	.read	       = video_read,
	.poll          = video_poll,
	.mmap	       = video_mmap,
	.ioctl	       = video_ioctl2,
};
#else
static const struct file_operations video_fops = {
	.owner	       = THIS_MODULE,
	.open	       = video_open,
	.release       = video_release,
	.read	       = video_read,
	.poll          = video_poll,
	.mmap	       = video_mmap,
	.ioctl	       = video_ioctl2,
	.compat_ioctl  = v4l_compat_ioctl32,
	.llseek        = no_llseek,
};
#endif

static const struct v4l2_ioctl_ops video_ioctl_ops = {
	.vidioc_querycap      = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap  = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs       = vidioc_reqbufs,
	.vidioc_querybuf      = vidioc_querybuf,
	.vidioc_qbuf          = vidioc_qbuf,
	.vidioc_dqbuf         = vidioc_dqbuf,
	.vidioc_g_std         = vidioc_g_std,
	.vidioc_s_std         = vidioc_s_std,
	.vidioc_enum_input    = vidioc_enum_input,
	.vidioc_g_input       = vidioc_g_input,
	.vidioc_s_input       = vidioc_s_input,
	.vidioc_queryctrl     = vidioc_queryctrl,
	.vidioc_g_ctrl        = vidioc_g_ctrl,
	.vidioc_s_ctrl        = vidioc_s_ctrl,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf          = vidiocgmbuf,
#endif
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.vidioc_g_register    = vidioc_g_register,
	.vidioc_s_register    = vidioc_s_register,
#endif
	.vidioc_default       = vidioc_tw686x,
};

static struct video_device tw686x_video_template = {
	.name                 = "tw6869-video",
	.fops                 = &video_fops,
	.minor                = -1,
	.ioctl_ops 	          = &video_ioctl_ops,
	.tvnorms              = TW686X_NORMS,
	.current_norm         = V4L2_STD_NTSC,
};

int tw686x_video_register(struct tw686x_vdev *dev)
{
	int err;
	struct video_device *vfd;

	dvprintk(1, dev, "%s()\n", __func__);

	mutex_init(&dev->lock);

	/* init video dma queues */
	INIT_LIST_HEAD(&dev->video_q.active);
	INIT_LIST_HEAD(&dev->video_q.queued);
	dev->video_q.timeout.function = tw686x_buffer_timeout;
	dev->video_q.timeout.data = (unsigned long)dev;
	init_timer(&dev->video_q.timeout);
	dev->tm_restart.function = tw686x_video_restart_timeout;
	dev->tm_restart.data = (unsigned long)dev;
	init_timer(&dev->tm_restart);

    dev->tvnorm  = TW686X_DEFAULT_STANDARD;
    dev->field   = V4L2_FIELD_TOP;

    /* create v4l devices */
	vfd = video_device_alloc();
	if (vfd) {
        *vfd = tw686x_video_template;
        vfd->minor = -1;
#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
        vfd->v4l2_dev = &dev->chip->v4l2_dev;
#endif
        vfd->release = video_device_release;
        snprintf(vfd->name, sizeof(vfd->name), "%s %s %d",
             dev->chip->name, "video", dev->channel_id);
        video_set_drvdata(vfd, dev);
	}

	/* register v4l devices */
	dev->video_dev = vfd;
	err = video_register_device(dev->video_dev, VFL_TYPE_GRABBER, tw686x_vidcount++);
	if (err < 0) {
		printk(KERN_INFO "%s: can't register video device %d\n", dev->chip->name, tw686x_vidcount);
		goto fail_unreg;
	}
#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	printk(KERN_INFO "%s: registered device video %d [v4l2]\n",
	       dev->chip->name, dev->video_dev->num);
#else
	printk(KERN_INFO "%s: registered device video %d [v4l2]\n",
	       dev->chip->name, dev->video_dev->minor & 0x1f);
#endif

	/* initial device configuration */
	tw686x_set_tvnorm(dev, dev->tvnorm);
	tw686x_dev_set_mux(dev, 0);

	return 0;

fail_unreg:
	tw686x_video_unregister(dev);
	return err;
}

void tw686x_video_unregister(struct tw686x_vdev *dev)
{
	dvprintk(1, dev, "%s()\n", __func__);

	if (dev->video_dev) {
		if (-1 != dev->video_dev->minor)
			video_unregister_device(dev->video_dev);
		else
			video_device_release(dev->video_dev);
		dev->video_dev = NULL;
	}
}
