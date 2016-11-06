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
#include <media/videobuf2-vmalloc.h>

#include "uvcvideo.h"

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

/* -----------------------------------------------------------------------------
 * videobuf2 queue operations
 */

static int uvc_queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
			   unsigned int *nbuffers, unsigned int *nplanes,
			   unsigned int sizes[], void *alloc_ctxs[])
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vq);
	struct uvc_streaming *stream =
			container_of(queue, struct uvc_streaming, queue);
	unsigned sz;
	unsigned npackets;
	unsigned psize;

	if (*nbuffers > UVC_MAX_VIDEO_BUFFERS)
		*nbuffers = UVC_MAX_VIDEO_BUFFERS;

//	if (*nbuffers < 6)
//		*nbuffers = 6;

	*nplanes = 1;
	psize = stream->psize;
	sz = stream->ctrl.dwMaxVideoFrameSize;
	/*
	 * let the last segment transfer an entire payload
	 * in case things are not yet synced
	 */
	if ((queue->dma_mode == DMA_MODE_CONTIG) && !stream->iso_packets)
		sz += stream->max_payload_size;
	if (0) pr_info("%s:psize = %x sz=%x\n", __func__, psize, sz);
	npackets = DIV_ROUND_UP(sz, psize);
	sz = npackets * psize;
	sizes[0] = sz;
	queue->fake_buf.length = sz;

	if (sz > stream->max_payload_size)
		sz = stream->max_payload_size;
	stream->npackets = sz / stream->psize;
	if (stream->iso_packets)
		stream->iso_packets = stream->npackets;
	if (0) pr_info("%s:psize = %x sz=%x npackets=%x\n", __func__, psize, sz, stream->npackets);

	if ((queue->dma_mode == DMA_MODE_CONTIG) && !alloc_ctxs[0]) {
		dma_set_mask_and_coherent(&stream->dev->udev->dev, DMA_BIT_MASK(32));
		alloc_ctxs[0] = vb2_dma_contig_init_ctx(&stream->dev->udev->dev);
		if (0) pr_info("%s: %p %p\n", __func__, alloc_ctxs[0], vq->alloc_ctx[0]);
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
		if (0) pr_info("%s: buf=%p mem %p, length %x to setup_done=%x\n", __func__,
			buf, buf->mem, buf->length, buf->setup_done);
		buf->setup_done = 0;
	}

	if (buf->setup_done)
		return 0;

	if (0) pr_info("%s:b\n", __func__);
	mem = buf->mem;
	phys = vb2_dma_contig_plane_dma_addr(&buf->buf, 0);
	rem = buf->length;

	h_phys = buf->header_phys;
	h_rem = buf->header_buf_len;

	buf->repeat_payload = repeat_payload;
	buf->header_sz = header_sz;

	if (!header_sz) {
		while (i < buf->urb_cnt) {
			if (rem < repeat_payload)
				break;
			stream->init_urb(stream, buf->urbs[i], &buf->buf, mem, repeat_payload, phys, 0);
			if (0) pr_info("%s: init_urb done, urb(%p)=%p len=0x%x\n",
				__func__, &buf->urbs[i], buf->urbs[i], buf->length);
			mem += repeat_payload;
			phys += repeat_payload;
			rem -= repeat_payload;
			i++;
		}
		if (!h_mem)
			goto exit1;

		while (i < buf->urb_cnt) {
			if (h_rem < repeat_payload)
				break;
			stream->init_urb(stream, buf->urbs[i], &buf->buf, h_mem,
				repeat_payload, h_phys, URB_NO_TRANSFER_DMA_MAP);
			if (0) pr_info("%s: init_urb done, urb(%p)=%p len=0x%x\n",
				__func__, &buf->urbs[i], buf->urbs[i], buf->length);
			h_mem += repeat_payload;
			h_phys += repeat_payload;
			h_rem -= repeat_payload;
			i++;
		}
		goto exit1;
	}

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
			stream->init_urb(stream, buf->urbs[i++], &buf->buf, h_mem,
				psize, h_phys, URB_NO_TRANSFER_DMA_MAP);
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
			buf->last_skip_header = 0;
		} else {
			if (rem < mps) {
				pr_err("%s: buf too small\n", __func__);
				break;
			}
			buf->last_skip_header = 1;
		}

		stream->init_urb(stream, buf->urbs[i++], &buf->buf, mem,
				mps, phys, 0);
		if (0) pr_info("%s:(%d) mem=%p len=%x\n", __func__, i-1, mem, mps);
		if (frame_rem <= rp)
			break;
		mem += rp;
		phys += rp;
		rem -= rp;
		frame_rem -= rp;
	}
exit1:
	buf->used_urb_cnt = i;
	buf->setup_done = 1;
	if (0) pr_info("%s: used %u of %u\n", __func__, i, buf->urb_cnt);
	return 0;
}

void uvc_buffer_done(struct uvc_buffer *buf, int state, const char *s)
{
	if (0) pr_info("%s:%s: %p mem=%p from %x to %x, bytesused=%x\n",
		__func__, s, buf, buf->mem,
		buf->owner, UVC_OWNER_QUEUE, buf->bytesused);
	vb2_set_plane_payload(&buf->buf, 0, buf->bytesused);
	buf->owner = UVC_OWNER_QUEUE;
	buf->error = 0;
	buf->bytesused = 0;
	buf->ready = 0;
	buf->ts = 0;
	vb2_buffer_done(&buf->buf, state);
}

static int add_to_available(struct uvc_video_queue *queue, struct uvc_buffer *buf)
{
	unsigned long flags;
	int ret = 0;

	if (queue->return_buffers) {
		/* If the device is disconnected return the buffer to userspace
		 * directly. The next QBUF call will fail with -ENODEV.
		 */
		uvc_buffer_done(buf, VB2_BUF_STATE_ERROR, __func__);
		return 0;
	}

	spin_lock_irqsave(&queue->irqlock, flags);
	if (buf->usb_active) {
		buf->owner = UVC_OWNER_USB_ACTIVE;
	} else {
		queue->available |= (1 << buf->buf.v4l2_buf.index);
		buf->owner = UVC_OWNER_AVAIL;
		ret = 1;
	}
	spin_unlock_irqrestore(&queue->irqlock, flags);
	return ret;
}

struct uvc_buffer *uvc_get_available_buffer(struct uvc_video_queue *queue, int even_last)
{
	unsigned long flags;
	struct uvc_buffer *buf = NULL;

	spin_lock_irqsave(&queue->irqlock, flags);
	if (queue->available) {
		struct vb2_buffer *vb;
		int buf_index = __ffs(queue->available);
		unsigned submitted = queue->submitted;
		/*
		 * return buffer if <=1 buffers submitted
		 * or >1 buffers available
		 * or really needed
		 */
		if ((submitted == (submitted & -submitted)) ||
				(queue->available != (1 << buf_index)) ||
				even_last) {
			queue->available &= ~(1 << buf_index);
			vb = queue->queue.bufs[buf_index];
			buf = container_of(vb, struct uvc_buffer, buf);
			buf->owner = UVC_OWNER_USB;
			buf->error = 0;
			buf->bytesused = 0;
			buf->ready = 0;
			buf->ts = 0;
		}
	}
	spin_unlock_irqrestore(&queue->irqlock, flags);
	if (0) pr_info("%s: %p, available=%x even_last=%x\n", __func__, buf,
			queue->available, even_last);
	return buf;
}

static void submit_buffers(struct uvc_video_queue *queue)
{
	struct uvc_streaming *stream = container_of(queue,
					struct uvc_streaming, queue);
	struct uvc_buffer *buf;
	unsigned long flags;
	int i = 0;

	if (!queue->streaming)
		return;
	if (!stream->sync && stream->header_sz && stream->repeat_payload &&
			queue->using_headers) {
		int sync_index;

		if (queue->submitted & queue->frame_sync_mask)
			goto next;

		queue->frame_sync_mask = 0;

		/*
		 * Now determine how many payloads are left to fill latest buffer
		 */
		sync_index = queue->sync_index;
		if (!sync_index || (sync_index & 1))
			goto next;

		buf = uvc_get_available_buffer(queue, 0);
		if (!buf)
			return;
		setup_buf(stream, buf);
		queue->sync_index = 0;

		sync_index -= buf->last_skip_header;
		if (buf->used_urb_cnt > sync_index) {
			i = buf->used_urb_cnt - sync_index;
			queue->frame_sync_mask = 1 << buf->buf.v4l2_buf.index;
			if (0) pr_info("%s: syncing %i\n", __func__, i);
		}
	} else {
		goto next;
	}

	/* Submit the URBs. */
	while (1) {
		int first = i;

		spin_lock_irqsave(&queue->irqlock, flags);
		queue->submitted |= (1 << buf->buf.v4l2_buf.index);
		buf->owner = UVC_OWNER_USB_ACTIVE;
		buf->last_completed_urb = NULL;
		buf->usb_active = 1;
		buf->urb_index_of_frame = i;
		spin_unlock_irqrestore(&queue->irqlock, flags);
		if (0) pr_info("%s: start %d total %d\n", __func__, i, buf->used_urb_cnt);

		while (i < buf->used_urb_cnt) {
			int ret;

			if (queue->streaming)
				ret = usb_submit_urb(buf->urbs[i], GFP_KERNEL);
			else
				ret = -EINVAL;
			if (ret < 0) {
				if (queue->streaming)
					pr_err("%s: Failed to submit URB "
						"%u-%u (%d).\n",
						__func__, first, i, ret);
				if (i == first) {
					spin_lock_irqsave(&queue->irqlock, flags);
					queue->submitted &= ~(1 << buf->buf.v4l2_buf.index);
					buf->usb_active = 0;
					spin_unlock_irqrestore(&queue->irqlock, flags);

					uvc_buffer_done(buf, VB2_BUF_STATE_ERROR, __func__);
				} else {
					spin_lock_irqsave(&queue->irqlock, flags);
					if (buf->last_completed_urb == buf->urbs[i - 1]) {
						queue->submitted &= ~(1 << buf->buf.v4l2_buf.index);
						buf->usb_active = 0;
						spin_unlock_irqrestore(&queue->irqlock, flags);

						uvc_buffer_done(buf, VB2_BUF_STATE_ERROR, __func__);
					} else {
						buf->setup_done = 0;
						buf->used_urb_cnt = i;
						spin_unlock_irqrestore(&queue->irqlock, flags);
					}
				}
				break;
			}
			if (0) pr_info("%s:3 %u of %u\n", __func__, i, buf->used_urb_cnt);
			i++;
		}
		if (0) pr_info("%s: buf=%p(%i) urbs %u-%u submitted=%x avail=%x\n",
				__func__, buf, buf->buf.v4l2_buf.index,
				first, i, queue->submitted, queue->available);

next:
		if (!queue->streaming)
			break;
		buf = uvc_get_available_buffer(queue, 0);
		if (!buf)
			break;
		setup_buf(stream, buf);
		i = 0;
	}
	if (0) pr_info("%s:e submitted=%x avail=%x\n", __func__, queue->submitted, queue->available);
}

static void submit_work(struct work_struct *work)
{
	struct uvc_video_queue *queue = container_of(work, struct uvc_video_queue, work);

	if (queue->streaming)
		submit_buffers(queue);
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
		usb_free_coherent(stream->dev->udev,
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
			buf->buf.v4l2_buf.index, n, buf->urb_cnt);
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
	buf->header_buf = usb_alloc_coherent(
		stream->dev->udev, header_buf_len,
		stream->gfp_flags | __GFP_NOWARN, &buf->header_phys);

	if (!buf->header_buf) {
		cleanup_buf(stream, buf);
		return -ENOMEM;
	}
	buf->header_buf_len = header_buf_len;
	stream->queue.using_headers = 1;
	return 0;
}

static int uvc_buffer_prepare(struct vb2_buffer *vb)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);
	struct uvc_streaming *stream = container_of(queue, struct uvc_streaming, queue);
	void *req_mem;
	unsigned int req_length;

	if (0) pr_info("%s:buf=%p(%i) mem=%p\n", __func__, buf, buf->buf.v4l2_buf.index, buf->mem);
	if (vb->v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT &&
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
		buf->mem = req_mem;
		buf->length = req_length;
		buf->setup_done = 0;
	}

	if (vb->v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		buf->bytesused = 0;
	else
		buf->bytesused = vb2_get_plane_payload(vb, 0);

	if (queue->dma_mode == DMA_MODE_CONTIG)
		return alloc_buf_urbs(stream, buf);
	return 0;
}

static void uvc_buffer_queue(struct vb2_buffer *vb)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);

	if (0) pr_info("%s: buf=%p(%d)\n", __func__, buf, buf->buf.v4l2_buf.index);

	if (buf->urbs) {
		if (queue->streaming) {
			if (add_to_available(queue, buf))
				queue_work(queue->workqueue, &queue->work);
		} else {
			add_to_available(queue, buf);
		}
	} else {
		add_to_available(queue, buf);
	}
}

void uvc_queue_start_work(struct uvc_video_queue *queue, struct uvc_buffer *buf)
{
	if (buf)
		add_to_available(queue, buf);

	if (queue->workqueue && queue->streaming)
		queue_work(queue->workqueue, &queue->work);
}

static int uvc_buffer_finish(struct vb2_buffer *vb)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct uvc_streaming *stream =
			container_of(queue, struct uvc_streaming, queue);
	struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);

	uvc_video_clock_update(stream, &vb->v4l2_buf, buf);
	return 0;
}

static void uvc_buf_cleanup(struct vb2_buffer *vb)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vb->vb2_queue);
	struct uvc_streaming *stream =
			container_of(queue, struct uvc_streaming, queue);
	struct uvc_buffer *buf = container_of(vb, struct uvc_buffer, buf);

	cleanup_buf(stream, buf);
}

static void uvc_wait_prepare(struct vb2_queue *vq)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vq);

	mutex_unlock(&queue->mutex);
}

static void uvc_wait_finish(struct vb2_queue *vq)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vq);

	mutex_lock(&queue->mutex);
}

static void stop_queue(struct uvc_video_queue *queue)
{
	int i = 0;
	int retry = 0;

	queue->return_buffers = 1;
	uvc_queue_cancel(queue);
	while (1) {
		struct vb2_buffer *vb;
		struct uvc_buffer *buf;

		if (i >= queue->queue.num_buffers)
			break;
		vb = queue->queue.bufs[i];
		buf = container_of(vb, struct uvc_buffer, buf);
		if (!retry)
			if (0) pr_info("%s: %p(%d): usb_active=%x owner=%x\n", __func__, buf,
				i, buf->usb_active, buf->owner);
		if (buf->usb_active) {
			if (!retry)
				pr_info("%s: waiting for buf %d to be returned\n", __func__, i);
			msleep(100);
			if (retry < 40) {
				retry++;
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
	struct uvc_buffer *buf;

	if (0) pr_info("%s\n", __func__);
	queue->streaming = 1;
	buf = uvc_get_available_buffer(queue, 0);
	if (buf)
		uvc_buffer_queue(&buf->buf);
	return 0;
}

static int uvc_stop_streaming(struct vb2_queue *vq)
{
	struct uvc_video_queue *queue = vb2_get_drv_priv(vq);

	if (0) pr_info("%s\n", __func__);
	queue->streaming = 0;
	stop_queue(queue);
	return 0;
}

static struct vb2_ops uvc_queue_qops = {
	.queue_setup = uvc_queue_setup,
	.buf_prepare = uvc_buffer_prepare,
	.buf_queue = uvc_buffer_queue,
	.buf_finish = uvc_buffer_finish,
	.buf_cleanup = uvc_buf_cleanup,
	.wait_prepare = uvc_wait_prepare,
	.wait_finish = uvc_wait_finish,
	.start_streaming = uvc_start_streaming,
	.stop_streaming = uvc_stop_streaming,
};

int uvc_queue_init(struct uvc_video_queue *queue, enum v4l2_buf_type type,
		    int drop_corrupted, int dma_mode)
{
	int ret;

	queue->queue.type = type;
	queue->queue.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	queue->queue.drv_priv = queue;
	queue->queue.buf_struct_size = sizeof(struct uvc_buffer);
	queue->queue.ops = &uvc_queue_qops;
	queue->queue.mem_ops = (dma_mode == DMA_MODE_CONTIG) ?
		&vb2_dma_contig_memops : &vb2_vmalloc_memops;
	queue->dma_mode = dma_mode;

	queue->queue.timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	ret = vb2_queue_init(&queue->queue);
	if (ret)
		return ret;

	mutex_init(&queue->mutex);
	spin_lock_init(&queue->irqlock);

	queue->flags = drop_corrupted ? UVC_QUEUE_DROP_CORRUPTED : 0;
	if (dma_mode == DMA_MODE_CONTIG) {
		queue->workqueue = create_singlethread_workqueue("uvc_queue");
		INIT_WORK(&queue->work, submit_work);
		if (!queue->workqueue)
			return -ENOMEM;
	}
	return 0;
}

void uvc_queue_deinit(struct uvc_video_queue *queue)
{
	if (queue->workqueue) {
		destroy_workqueue(queue->workqueue);
		queue->workqueue = NULL;
	}
}
/* -----------------------------------------------------------------------------
 * V4L2 queue operations
 */

int uvc_alloc_buffers(struct uvc_video_queue *queue,
		      struct v4l2_requestbuffers *rb)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_reqbufs(&queue->queue, rb);
	mutex_unlock(&queue->mutex);

	return ret ? ret : rb->count;
}

void uvc_free_buffers(struct uvc_video_queue *queue)
{
	mutex_lock(&queue->mutex);
	stop_queue(queue);
	vb2_queue_release(&queue->queue);
	mutex_unlock(&queue->mutex);
	if (queue->queue.alloc_ctx[0]) {
		vb2_dma_contig_cleanup_ctx(queue->queue.alloc_ctx[0]);
		queue->queue.alloc_ctx[0] = NULL;
	}
}

int uvc_query_buffer(struct uvc_video_queue *queue, struct v4l2_buffer *buf)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_querybuf(&queue->queue, buf);
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

int uvc_dequeue_buffer(struct uvc_video_queue *queue, struct v4l2_buffer *buf,
		       int nonblocking)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_dqbuf(&queue->queue, buf, nonblocking);
	mutex_unlock(&queue->mutex);

	return ret;
}

int uvc_queue_mmap(struct uvc_video_queue *queue, struct vm_area_struct *vma)
{
	int ret;

	mutex_lock(&queue->mutex);
	ret = vb2_mmap(&queue->queue, vma);
	mutex_unlock(&queue->mutex);

	return ret;
}

#ifndef CONFIG_MMU
unsigned long uvc_queue_get_unmapped_area(struct uvc_video_queue *queue,
		unsigned long pgoff)
{
	unsigned long ret;

	mutex_lock(&queue->mutex);
	ret = vb2_get_unmapped_area(&queue->queue, 0, 0, pgoff, 0);
	mutex_unlock(&queue->mutex);
	return ret;
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

/*
 * Enable or disable the video buffers queue.
 *
 * The queue must be enabled before starting video acquisition and must be
 * disabled after stopping it. This ensures that the video buffers queue
 * state can be properly initialized before buffers are accessed from the
 * interrupt handler.
 *
 * Enabling the video queue returns -EBUSY if the queue is already enabled.
 *
 * Disabling the video queue cancels the queue and removes all buffers from
 * the main queue.
 *
 * This function can't be called from interrupt context. Use
 * uvc_queue_cancel() instead.
 */
int uvc_queue_enable(struct uvc_video_queue *queue, int enable)
{
	int ret;

	mutex_lock(&queue->mutex);
	if (enable) {
		ret = vb2_streamon(&queue->queue, queue->queue.type);
		if (ret < 0)
			goto done;

		queue->buf_used = 0;
	} else {
		queue->streaming = 0;
		ret = vb2_streamoff(&queue->queue, queue->queue.type);
		if (ret < 0)
			goto done;

	}

done:
	mutex_unlock(&queue->mutex);
	return ret;
}

static void uvc_cancel_buffers(struct uvc_video_queue *queue, unsigned mask)
{
	struct vb2_buffer *vb;
	struct uvc_buffer *buf;
	unsigned i, j;

	while (mask) {
		i = __ffs(mask);
		mask &= ~(1 << i);
		vb = queue->queue.bufs[i];
		if (!vb)
			continue;
		buf = container_of(vb, struct uvc_buffer, buf);

		if (buf->usb_active) {
			for (j = 0; j < buf->used_urb_cnt; j++) {
				if (0) pr_info("%s: buf=%p(%i) owner=%x,urb=%p(%i)\n", __func__,
						buf, i, buf->owner, buf->urbs[j], j);
				usb_unlink_urb(buf->urbs[j]);
			}
		}
		if (buf->owner == UVC_OWNER_AVAIL)
			uvc_buffer_done(buf, VB2_BUF_STATE_ERROR, __func__);
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
	unsigned submitted;
	unsigned available;

	queue->streaming = 0;
	spin_lock_irqsave(&queue->irqlock, flags);
	submitted = queue->submitted;
	available = queue->available;
	queue->available = 0;
	spin_unlock_irqrestore(&queue->irqlock, flags);

	uvc_cancel_buffers(queue, submitted);
	uvc_cancel_buffers(queue, available);

	spin_lock_irqsave(&queue->irqlock, flags);
	buf = queue->in_progress;
	queue->in_progress = NULL;
	spin_unlock_irqrestore(&queue->irqlock, flags);
	if (buf && (buf->owner == UVC_OWNER_USB))
		uvc_buffer_done(buf, VB2_BUF_STATE_ERROR, __func__);
}

void uvc_queue_cancel_sync(struct uvc_video_queue *queue)
{
	queue->streaming = 0;
	if (queue->workqueue)
		cancel_work_sync(&queue->work);
	uvc_queue_cancel(queue);
}

void uvc_put_buffer(struct uvc_video_queue *queue)
{
	struct uvc_buffer *buf = queue->in_progress;

	if (!buf)
		return;
	queue->in_progress = NULL;
	if (0) pr_info("%s:bytesused=%x\n", __func__, buf->bytesused);
	if (!buf->mem)
		return;		/* This was fake_buf */
	if ((queue->flags & UVC_QUEUE_DROP_CORRUPTED) && buf->error) {
		buf->error = 0;
		buf->bytesused = 0;
		buf->ready = 0;
		buf->ts = 0;
		vb2_set_plane_payload(&buf->buf, 0, 0);
		if (add_to_available(queue, buf))
			if (queue->workqueue && queue->streaming)
				queue_work(queue->workqueue, &queue->work);
	} else {
		uvc_buffer_done(buf, VB2_BUF_STATE_DONE, __func__);
	}
}

/* nextbuf is the desired buffer for next */

struct uvc_buffer *uvc_get_buffer(struct uvc_video_queue *queue,
		struct uvc_buffer *nextbuf)
{
	if (!queue->streaming) {
		uvc_put_buffer(queue);
		return NULL;
	}
	if (queue->in_progress)
		return queue->in_progress;

	if (nextbuf) {
		if (nextbuf->owner == UVC_OWNER_USB_ACTIVE) {
			queue->in_progress = nextbuf;
			if (!(queue->submitted & queue->frame_sync_mask))
				queue->sync_index = queue->urb_index_of_frame;
			return nextbuf;
		}
	}
	nextbuf = uvc_get_available_buffer(queue, 1);
	if (!nextbuf) {
		nextbuf = &queue->fake_buf;
		nextbuf->error = 0;
		nextbuf->bytesused = 0;
		nextbuf->ready = 0;
		nextbuf->ts = 0;
		if (0) pr_info("%s:next=%p(%i) mem=%p avail=%x submitted=%x\n", __func__,
			nextbuf, nextbuf->buf.v4l2_buf.index, nextbuf->mem,
			queue->available, queue->submitted);
	}
	queue->in_progress = nextbuf;
	if (0) pr_info("%s:next=%p(%i) mem=%p avail=%x submitted=%x\n", __func__,
		nextbuf, nextbuf->buf.v4l2_buf.index, nextbuf->mem,
		queue->available, queue->submitted);
	if (!(queue->submitted & queue->frame_sync_mask))
		queue->sync_index = queue->urb_index_of_frame;
	return nextbuf;
}
