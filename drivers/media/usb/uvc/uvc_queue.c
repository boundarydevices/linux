/*
 *      uvc_queue.c  --  USB Video Class driver - Buffers management
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 */

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>

#include "uvcvideo.h"

static int cacheable;

module_param(cacheable, int, 0);
MODULE_PARM_DESC(cacheable, "Use cacheable memory for returned buffer");

//static int hcacheable;
//module_param(hcacheable, int, 0);
//MODULE_PARM_DESC(hcacheable, "Use cacheable memory for headers buffer");

//currently, they both have to be the same
#define hcacheable cacheable

//struct dma_attrs uvc_dma_attrs;
/* ------------------------------------------------------------------------
 * Video buffers queue management.
 *
 * Video queues is initialized by uvc_queue_init(). The function performs
 * basic initialization of the uvc_video_queue struct and never fails.
 *
 * Video buffers are managed by videobuf2. The driver uses a mutex to protect
 * the videobuf2 queue operations by serializing calls to videobuf2 and a
 * spinlock to protect the IRQ queue that holds the buffers to be processed by
 * the driver.
 */

static inline struct uvc_streaming *
uvc_queue_to_stream(struct uvc_video_queue *queue)
{
	return container_of(queue, struct uvc_streaming, queue);
}

static struct uvc_buffer *uvc_vbuf_to_buffer(struct vb2_v4l2_buffer *buf)
{
	return container_of(buf, struct uvc_buffer, buf);
}

static struct uvc_buffer *uvc_vb2_to_buffer(struct vb2_buffer *vb)
{
	return uvc_vbuf_to_buffer(to_vb2_v4l2_buffer(vb));
}

/* -----------------------------------------------------------------------------
 * videobuf2 queue operations
 */

static int uvc_queue_setup(struct vb2_queue *vq,
			   unsigned int *nbuffers, unsigned int *nplanes,
			   unsigned int sizes[], struct device *alloc_devs[])
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vq);
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);
	unsigned size = stream->ctrl.dwMaxVideoFrameSize;
	unsigned npackets;
	unsigned psize;

	if (*nbuffers > UVC_MAX_VIDEO_BUFFERS)
		*nbuffers = UVC_MAX_VIDEO_BUFFERS;

	/* Make sure the image size is large enough. */
	if (*nplanes && (sizes[0] < size))
		return  -EINVAL;

//	dma_set_attr(DMA_ATTR_FORCE_CONTIGUOUS, &uvc_dma_attrs);
//	dma_set_attr(DMA_ATTR_WRITE_COMBINE, &uvc_dma_attrs);

	*nplanes = 1;
	psize = stream->psize;
	if (queue->dma_mode == DMA_MODE_CONTIG) {
		/*
		 * let the last segment transfer an entire payload
		 * in case things are not yet synced
		 */
		if (!stream->iso_packets)
			size += stream->max_payload_size;
		if (*nbuffers < 4)
			*nbuffers = 4;
	}
	if (0) pr_info("%s:psize = %x size=%x\n", __func__, psize, size);
	npackets = DIV_ROUND_UP(size, psize);
	size = npackets * psize;
	sizes[0] = size;
	queue->fake_buf.length = size;

	if (size > stream->max_payload_size)
		size = stream->max_payload_size;
	stream->npackets = size / stream->psize;
	if (stream->iso_packets)
		stream->iso_packets = stream->npackets;
	if (0) pr_info("%s:psize = %x size=%x npackets=%x\n", __func__, psize, size, stream->npackets);

	if ((queue->dma_mode == DMA_MODE_CONTIG) && !alloc_devs[0]) {
		dev_dbg(&stream->dev->udev->dev, "setting dma_ops\n");
		dma_set_mask_and_coherent(&stream->dev->udev->dev, DMA_BIT_MASK(32));
		dma_set_mask_and_coherent(stream->dev->udev->dev.parent, DMA_BIT_MASK(32));
#if 0
		if (cacheable)
			arch_setup_dma_ops(&stream->dev->udev->dev, 0, 0, NULL, true);
#endif
	}
	return 0;
}

static int setup_buf(struct uvc_streaming *stream, struct uvc_buffer *buf)
{
	dma_addr_t phys, h_phys;
	u8 *mem, *h_mem;
	unsigned repeat_payload;
	int i = 0;
	unsigned rem, h_rem, frame_rem;
	unsigned psize = stream->psize;
	unsigned header_copy_sz;
	unsigned header_sz = 0;
	unsigned mps, max_payload_size;
	unsigned rp;
	struct urb *urb;
	int tf = URB_NO_TRANSFER_DMA_MAP;

	if (!buf->urb_cnt)
		return 0;

	h_mem = buf->header_buf;
	repeat_payload = max_payload_size = stream->max_payload_size;
	if (h_mem) {
		header_sz = stream->header_sz;
		if (stream->repeat_payload)
			repeat_payload = stream->repeat_payload;
	}

	if ((header_sz != buf->header_sz) ||
			(repeat_payload != buf->repeat_payload)) {
		if (0) pr_info("%s: buf=%p mem %p, length %x, setup_done=%x\n", __func__,
				buf, buf->mem, buf->length, buf->setup_done);
		buf->setup_done = 0;
	}

	if (buf->setup_done)
		return 0;

	if (0) pr_info("%s:b\n", __func__);
	mem = buf->mem;
	phys = vb2_dma_contig_plane_dma_addr(&buf->buf.vb2_buf, 0);
	rem = buf->length;

	h_phys = buf->header_phys;
	h_rem = buf->header_buf_len;

	buf->repeat_payload = repeat_payload;
	if (!header_sz)
		header_sz = 12;
	buf->header_sz = header_sz;

	frame_rem = stream->ctrl.dwMaxVideoFrameSize;
	header_copy_sz = psize - header_sz;

	while (1) {
		if (i >= buf->urb_cnt) {
			pr_err("%s: not enough urbs\n", __func__);
			break;
		}
		mps = max_payload_size;
		rp = repeat_payload;
		if (frame_rem > header_copy_sz) {
			if (h_rem < psize) {
				pr_err("%s: header buf too small\n", __func__);
				break;
			}
			if ((rem - header_copy_sz) < mps) {
				pr_err("%s: buf too small\n", __func__);
				break;
			}
			if ((i + 1) >= buf->urb_cnt) {
				pr_err("%s: not enough urbs\n", __func__);
				break;
			}
			urb = buf->urbs[i++];
			stream->init_urb(stream, urb,
				&buf->buf,
				h_mem, psize, h_phys, URB_NO_TRANSFER_DMA_MAP);
			if (0) pr_info("%s:(%d) h_mem=%p len=%x\n", __func__, i-1, h_mem, psize);
			h_mem += psize;
			h_phys += psize;
			h_rem -= psize;

			mem += header_copy_sz;
			phys += header_copy_sz;
			rem -= header_copy_sz;
			frame_rem -= header_copy_sz;

			mps -= psize;
			rp -= psize;
			if (frame_rem <= rp)
				mps += psize;
		} else {
			if (rem < mps) {
				pr_err("%s: buf too small\n", __func__);
				break;
			}
		}

		urb = buf->urbs[i++];
		stream->init_urb(stream, urb,
				&buf->buf,
				mem, mps, phys, tf);
		if (0) pr_info("%s:(%d) mem=%p len=%x\n", __func__, i-1, mem, mps);
		if (frame_rem <= rp)
			break;
		mem += rp;
		phys += rp;
		rem -= rp;
		frame_rem -= rp;
	}
	buf->eof_transfer_length = frame_rem;
	if (i & 1)
		buf->eof_transfer_length += header_sz;

	buf->used_urb_cnt = i;
	buf->setup_done = 1;
	pr_debug("%s: used %u of %u, eof_transfer_length=%x\n", __func__,
			i, buf->urb_cnt, buf->eof_transfer_length);
	return 0;
}

void uvc_buffer_done(struct uvc_buffer *buf, int state, const char *s)
{
	if (!buf->mem)
		return;
	if (0) pr_info("%s:%s: %p mem=%p from %x to %x, bytesused=%x\n",
		__func__, s, buf, buf->mem,
		buf->owner, UVC_OWNER_QUEUE, buf->bytesused);
	vb2_set_plane_payload(&buf->buf.vb2_buf, 0, buf->bytesused);
	buf->owner = UVC_OWNER_QUEUE;
	buf->error = 0;
	buf->bytesused = 0;
	buf->ready = 0;
	buf->ts = 0;
	vb2_buffer_done(&buf->buf.vb2_buf, state);
}

static void add_to_available(struct uvc_video_queue *queue, struct uvc_buffer *buf)
{
	unsigned long flags;

	if ((buf == queue->in_progress) || (buf->owner == UVC_OWNER_QUEUE)
			|| !buf->mem)
		return;
	if (queue->return_buffers) {
		/* If the device is disconnected return the buffer to userspace
		 * directly. The next QBUF call will fail with -ENODEV.
		 */
		uvc_buffer_done(buf, VB2_BUF_STATE_ERROR, __func__);
		return;
	}

	buf->for_cpu = 0;

	if (!buf->buf_dma_handle)
		buf->cpu_dirty = 0;


	spin_lock_irqsave(&queue->irqlock, flags);
	if (buf->usb_active) {
		buf->owner = UVC_OWNER_USB_ACTIVE;
	} else {
		unsigned mask = (1 << buf->buf.vb2_buf.index);

		if (!(queue->pending & mask)) {
			if (buf->cpu_dirty) {
				queue_work(queue->workqueue, &buf->to_dev_work);
			} else {
				queue->available |= mask;
				buf->owner = UVC_OWNER_AVAIL;
			}
		}
	}
	spin_unlock_irqrestore(&queue->irqlock, flags);
	return;
}

static struct uvc_buffer *uvc_get_available_buffer(struct uvc_video_queue *queue, int for_dma)
{
	unsigned long flags;
	struct vb2_buffer *vb;
	struct uvc_buffer *buf = NULL;
	int buf_index;

	spin_lock_irqsave(&queue->irqlock, flags);
	if (queue->available) {
		buf_index = __ffs(queue->available);

		queue->available &= ~(1 << buf_index);
		vb = queue->queue.bufs[buf_index];
		buf = uvc_vb2_to_buffer(vb);
		buf->owner = UVC_OWNER_USB;
		buf->error = 0;
		buf->bytesused = 0;
		buf->ready = 0;
		buf->ts = 0;
	}
	spin_unlock_irqrestore(&queue->irqlock, flags);

	if (0) pr_info("%s: %p, available=%x for_dma=%x\n", __func__, buf,
			queue->available, for_dma);
	return buf;
}

static int submit_buffer(struct uvc_video_queue *queue, struct uvc_streaming *stream)
{
	struct uvc_buffer *buf;
	unsigned long flags;
	int i;
	int first;
	int buf_index;
	int sof_index;

	if ((queue->submitted_insert_shift >= (6 * 5)) || !queue->streaming)
		return 0;
	i = 0;
	{
		unsigned s = queue->submitted;

		/* Only have 2 buffers submitted for DMA */
		if (s != (s & -s))
			return 0;
	}

	sof_index = queue->sof_index;

	if (sof_index && stream->header_sz
			&& stream->repeat_payload
			&& queue->using_headers
			&& !queue->frame_sync_mask) {
		/*
		 * Now determine how many payloads are left to fill latest buffer
		 */

		buf = uvc_get_available_buffer(queue, 1);
		if (!buf)
			return 0;
		setup_buf(stream, buf);
		queue->sof_index = 0;

		if (buf->used_urb_cnt > sof_index) {
			i = buf->used_urb_cnt - sof_index;
			queue->frame_sync_mask = 1 << buf->buf.vb2_buf.index;
			if (0) pr_info("%s: syncing %i\n", __func__, i);
		}
	} else {
		buf = uvc_get_available_buffer(queue, 1);
		if (!buf)
			return 0;
		setup_buf(stream, buf);
	}

	/* Submit the URBs. */
	first = i;
	buf_index = buf->buf.vb2_buf.index;

	spin_lock_irqsave(&queue->irqlock, flags);
	buf->owner = UVC_OWNER_USB_ACTIVE;
	buf->prev_completed_urb = NULL;
	buf->last_completed_urb = NULL;
	buf->usb_active = 1;
	buf->ready_urb_index = i;
	buf->pending_urb_index = i;
	buf->pending_dma_index = i;
	queue->submitted_buffers |= buf_index << queue->submitted_insert_shift;
	queue->submitted_insert_shift += 5;
	queue->ready_buffers |= buf_index << queue->ready_insert_shift;
	queue->ready_insert_shift += 5;

	queue->submitted |= (1 << buf_index);
	queue->pending |= (1 << buf_index);
	spin_unlock_irqrestore(&queue->irqlock, flags);
	if (0) pr_info("%s: start %d total %d\n", __func__, i, buf->used_urb_cnt);
	if (0) pr_info("%s: buf=%p(%i) urbs %u-%u submitted=%x avail=%x\n",
			__func__, buf, buf->buf.vb2_buf.index,
			first, i, queue->submitted, queue->available);
	return 1;
}

/* spinlock is held when called */
static int drop_from_submitted(struct uvc_video_queue *queue, unsigned drop)
{
	int pos = queue->submitted_insert_shift;
	unsigned buf_index;
	unsigned h,l;

	while (pos > 0) {
		pos -= 5;
		buf_index = (queue->submitted_buffers >> pos) & 0x1f;
		if (buf_index == drop) {
			h = queue->submitted_buffers >> (pos + 5);
			l = queue->submitted_buffers & ((1 << pos) - 1);
			queue->submitted_buffers = l | (h << pos);
			queue->submitted_insert_shift -= 5;
			return 0;
		}
	}
	return -EINVAL;
}

/* spinlock is held when called */
static void drop_from_ready(struct uvc_video_queue *queue, int pos)
{
	unsigned h,l;

	h = queue->ready_buffers >> (pos + 5);
	l = queue->ready_buffers & ((1 << pos) - 1);
	queue->ready_buffers = l | (h << pos);
	queue->ready_insert_shift -= 5;
}

static int submit_ready_urbs(struct uvc_video_queue *queue,
		struct uvc_buffer *buf, unsigned outstanding, gfp_t mem_flags)
{
	int i = buf->ready_urb_index;

	while (i < buf->used_urb_cnt) {
		int ret;

		if ((outstanding >= UVC_MAX_BULK_URBS_OUTSTANDING) ||
				!queue->streaming)
			return 0;

		ret = usb_submit_urb(buf->urbs[i], mem_flags);
		if (ret < 0) {
			if (queue->streaming)
				pr_err("%s: Failed to submit URB "
					"%u (%d).\n",
					__func__, i, ret);
			return 0;
		}
		if (0) pr_info("%s:3 %u of %u\n", __func__, i, buf->used_urb_cnt);
		i++;
		buf->ready_urb_index = i;
		outstanding++;
	}
	return outstanding;
}

int uvc_submit_ready_buffers(struct uvc_video_queue *queue, gfp_t mem_flags)
{
	int buf_index;
	struct vb2_buffer *vb;
	struct uvc_buffer *buf;
	unsigned outstanding = 0;
	int cnt;
	int ret;
	int pos = 0;
	unsigned long flags;

	if (!mutex_trylock(&queue->ready_mutex))
		return 0;

	while (pos < queue->ready_insert_shift) {
		buf_index = (queue->ready_buffers >> pos) & 0x1f;

		vb = queue->queue.bufs[buf_index];
		buf = uvc_vb2_to_buffer(vb);
		cnt = buf->ready_urb_index - buf->pending_dma_index;

		pr_debug("%s: buf_index=%d, pos=%d, ready_buffers=%x %d %d\n", __func__,
				buf_index, pos, queue->ready_buffers,
				cnt, !queue->streaming);
		if ((cnt <= 0) && !queue->streaming) {
			/* we can free this buffer */
			spin_lock_irqsave(&queue->irqlock, flags);
			drop_from_ready(queue, pos);

			queue->submitted &= ~(1 << buf_index);
			queue->pending &= ~(1 << buf_index);
			buf->usb_active = 0;
			ret = drop_from_submitted(queue, buf_index);
			spin_unlock_irqrestore(&queue->irqlock, flags);

			if (ret >= 0)
				add_to_available(queue, buf);
			continue;
		}
		if (buf->ready_urb_index >= buf->used_urb_cnt) {
			/* This buffer has all been submitted */
			if (cnt > 0) {
				outstanding += cnt;
				pos += 5;
				continue;
			}
			/* all urbs have completed */
			spin_lock_irqsave(&queue->irqlock, flags);
			drop_from_ready(queue, pos);
			spin_unlock_irqrestore(&queue->irqlock, flags);
			continue;
		}
		outstanding += cnt;
		outstanding = submit_ready_urbs(queue, buf,
						outstanding, mem_flags);
		if (!outstanding)
			break;
		pos += 5;
	}
	mutex_unlock(&queue->ready_mutex);
	return 0;
}

static void submit_buffers(struct uvc_video_queue *queue, gfp_t mem_flags)
{
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);

	while (submit_buffer(queue, stream))
		;
	uvc_submit_ready_buffers(queue, mem_flags);
	if (0) pr_info("%s:e submitted=%x avail=%x\n", __func__, queue->submitted, queue->available);
}

void uvc_queue_start_work(struct uvc_video_queue *queue, struct uvc_buffer *buf)
{
	if (buf)
		add_to_available(queue, buf);

	if (queue->workqueue && queue->streaming && queue->available
			&& (queue->submitted_insert_shift < (6 * 5))) {
		unsigned s = queue->submitted;

		/* Only have 2 buffers submitted for DMA */
		if (s == (s & -s))
			queue_work(queue->workqueue, &queue->work);
	}
}

struct uvc_buffer *uvc_get_first_pending(struct uvc_video_queue *queue)
{
	struct vb2_buffer *vb;
	struct uvc_buffer *buf = NULL;
	unsigned buf_index;
	unsigned long flags;
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);

	if (!queue->submitted_insert_shift)
		return NULL;
	spin_lock_irqsave(&queue->irqlock, flags);
	while (1) {
		if (!queue->submitted_insert_shift) {
			spin_unlock_irqrestore(&queue->irqlock, flags);
			return NULL;
		}
		buf_index = queue->submitted_buffers & 0x1f;
		if (buf_index != 0x1f)
			break;
		/* too much work pending, frame was dropped */
		queue->submitted_buffers >>= 5;
		queue->submitted_insert_shift -= 5;

		stream->last_fid ^= UVC_STREAM_FID;
		stream->sequence++;	/* Note lost frame */
		buf = queue->in_progress;
		if (buf && buf->bytesused) {
			buf->error = 1;
			stream->bulk.skip_payload = 1;
		}
	}
	vb = queue->queue.bufs[buf_index];
	buf = uvc_vb2_to_buffer(vb);
	if (!buf->for_cpu || buf->usb_active)
		buf = NULL;
	spin_unlock_irqrestore(&queue->irqlock, flags);
	return buf;
}

struct uvc_buffer *uvc_get_next_pending(struct uvc_video_queue *queue)
{
	struct vb2_buffer *vb;
	struct uvc_buffer *buf;
	unsigned buf_index;
	unsigned long flags;

	if (!queue->submitted_insert_shift)
		return NULL;

	spin_lock_irqsave(&queue->irqlock, flags);
	if (!queue->submitted_insert_shift) {
		spin_unlock_irqrestore(&queue->irqlock, flags);
		return NULL;
	}
	buf_index = queue->submitted_buffers;
	queue->submitted_buffers = buf_index >> 5;
	queue->submitted_insert_shift -= 5;
	buf_index &= 0x1f;
	queue->pending &= ~(1 << buf_index);
	spin_unlock_irqrestore(&queue->irqlock, flags);

	if (buf_index < 0x1f) {
		vb = queue->queue.bufs[buf_index];
		buf = uvc_vb2_to_buffer(vb);
		uvc_queue_start_work(queue, buf);
	}
	return uvc_get_first_pending(queue);
}

static void submit_work(struct work_struct *work)
{
	struct uvc_video_queue *queue = container_of(work, struct uvc_video_queue, work);

	if (queue->streaming)
		submit_buffers(queue, GFP_KERNEL);
}

static void uvc_to_dev_work(struct work_struct *work)
{
	struct uvc_buffer* buf = container_of(work, struct uvc_buffer, to_dev_work);
	struct vb2_queue* vq =  buf->buf.vb2_buf.vb2_queue;
	struct uvc_video_queue *queue = vb2_get_drv_priv(vq);
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);

	dma_sync_single_for_device(get_mdev(stream),
		buf->buf_dma_handle, buf->length, DMA_FROM_DEVICE);
	buf->cpu_dirty = 0;
	uvc_queue_start_work(queue, buf);
}

static void cache_processing_work(struct work_struct *work)
{
	struct uvc_buffer* rbuf = container_of(work, struct uvc_buffer, cache_work);
	struct vb2_queue* vq =  rbuf->buf.vb2_buf.vb2_queue;
	struct uvc_video_queue *queue = vb2_get_drv_priv(vq);
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);
	int i;
	unsigned header_sz = rbuf->header_sz;
	unsigned psize = stream->psize;
	unsigned header_copy_sz = psize - header_sz;
	unsigned rp = rbuf->repeat_payload - psize;
	unsigned combined_payload = 0;
	u8 *combined_start = NULL;
	u8 *dest;

	dma_sync_single_for_cpu(get_mdev(stream),
			rbuf->buf_dma_handle,
			rbuf->length, DMA_FROM_DEVICE);
	if (rbuf->hbuf_dma_handle)
		dma_sync_single_for_cpu(get_mdev(stream),
			rbuf->hbuf_dma_handle,
			rbuf->header_buf_len, DMA_FROM_DEVICE);

	if (!header_sz)
		goto exit1;

	i = rbuf->pending_urb_index;

	while (1) {
		struct urb *urb = rbuf->urbs[i++];
		struct urb *next = rbuf->urbs[i++];
		u8 *mem;
		int len;

		if (i > rbuf->used_urb_cnt) {
			i -= 2;
			break;
		}
		mem = urb->transfer_buffer;
		len = urb->actual_length;
		if ((len != psize) || (mem[0] != header_sz)) {
			i -= 2;
			break;
		}
		dest = next->transfer_buffer - header_copy_sz;
		memcpy(dest, mem + header_sz, header_copy_sz);
		rbuf->cpu_dirty = 1;
		if (!combined_start)
			combined_start = dest;
		dest += header_copy_sz;
		combined_payload += len;
		if ((next->actual_length != rp) || (i >= rbuf->used_urb_cnt)) {
			i--;
			break;
		}
		combined_payload += rp;
		dest += rp;
	}
	if (combined_start) {
		rbuf->combined_start = combined_start;
		rbuf->combined_cnt = dest - combined_start;
		rbuf->combined_payload = combined_payload;
		rbuf->pending_urb_index = i;
	} else {
exit1:
		rbuf->combined_start = NULL;
		rbuf->combined_cnt = 0;
		rbuf->combined_payload = 0;
	}
	rbuf->for_cpu = 1;
	queue_work(stream->workqueue, &stream->work);
}

static void cleanup_buf(struct uvc_streaming *stream, struct uvc_buffer *buf)
{
	int i;

	if (buf->urbs) {
		if (0) pr_info("%s:buf=%p\n", __func__, buf);
		for (i = 0; i < buf->urb_cnt; i++) {
			usb_free_urb(buf->urbs[i]);
		}
		kfree(buf->urbs);
		buf->urbs = NULL;
	}
	if (buf->header_buf) {
		if (0) pr_info("%s:header_buf=%p\n", __func__, buf->header_buf);
		if (buf->hbuf_dma_handle) {
			dma_unmap_single(get_mdev(stream), buf->hbuf_dma_handle,
					buf->header_buf_len, DMA_FROM_DEVICE);
			buf->hbuf_dma_handle = 0;
		}
		dma_free_coherent(
			&stream->dev->udev->dev,
			buf->header_buf_len,
			buf->header_buf, buf->header_phys);
		buf->header_buf = NULL;
	}
	buf->urb_cnt = 0;
}

static int alloc_buf_urbs(struct uvc_streaming *stream, struct uvc_buffer *buf)
{
	unsigned payload = stream->max_payload_size;
	struct urb *urb;
	unsigned n;
	unsigned header_buf_len;
	int i;

	if (!stream->iso_packets)
		payload -= 32 + 256;	/* worst case */
	n = DIV_ROUND_UP(stream->ctrl.dwMaxVideoFrameSize, payload);
	if (0) pr_info("%s: buf=%p(%d), %d %d\n", __func__, buf,
			buf->buf.vb2_buf.index, n, buf->urb_cnt);
	if (!stream->iso_packets)
		n <<= 1;
	if ((buf->urb_cnt == n) || buf->usb_active)
		return 0;

	cleanup_buf(stream, buf);

	buf->urbs = kzalloc(sizeof(void*) * n, stream->gfp_flags);
	if (!buf->urbs) {
		cleanup_buf(stream, buf);
		return -ENOMEM;
	}
	buf->urb_cnt = n;

	for (i = 0; i < n; i++) {
		urb = usb_alloc_urb(stream->iso_packets, stream->gfp_flags);
		if (urb == NULL) {
			cleanup_buf(stream, buf);
			return -ENOMEM;
		}
		buf->urbs[i] = urb;
	}
	if (stream->iso_packets)
		return 0;
//	return 0;

	/* n urbs * psize bytes / urb, half of which are for headers */
	header_buf_len = (n * stream->psize) >> 1;

	buf->header_buf = dma_alloc_coherent(
		&stream->dev->udev->dev,
		header_buf_len, &buf->header_phys,
		stream->gfp_flags | __GFP_NOWARN);

	if (!buf->header_buf) {
		cleanup_buf(stream, buf);
		return -ENOMEM;
	}
	if (hcacheable && (stream->queue.dma_mode == DMA_MODE_CONTIG)) {
		buf->hbuf_dma_handle = dma_map_single(get_mdev(stream),
				buf->header_buf, buf->header_buf_len,
				DMA_FROM_DEVICE);
		if (dma_mapping_error(get_mdev(stream), buf->hbuf_dma_handle)) {
			pr_err("%s:hbuf dma_mapping_error\n", __func__);
			buf->hbuf_dma_handle = 0;
		}
	}
	buf->header_buf_len = header_buf_len;
	stream->queue.sof_index = 0;
	stream->queue.using_headers = 1;
	return 0;
}

static int uvc_buffer_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct uvc_buffer *buf = uvc_vbuf_to_buffer(vbuf);
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);
	void *req_mem;
	unsigned int req_length;

	INIT_WORK(&buf->cache_work, cache_processing_work);
	INIT_WORK(&buf->to_dev_work, uvc_to_dev_work);

	if (0) pr_info("%s:buf=%p(%i) mem=%p\n", __func__, buf, vb->index, buf->mem);
	if (vb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT &&
	    vb2_get_plane_payload(vb, 0) > vb2_plane_size(vb, 0)) {
		uvc_trace(UVC_TRACE_CAPTURE, "[E] Bytes used out of bounds.\n");
		return -EINVAL;
	}

	if (unlikely(queue->return_buffers))
		return -ENODEV;

	buf->error = 0;
	req_mem = vb2_plane_vaddr(vb, 0);
	req_length = vb2_plane_size(vb, 0);
	if ((buf->mem != req_mem) || (buf->length != req_length)) {
		if (buf->mem || buf->length)
			pr_info("%s:buf=%p(%i) mem=%p to %p, len=0x%x to 0x%x\n",
				__func__, buf, vb->index,
				buf->mem, req_mem, buf->length, req_length);
		buf->mem = req_mem;
		buf->length = req_length;
		buf->setup_done = 0;
		if (buf->buf_dma_handle) {
			dma_unmap_single(get_mdev(stream), buf->buf_dma_handle,
					buf->length, DMA_FROM_DEVICE);
			buf->buf_dma_handle = 0;
		}
		if (cacheable && (queue->dma_mode == DMA_MODE_CONTIG)) {
			buf->buf_dma_handle = dma_map_single(get_mdev(stream),
					buf->mem, buf->length, DMA_FROM_DEVICE);
			if (dma_mapping_error(get_mdev(stream), buf->buf_dma_handle)) {
				pr_info("%s:dma_mapping_error\n", __func__);
				buf->buf_dma_handle = 0;
			}
			buf->for_cpu = 0;
		}
	}

	if (vb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		buf->bytesused = 0;
	else
		buf->bytesused = vb2_get_plane_payload(vb, 0);

	if (queue->dma_mode == DMA_MODE_CONTIG)
		return alloc_buf_urbs(stream, buf);
	return 0;
}

static void uvc_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct uvc_buffer *buf = uvc_vbuf_to_buffer(vbuf);

	if (0) pr_info("%s: buf=%p(%d)\n", __func__, buf, vb->index);

	buf->owner = UVC_OWNER_USB;
	buf->cpu_dirty = 1;
	uvc_queue_start_work(queue, buf);
}

static void uvc_buffer_finish(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);
	struct uvc_buffer *buf = uvc_vbuf_to_buffer(vbuf);

	if (vb->state == VB2_BUF_STATE_DONE)
		uvc_video_clock_update(stream, vbuf, buf);
}

static void uvc_buf_cleanup(struct vb2_buffer *vb)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);
	struct uvc_buffer *buf = uvc_vb2_to_buffer(vb);

	cleanup_buf(stream, buf);
	if (buf->buf_dma_handle) {
		dma_unmap_single(get_mdev(stream), buf->buf_dma_handle,
				buf->length, DMA_FROM_DEVICE);
		buf->buf_dma_handle = 0;
	}
}

static void uvc_cancel_buffer(struct uvc_video_queue *queue, struct uvc_buffer *buf)
{
	unsigned j;

	if (buf->usb_active) {
		for (j = 0; j < buf->used_urb_cnt; j++) {
			if (0) pr_info("%s: buf=%p owner=%x,urb=%p(%i)\n", __func__,
					buf, buf->owner, buf->urbs[j], j);
			usb_unlink_urb(buf->urbs[j]);
		}
		pr_info("%s: buffer=%p(%d)\n", __func__, buf, buf->buf.vb2_buf.index);
	}
	if (buf->owner == UVC_OWNER_AVAIL)
		uvc_buffer_done(buf, VB2_BUF_STATE_ERROR, __func__);
}

static void wait_for_urbs(struct uvc_video_queue *queue)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	while (1) {
		if (!queue->submitted)
			break;
		if (time_after(jiffies, timeout))
			break;
		msleep(1);
	}
}

static void stop_queue(struct uvc_video_queue *queue)
{
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);
	int i = 0;
	int retry = 0;

	queue->streaming = 0;
	queue->return_buffers = 1;
	uvc_queue_cancel(queue);
	if (queue->workqueue)
		flush_work(&queue->work);

	while (1) {
		struct vb2_buffer *vb;
		struct uvc_buffer *buf;

		uvc_submit_ready_buffers(queue, GFP_KERNEL);
		if (i >= queue->queue.num_buffers)
			break;
		vb = queue->queue.bufs[i];
		buf = uvc_vb2_to_buffer(vb);
		if (queue->cachequeue)
			flush_work(&buf->cache_work);
		if (stream->workqueue) {
			flush_work(&buf->to_dev_work);
			flush_work(&stream->work);
		}
		if (!retry)
			if (0) pr_info("%s: %p(%d): usb_active=%x owner=%x\n", __func__, buf,
				i, buf->usb_active, buf->owner);
		if (buf->usb_active) {
			msleep(100);
			if (!retry && buf->usb_active)
				pr_info("%s: waiting for buf %d to be returned\n", __func__, i);
			if (retry < 40) {
				retry++;
				if (retry == 30)
					uvc_cancel_buffer(queue, buf);
			} else {
				retry = 0;
				pr_err("%s: complete failed for buf %d\n", __func__, i);
				i++;
			}
		} else {
			retry = 0;
			i++;
		}
	}
	queue->return_buffers = 0;
}

static int uvc_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vq);
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);
	int ret;

	if (0) pr_info("%s\n", __func__);
	queue->buf_used = 0;

	ret = uvc_video_enable(stream, 1);
	if (ret == 0) {
		queue->streaming = 1;
		uvc_queue_start_work(queue, NULL);
		return 0;
	}

	stop_queue(queue);
	return ret;
}

static void uvc_stop_streaming(struct vb2_queue *vq)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vq);
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);

	queue->streaming = 0;
	wait_for_urbs(queue);
	uvc_video_enable(stream, 0);
	pr_debug("%s\n", __func__);
}

static const struct vb2_ops uvc_queue_qops = {
	.queue_setup = uvc_queue_setup,
	.buf_prepare = uvc_buffer_prepare,
	.buf_queue = uvc_buffer_queue,
	.buf_finish = uvc_buffer_finish,
	.buf_cleanup = uvc_buf_cleanup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = uvc_start_streaming,
	.stop_streaming = uvc_stop_streaming,
};

void uvc_queue_initialize(struct uvc_streaming *stream)
{
	struct uvc_video_queue *queue = &stream->queue;

	queue->queue.lock = &queue->mutex;
	mutex_init(&queue->ready_mutex);
	mutex_init(&queue->mutex);
	spin_lock_init(&queue->irqlock);
	INIT_WORK(&queue->work, submit_work);
}

int uvc_queue_init(struct uvc_video_queue *queue, enum v4l2_buf_type type,
		    int drop_corrupted, int dma_mode)
{
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);
	int ret;

	queue->queue.type = type;
	queue->queue.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	queue->queue.drv_priv = queue;
	queue->queue.buf_struct_size = sizeof(struct uvc_buffer);
	queue->queue.ops = &uvc_queue_qops;
	queue->dma_mode = dma_mode;
	queue->queue.mem_ops = (dma_mode == DMA_MODE_CONTIG) ?
		&vb2_dma_contig_memops : &vb2_vmalloc_memops;
	queue->queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC
		| V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
//	queue->queue.dma_attrs = &uvc_dma_attrs;
	queue->queue.dev = &stream->dev->udev->dev;

	ret = vb2_queue_init(&queue->queue);
	if (ret)
		return ret;

	queue->flags = drop_corrupted ? UVC_QUEUE_DROP_CORRUPTED : 0;
	if (dma_mode == DMA_MODE_CONTIG) {
		queue->workqueue = alloc_workqueue("uvc_queue", WQ_UNBOUND | WQ_HIGHPRI, 4);
		if (!queue->workqueue)
			return -ENOMEM;

		queue->cachequeue = alloc_workqueue("uvc_cache_process", WQ_UNBOUND, 4);
		if (!queue->cachequeue)
			return -ENOMEM;
	}
	return 0;
}

void uvc_queue_release(struct uvc_video_queue *queue)
{
	mutex_lock(&queue->mutex);
	stop_queue(queue);
	vb2_queue_release(&queue->queue);
	mutex_unlock(&queue->mutex);
}

void uvc_queue_deinit(struct uvc_video_queue *queue)
{
	if (queue->workqueue) {
		destroy_workqueue(queue->workqueue);
		queue->workqueue = NULL;
	}
	if (queue->cachequeue) {
		destroy_workqueue(queue->cachequeue);
		queue->cachequeue = NULL;
	}
}

/* -----------------------------------------------------------------------------
 * V4L2 queue operations
 */

int uvc_request_buffers(struct uvc_video_queue *queue,
			struct v4l2_requestbuffers *rb)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_reqbufs(&queue->queue, rb);
	mutex_unlock(&queue->mutex);

	return ret ? ret : rb->count;
}

int uvc_query_buffer(struct uvc_video_queue *queue, struct v4l2_buffer *buf)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_querybuf(&queue->queue, buf);
	mutex_unlock(&queue->mutex);

	return ret;
}

int uvc_create_buffers(struct uvc_video_queue *queue,
		       struct v4l2_create_buffers *cb)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_create_bufs(&queue->queue, cb);
	mutex_unlock(&queue->mutex);

	return ret;
}

int uvc_queue_buffer(struct uvc_video_queue *queue, struct v4l2_buffer *buf)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_qbuf(&queue->queue, buf);
	mutex_unlock(&queue->mutex);

	return ret;
}

int uvc_export_buffer(struct uvc_video_queue *queue,
		      struct v4l2_exportbuffer *exp)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_expbuf(&queue->queue, exp);
	mutex_unlock(&queue->mutex);

	return ret;
}

int uvc_dequeue_buffer(struct uvc_video_queue *queue, struct v4l2_buffer *buf,
		       int nonblocking)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_dqbuf(&queue->queue, buf, nonblocking);
	mutex_unlock(&queue->mutex);

	return ret;
}

int uvc_queue_streamon(struct uvc_video_queue *queue, enum v4l2_buf_type type)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_streamon(&queue->queue, type);
	mutex_unlock(&queue->mutex);

	return ret;
}

int uvc_queue_streamoff(struct uvc_video_queue *queue, enum v4l2_buf_type type)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_streamoff(&queue->queue, type);
	mutex_unlock(&queue->mutex);

	return ret;
}

int uvc_queue_mmap(struct uvc_video_queue *queue, struct vm_area_struct *vma)
{
	return vb2_mmap(&queue->queue, vma);
}

#ifndef CONFIG_MMU
unsigned long uvc_queue_get_unmapped_area(struct uvc_video_queue *queue,
		unsigned long pgoff)
{
	return vb2_get_unmapped_area(&queue->queue, 0, 0, pgoff, 0);
}
#endif

unsigned int uvc_queue_poll(struct uvc_video_queue *queue, struct file *file,
			    poll_table *wait)
{
	unsigned int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_poll(&queue->queue, file, wait);
	mutex_unlock(&queue->mutex);

	return ret;
}

/* -----------------------------------------------------------------------------
 *
 */

/*
 * Check if buffers have been allocated.
 */
int uvc_queue_allocated(struct uvc_video_queue *queue)
{
	int allocated;

	mutex_lock(&queue->mutex);
	allocated = vb2_is_busy(&queue->queue);
	mutex_unlock(&queue->mutex);

	return allocated;
}

static void uvc_cancel_buffers(struct uvc_video_queue *queue, unsigned mask)
{
	struct vb2_buffer *vb;
	struct uvc_buffer *buf;
	unsigned i;

	while (mask) {
		i = __ffs(mask);
		mask &= ~(1 << i);
		vb = queue->queue.bufs[i];
		if (!vb)
			continue;
		buf = uvc_vb2_to_buffer(vb);
		uvc_cancel_buffer(queue, buf);
	}
}

/*
 * Cancel the video buffers queue.
 *
 * Cancelling the queue marks all buffers on the irq queue as erroneous,
 * wakes them up and removes them from the queue.
 *
 * If the disconnect parameter is set, further calls to uvc_queue_buffer will
 * fail with -ENODEV.
 *
 * This function acquires the irq spinlock and can be called from interrupt
 * context.
 */
void uvc_queue_cancel(struct uvc_video_queue *queue)
{
	struct uvc_buffer *buf;
	unsigned long flags;
	unsigned available;

	queue->streaming = 0;
	spin_lock_irqsave(&queue->irqlock, flags);
	available = queue->available;
	queue->available = 0;
	spin_unlock_irqrestore(&queue->irqlock, flags);

	uvc_cancel_buffers(queue, available);
	uvc_cancel_buffers(queue, queue->submitted);

	spin_lock_irqsave(&queue->irqlock, flags);
	buf = queue->in_progress;
	queue->in_progress = NULL;
	spin_unlock_irqrestore(&queue->irqlock, flags);
	if (buf && (buf->owner == UVC_OWNER_USB) && buf->mem)
		uvc_buffer_done(buf, VB2_BUF_STATE_ERROR, __func__);
}

void uvc_queue_cancel_sync(struct uvc_video_queue *queue)
{
	stop_queue(queue);
}

void uvc_put_buffer(struct uvc_video_queue *queue)
{
	struct uvc_buffer *buf = queue->in_progress;
#ifndef ALLOW_SHORT_BUFFERS
	struct uvc_streaming *stream = uvc_queue_to_stream(queue);
#endif

	if (!buf)
		return;
	queue->in_progress = NULL;
	if (0) pr_info("%s:bytesused=%x\n", __func__, buf->bytesused);
	if (!buf->mem)
		return;		/* This was fake_buf */
#ifndef ALLOW_SHORT_BUFFERS
	if (stream->ctrl.dwMaxVideoFrameSize != buf->bytesused &&
	    !(stream->cur_format->flags & UVC_FMT_FLAG_COMPRESSED)) {
		buf->error = 1;
		pr_info("%s: Bad frame size %x\n", __func__, buf->bytesused);
	}
#endif
	if (!buf->error || !(queue->flags & UVC_QUEUE_DROP_CORRUPTED)) {
		uvc_buffer_done(buf, VB2_BUF_STATE_DONE, __func__);
		return;
	}
	buf->error = 0;
	buf->bytesused = 0;
	buf->ready = 0;
	buf->ts = 0;
	vb2_set_plane_payload(&buf->buf.vb2_buf, 0, 0);
	uvc_queue_start_work(queue, buf);
}

/* nextbuf is the desired buffer for next */

struct uvc_buffer *uvc_get_buffer(struct uvc_video_queue *queue,
		struct uvc_buffer *nextbuf)
{
	struct uvc_buffer *buf;

	if (!queue->streaming) {
		nextbuf = queue->in_progress;
		if (!nextbuf)
			goto fake_buf;
		if (nextbuf->mem) {
			uvc_put_buffer(queue);
			goto fake_buf;
		}
		return nextbuf;
	}

	buf = queue->in_progress;

	if (buf) {
		if ((buf == nextbuf) || buf->bytesused || !nextbuf)
			return buf;
		queue->in_progress = NULL;
		uvc_queue_start_work(queue, buf);
	}

	if (nextbuf) {
		if ((nextbuf->owner == UVC_OWNER_USB_ACTIVE) ||
				(nextbuf->owner == UVC_OWNER_USB)) {
			queue->in_progress = nextbuf;
			return nextbuf;
		}
	}
	nextbuf = uvc_get_available_buffer(queue, 0);
	if (!nextbuf) {
fake_buf:
		nextbuf = &queue->fake_buf;
		nextbuf->error = 0;
		nextbuf->bytesused = 0;
		nextbuf->ready = 0;
		nextbuf->ts = 0;
		nextbuf->owner = UVC_OWNER_FAKE;
		nextbuf->length = 0x7fffffff;
		if (0) pr_info("%s:next=%p(%i) mem=%p avail=%x submitted=%x\n", __func__,
			nextbuf, nextbuf->buf.vb2_buf.index, nextbuf->mem,
			queue->available, queue->submitted);
	}
	queue->in_progress = nextbuf;
	if (0) pr_info("%s:next=%p(%i) mem=%p avail=%x submitted=%x\n", __func__,
		nextbuf, nextbuf->buf.vb2_buf.index, nextbuf->mem,
		queue->available, queue->submitted);
	return nextbuf;
}
