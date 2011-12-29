/*
 * Copyright 2010 Boundary Devices, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/mxcfb.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-int-device.h>
#include <linux/i2c.h>
#include <mach/hardware.h>
#include <linux/ipu.h>
#include <linux/fsl_devices.h>
#include "ov5640.h"

#define MODULE_NAME "bdcam"
#define V4L2_BUF_FLAG_STATE (V4L2_BUF_FLAG_MAPPED|V4L2_BUF_FLAG_QUEUED|V4L2_BUF_FLAG_DONE)

extern unsigned long get_camera_phys(unsigned maxsize);

struct camera_frame_t {
	struct list_head queue ;
	u32 paddress;
	void *vaddress;
	struct v4l2_buffer buffer;
};

static LIST_HEAD(devlist);
static unsigned long camera_phys ;
static void *camera_virt ;

#define DEBUGMSG(arg...)

struct camera_data_t {
        struct v4l2_device	v4l2_dev;
	struct list_head        devlist;
	struct video_device     *vfd;
	struct v4l2_subdev	*subdev ;
        struct mxc_camera_platform_data const *platform_data;
        spinlock_t 		lock ;
	int			use_count ;
	int			streaming ;
	int			num_frames ;
	struct camera_frame_t  *buf_queued[2];
	struct list_head 	capture_queue;
	struct list_head 	done_queue;
        wait_queue_head_t 	wait ;
	struct camera_frame_t  *frames ;
	unsigned		framesize ;
};

/*!
 * Free frame buffers status
 *
 * @param cam    Structure cam_data *
 *
 * @return none
 */
static void free_frames(struct camera_data_t *data)
{
	int i;

	pr_err("%s: %d frames\n", __func__, data ? data->num_frames : -1 );

	BUG_ON(data->streaming);
	if (0 == data->frames)
		return ;
	BUG_ON(0 == camera_virt);
	BUG_ON(0 == camera_phys);

	for (i = 0 ; i < data->num_frames ; i++) {
		struct camera_frame_t *f = data->frames+i ;
		if ((f->vaddress != 0) && (f->paddress != 0)){
			f->vaddress = 0;
		} else {
			DEBUGMSG ("%s: frame %d has no vaddr\n", __func__, i );
		}
	}
	data->num_frames = 0 ;
	kfree(data->frames);
	data->frames = 0 ;
	data->framesize = 0 ;
	iounmap(camera_virt);
	camera_virt = 0 ;
	camera_phys = 0 ;
}

static int allocate_frames(struct camera_data_t *data, unsigned count, unsigned size)
{
	int i;
	unsigned frame_size = PAGE_ALIGN(size);
	unsigned total_size = count*frame_size ;
	unsigned next_phys ;
	void 	*next_virt ;

	pr_err("%s: %u frames of %u bytes each\n", __func__, count, size );

	BUG_ON(data->streaming);
	BUG_ON(data->frames);
	BUG_ON(camera_virt);
	BUG_ON(camera_phys);
	camera_phys = get_camera_phys(total_size);
	if (0 == camera_phys)
		return -ENOBUFS ;
	camera_virt = ioremap_wc(camera_phys,total_size);
	if (0 == camera_virt) {
		camera_phys = 0 ;
		return -ENOBUFS ;
	}

	next_phys = camera_phys ;
	next_virt = camera_virt ;

	data->frames = kmalloc(count*sizeof(data->frames[0]),GFP_KERNEL);
	if (0 == data->frames)
		return -ENOBUFS ;
	memset(data->frames,0,count*sizeof(data->frames[0]));
	data->num_frames = count ;
	data->framesize = size = PAGE_ALIGN(size);
	for (i = 0; i < count; i++) {
		data->frames[i].vaddress = next_virt ;
		data->frames[i].paddress = next_phys ;
		next_virt = (char *)next_virt + frame_size ;
		next_phys += frame_size ;
		data->frames[i].buffer.index = i ;
		data->frames[i].buffer.flags = V4L2_BUF_FLAG_MAPPED;
		data->frames[i].buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		data->frames[i].buffer.length = size ;
		data->frames[i].buffer.memory = V4L2_MEMORY_MMAP;
		data->frames[i].buffer.m.offset = data->frames[i].paddress;
		INIT_LIST_HEAD(&data->frames[i].queue);
		pr_err("%u: %08x: virt %p\n",i,data->frames[i].paddress,data->frames[i].vaddress);
	}

	return 0;
}

static int boundary_camera_open(struct file *file)
{
        int rv = 0 ;
	struct video_device *dev = video_devdata(file);
	struct camera_data_t *cam = video_get_drvdata(dev);
	if( 1 == ++cam->use_count ) {
		rv = v4l2_subdev_call(cam->subdev, core, load_fw);
		if (0 != rv)
			printk (KERN_ERR "%s: error %d loading firmware\n", __func__, rv );
		else
			printk (KERN_ERR "%s: firmware loaded\n", __func__ );
	}
	return 0 ;
}

#define IDMAC_NFACK_0	(2*32+0)		// IDMAC_NFACK_0

static void feed_frames(struct camera_data_t *cam)
{
	int i ;
	unsigned long flags ;

	if (list_empty(&cam->capture_queue))
		return ;

	spin_lock_irqsave(&cam->lock,flags);

	for (i = 0; i < ARRAY_SIZE(cam->buf_queued); i++) {
		if (!cam->buf_queued[i]) {
			if (!list_empty(&cam->capture_queue)) {
				int err ;
				struct camera_frame_t *frame = list_entry(cam->capture_queue.next, struct camera_frame_t, queue);
				if (!frame) {
					DEBUGMSG ("%s: no more buffers %u\n", __func__, i );
					break;
				}
				DEBUGMSG ("%s: queue buffer %u\n", __func__, i );
				list_del(cam->capture_queue.next);
				err = ipu_update_channel_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, i, frame->buffer.m.offset);
				if (err != 0)
					printk (KERN_ERR "%s: err %d buffer %d/0x%x\n", __func__, err, i,frame->buffer.m.offset);
				ipu_select_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, i);
				cam->buf_queued[i] = frame ;
			}
			else
				break;
		} else {
			DEBUGMSG ("%s: buffers %u already queued\n", __func__, i );
		}
	}
	spin_unlock_irqrestore(&cam->lock,flags);
}

static void deliver_frame(struct camera_data_t *cam,unsigned buf)
{
	int was_ready = ipu_check_buffer_ready(CSI_MEM,IPU_OUTPUT_BUFFER,buf);
	struct camera_frame_t *frame = cam->buf_queued[buf];
	BUG_ON(buf >= ARRAY_SIZE(cam->buf_queued));
	BUG_ON(!cam->buf_queued[buf]);
	cam->buf_queued[buf] = 0 ;
	frame->buffer.flags |= V4L2_BUF_FLAG_DONE ;
	frame->buffer.flags &= ~V4L2_BUF_FLAG_QUEUED ;
        do_gettimeofday(&frame->buffer.timestamp);
	list_add_tail(&frame->queue,&cam->done_queue);
	wake_up_all(&cam->wait);
	DEBUGMSG ("%s: woke up all\n", __func__ );
	if (was_ready) {
		printk (KERN_ERR "%s: returning buffer marked as ready\n", __func__ );
	}
}

/*!
 * Camera frame done callback.
 *
 *	We keep track of what we've queued (which buffers were flagged as ready)
 *	and compare against what's currently flagged as ready.
 *
 *	We return a frame IFF:
 *		one frame appears to be returned (ready cleared)
 *		and one frame is still queued
 *
 */
static irqreturn_t eof_callback(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

/*!
 * Camera frame start callback.
 *
 * This seems to be a more reliable place to return frames than the eof
 * callback.
 *
 * Return a frame IFF:
 *
 *	the frame we're not starting is flagged as !ready (meaning that the
 *	IPU filled it)
 *
 *	we have another item in the queue.
 *
 */
static irqreturn_t sof_callback(int irq, void *dev_id)
{
        struct camera_data_t *cam = (struct camera_data_t *)dev_id ;
	int curbuf = ipu_get_cur_buffer_idx(CSI_MEM, IPU_OUTPUT_BUFFER);
	int prevbuf = curbuf^1 ;
	unsigned curmask = 1<<curbuf ;
	unsigned prevmask = curmask^3 ;
        int bufsready = ipu_buffers_ready(CSI_MEM, IPU_OUTPUT_BUFFER);
	if ((0 == (bufsready & prevmask))
	    &&
	    !list_empty(&cam->capture_queue)){
		int err ;
		unsigned long flags ;
		struct camera_frame_t *frame ;
		spin_lock_irqsave(&cam->lock,flags);
		deliver_frame(cam,prevbuf);
		frame = list_entry(cam->capture_queue.next, struct camera_frame_t, queue);
		list_del(cam->capture_queue.next);
		err = ipu_update_channel_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, prevbuf, frame->buffer.m.offset);
		if (err != 0)
			printk (KERN_ERR "%s: err %d buffer %d/0x%x\n", __func__, err, prevbuf,frame->buffer.m.offset);
		ipu_select_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, prevbuf);
		cam->buf_queued[prevbuf] = frame ;
		spin_unlock_irqrestore(&cam->lock,flags);
	}
	return IRQ_HANDLED;
}

static void stop_streaming(struct camera_data_t *data)
{
	int rv ;
	unsigned csi = 0 ; /* TODO: fix csi */

	DEBUGMSG ("%s\n", __func__ );
	rv = v4l2_subdev_call(data->subdev, video, s_stream, 0);
	ipu_disable_csi(csi);
	ipu_disable_channel(CSI_MEM,0);
	ipu_uninit_channel(CSI_MEM);
        ipu_clear_buffer_ready(CSI_MEM,IPU_OUTPUT_BUFFER, 0);
        ipu_clear_buffer_ready(CSI_MEM,IPU_OUTPUT_BUFFER, 1);
	ipu_update_channel_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, 0, -1);
	ipu_update_channel_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, 1, -1);
	data->buf_queued[0] = data->buf_queued[1] = 0 ;
        ipu_free_irq(IPU_IRQ_CSI0_OUT_EOF, data);
        ipu_free_irq(IDMAC_NFACK_0, data);
	/* should we return these to the app here? */
	INIT_LIST_HEAD(&data->capture_queue);
	INIT_LIST_HEAD(&data->done_queue);

	data->streaming = 0 ;
}

static int boundary_camera_close(struct file *file)
{
	struct video_device *dev = video_devdata(file);
	struct camera_data_t *cam = video_get_drvdata(dev);
	DEBUGMSG ("%s\n", __func__ );
	if( 0 == --cam->use_count ) {
		DEBUGMSG ("%s: last use\n", __func__ );
		if(cam->streaming)
			stop_streaming(cam);
		free_frames(cam);
	}
	return 0 ;
}

static ssize_t boundary_camera_read
	( struct file *file,
	  char *buf,
	  size_t count,
          loff_t *ppos)
{
	DEBUGMSG ("%s\n", __func__ );
	return -ENOTSUPP ;
}

static int data_width_enum(unsigned dataw) {
	if (4 == dataw) {
		return IPU_CSI_DATA_WIDTH_4 ;
	} else if (8 == dataw) {
		return IPU_CSI_DATA_WIDTH_8 ;
	} else if (10 == dataw) {
		return IPU_CSI_DATA_WIDTH_10 ;
	} else if (16 == dataw) {
		return IPU_CSI_DATA_WIDTH_10 ;
	} else
		return -1 ;
}

static int enable_camera
	( struct ov5640_setting *setting,
	  unsigned csi)
{
	int err ;
	ipu_channel_params_t enc;
        dma_addr_t dummy = 0x01 ;
	ipu_csi_signal_cfg_t csi_param ;

	ipu_csi_set_window_size(setting->xres,setting->yres, csi);
	ipu_csi_set_window_pos(setting->hoffs, setting->voffs, csi);

	csi_param.sens_clksrc = 0 ; // never used
	csi_param.clk_mode = setting->clk_mode ;
	csi_param.data_pol = setting->data_pol ;
	csi_param.ext_vsync = setting->ext_vsync ;
	csi_param.pack_tight = setting->pack_tight ;
	csi_param.force_eof = setting->force_eof ;
	csi_param.data_en_pol = setting->data_en_pol ;
	csi_param.mclk = setting->mclk ;
	csi_param.pixclk_pol = setting->pixclk_pol  ;
	csi_param.csi = csi;
	csi_param.data_width = data_width_enum(setting->data_width);
	csi_param.Vsync_pol = setting->Vsync_pol ;
	csi_param.Hsync_pol = setting->Hsync_pol ;
	csi_param.data_fmt = setting->ipuin_fourcc ;

	printk(KERN_ERR "init_csi: %ux%u +%u+%u\n", setting->xres, setting->yres, setting->hmarg, setting->vmarg);
	ipu_csi_init_interface(setting->xres+setting->hmarg,setting->yres+setting->vmarg,setting->ipuin_fourcc,csi_param);

	memset(&enc, 0, sizeof(ipu_channel_params_t));

	enc.csi_mem.csi = csi ;
	enc.csi_mem.mipi_en = 0 ;
	enc.csi_mem.mipi_id = 0 ;

	printk(KERN_ERR "init channel %x for csi %d\n", CSI_MEM, csi);
	err = ipu_init_channel(CSI_MEM, &enc);
	if (err != 0) {
		printk(KERN_ERR "ipu_init_channel %d\n", err);
		return err;
	}

	printk(KERN_ERR "init channel buffer %x, stride %u\n", CSI_MEM, setting->stride );
	printk(KERN_ERR "%s: forcing NV12 output\n", __func__ );
	err = ipu_init_channel_buffer(CSI_MEM, IPU_OUTPUT_BUFFER,
				      setting->ipuout_fourcc,
				      setting->xres,
				      setting->yres,
				      setting->stride,
				      0,
				      dummy,
				      dummy,
				      0,
				      0,
				      0 );
	if (err != 0) {
		printk(KERN_ERR "CSI_MEM output buffer\n");
		return err;
	}
	printk(KERN_ERR "enabling channel %x\n", CSI_MEM);
	err = ipu_enable_channel(CSI_MEM);
	if (err < 0) {
		printk(KERN_ERR "ipu_enable_channel CSI_MEM\n");
	}
	return err;
}

static long boundary_camera_ioctl
	( struct file *file,
          unsigned int ioctlnr,
	  unsigned long arg)
{
	struct video_device *dev = video_devdata(file);
	struct camera_data_t *cam = video_get_drvdata(dev);
	int retval = -EINVAL ;

	DEBUGMSG ("%s:\n", __func__);

	switch (ioctlnr) {
	/*!
	 * V4l2 VIDIOC_QUERYCAP ioctl
	 */
	case VIDIOC_QUERYCAP: {
		struct v4l2_capability *cap = (struct v4l2_capability *)arg;
		pr_err("   case VIDIOC_QUERYCAP\n");
		strcpy(cap->driver, "boundary_camera");
//		strcpy(cap->bus_info, cam->master->name );
//		strcpy(cap->card, cam->slave->name);
		cap->version = KERNEL_VERSION(0, 1, 11);
		cap->capabilities = V4L2_CAP_VIDEO_CAPTURE |
				    V4L2_CAP_STREAMING ;
		retval = 0 ;
		break;
	}

	case VIDIOC_ENUM_FMT: {
                struct v4l2_fmtdesc *fmt = (struct v4l2_fmtdesc *)arg;
		pr_err("   case VIDIOC_ENUM_FMT: %p/%d\n", fmt, fmt ? fmt->index : -1 );
                retval = v4l2_subdev_call(cam->subdev, video, enum_fmt, fmt);
		break;
	}

	case VIDIOC_G_FMT: {
                struct v4l2_format *fmt = (struct v4l2_format *)arg ;
		pr_err("   case VIDIOC_G_FMT: %p/%d\n", fmt, fmt ? fmt->type : -1 );
                retval = v4l2_subdev_call(cam->subdev, video, g_fmt, fmt);
		break;
	}

	case VIDIOC_S_FMT: {
		struct v4l2_format *sf = (struct v4l2_format *)arg;
		pr_err("   case VIDIOC_S_FMT\n");
                retval = v4l2_subdev_call(cam->subdev, video, s_fmt, sf);
		break;
	}

	case VIDIOC_REQBUFS: {
                struct v4l2_format fmt ;
		struct v4l2_requestbuffers *req = (struct v4l2_requestbuffers *)arg;

		pr_err("   case VIDIOC_REQBUFS\n");
		if (cam->streaming) {
			pr_err( "%s: can't allocate buffers while streaming\n", __func__ );
			retval = -EINVAL ;
			break;
		}
		if ((req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) ||
		    (req->memory != V4L2_MEMORY_MMAP)) {
			pr_err("wrong buffer type\n");
			retval = -EINVAL;
			break;
		}

                free_frames(cam);
                retval = v4l2_subdev_call(cam->subdev, video, g_fmt, &fmt);
		if (0 == retval) {
                        retval = allocate_frames(cam,req->count,fmt.fmt.pix.sizeimage);
		} else {
			pr_err("g_fmt_cap: %d\n", retval);
		}

		break;
	}

	case VIDIOC_QUERYBUF: {
		struct v4l2_buffer *buf = (struct v4l2_buffer *)arg ;
		unsigned i = buf->index ;

		DEBUGMSG ("case VIDIOC_QUERYBUF: %p/%d\n", buf, i);
		if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			pr_err("wrong buffer type\n");
			retval = -EINVAL;
			break;
		}

		if (i >= cam->num_frames) {
			pr_err("Invalid index %d\n", i);
			retval = -EINVAL ;
			break;
		}
		memcpy(buf, &(cam->frames[i].buffer), sizeof(*buf));
		DEBUGMSG ("%u: %08x: virt %p\n",i, cam->frames[i].paddress,cam->frames[i].vaddress);
		retval = 0 ;
		break;
	}

	case VIDIOC_QBUF: {
		struct v4l2_buffer *buf = (struct v4l2_buffer *)arg;
		int i = buf->index;
		struct v4l2_buffer *ourbuf = &cam->frames[i].buffer ;

		DEBUGMSG ("case VIDIOC_QBUF: %u -> %p/%08x/%p\n", i, ourbuf,cam->frames[i].paddress,cam->frames[i].vaddress);

		if (buf->m.offset && (ourbuf->m.offset != buf->m.offset)) {
			pr_err( "Invalid offset 0x%x != 0x%x\n", ourbuf->m.offset, buf->m.offset);
			retval = -EINVAL ;
			break;
		}
		if ((ourbuf->flags & V4L2_BUF_FLAG_STATE) == V4L2_BUF_FLAG_MAPPED) {
			unsigned long flags ;
			DEBUGMSG ("locking\n");
			spin_lock_irqsave(&cam->lock,flags);
			ourbuf->flags |= V4L2_BUF_FLAG_QUEUED;
			DEBUGMSG ("adding to queue\n");

			list_add_tail(&cam->frames[i].queue,&cam->capture_queue);
			DEBUGMSG ("done adding to queue\n");

			spin_unlock_irqrestore(&cam->lock,flags);
			retval = 0 ;
		} else if (ourbuf->flags & V4L2_BUF_FLAG_QUEUED) {
			pr_err("buffer already queued\n");
			retval = -EINVAL;
			break;
		} else if (ourbuf->flags & V4L2_BUF_FLAG_DONE) {
			pr_err("overwrite done buffer.\n");
			retval = -EINVAL;
			break;
		} else {
			pr_err("unknown flag combination: %x\n", ourbuf->flags);
		}

		buf->flags = ourbuf->flags;
		break;
	}

	case VIDIOC_DQBUF: {
		DEBUGMSG ("VIDIOC_DQBUF\n");
		if (list_empty(&cam->done_queue) && (file->f_flags & O_NONBLOCK)) {
			retval = -EAGAIN;
                        DEBUGMSG ("not ready\n");
			break;
		}

		DEBUGMSG ("waiting for data\n");
                wait_event_interruptible(cam->wait,!list_empty(&cam->done_queue));
		if (signal_pending(current)) {
			DEBUGMSG ("interrupt received\n");
			retval = -ERESTARTSYS;
		} else if( !list_empty(&cam->done_queue) ){
			unsigned long flags ;
                        struct camera_frame_t *frame ;

			spin_lock_irqsave(&cam->lock,flags);
                        frame = (struct camera_frame_t *)cam->done_queue.next ;
			DEBUGMSG ("reading done frame\n");
			list_del(cam->done_queue.next);
			spin_unlock_irqrestore(&cam->lock,flags);

                        if (frame->buffer.flags & V4L2_BUF_FLAG_DONE) {
				struct v4l2_buffer *buf = (struct v4l2_buffer *)arg;
				frame->buffer.flags &= ~V4L2_BUF_FLAG_DONE;
				buf->bytesused = cam->framesize ;
				buf->index = frame->buffer.index;
				buf->flags = frame->buffer.flags;
				buf->m.offset = frame->buffer.m.offset ;
				buf->timestamp = frame->buffer.timestamp;
				DEBUGMSG ("DQBUF: returning %d/%d/%d/0x%x/%ld.%06ld\n", buf->bytesused,buf->index,buf->flags,buf->m.offset,buf->timestamp.tv_sec,buf->timestamp.tv_usec);
				retval = 0 ;
			} else if (frame->buffer.flags & V4L2_BUF_FLAG_QUEUED) {
				pr_err("Buffer not filled.\n");
				retval = -EINVAL;
			} else if ((frame->buffer.flags & 0x7) == V4L2_BUF_FLAG_MAPPED) {
				pr_err("Buffer not queued.\n");
				retval = -EINVAL;
			}
		} else {
			pr_err( "spurious wake\n");
			retval = -ERESTARTSYS;
		}

		break;
	}

	case VIDIOC_STREAMON: {
		struct ov5640_setting setting;

		printk (KERN_ERR "   case VIDIOC_STREAMON\n");
		if (cam->streaming) {
			pr_err( "already streaming\n" );
			retval = -EINVAL ;
			break;
		}
		printk (KERN_ERR "%s: calling bogus ioctl\n", __func__ );
                retval = v4l2_subdev_call(cam->subdev, core, ioctl, vidioc_int_g_ifparm_num, &setting);
		if (retval) {
			pr_err("vidioc_int_g_ifparm_num");
			break;
		}
		printk (KERN_ERR "%s: sizeof(struct v4l2_if_type_bt656) == %u\n", __func__, sizeof(struct v4l2_if_type_bt656));
		retval = enable_camera(&setting,cam->platform_data->csi);
		if (0 != retval) {
			pr_err("Error %d enabling camera\n", retval );
			break;
		}

		retval = ipu_request_irq(IPU_IRQ_CSI0_OUT_EOF, eof_callback, 0, "camera", cam);
		if (0 != retval) {
			pr_err("Error %d setting IRQ handler\n", retval );
			break;
		}

		retval = ipu_request_irq(IDMAC_NFACK_0, sof_callback, 0, "camera", cam);
		if (0 != retval) {
			pr_err("Error %d setting IRQ handler\n", retval );
			break;
		}

                retval = v4l2_subdev_call(cam->subdev, video, s_stream, 1);
		if (0 != retval) {
			pr_err("Error %d setting power to camera\n", retval );
			break;
		}

		feed_frames(cam);
		cam->streaming = 1 ;
		ipu_enable_csi(cam->platform_data->csi);

		break;
	}

	case VIDIOC_STREAMOFF: {
		DEBUGMSG ("case VIDIOC_STREAMOFF\n");
		if (0 == cam->streaming) {
			pr_err("not streaming\n");
			retval = -EINVAL ;
			break;
		}

		stop_streaming(cam);
		retval = 0 ;
		break;
	}

	case VIDIOC_G_CTRL: {
                struct v4l2_control *a = (struct v4l2_control *)arg ;
		DEBUGMSG ("case VIDIOC_G_CTRL\n");
                retval = v4l2_subdev_call(cam->subdev, core, g_ctrl, a);
		break;
	}

	case VIDIOC_S_CTRL: {
                struct v4l2_control *a = (struct v4l2_control *)arg ;
		DEBUGMSG ("case VIDIOC_S_CTRL\n");
                retval = v4l2_subdev_call(cam->subdev, core, s_ctrl, a);
		break;
	}

	case VIDIOC_CROPCAP: {
		pr_err("   case VIDIOC_CROPCAP\n");
		break;
	}

	case VIDIOC_G_CROP: {
		pr_err("   case VIDIOC_G_CROP\n");
		break;
	}

	case VIDIOC_S_CROP: {
		pr_err("   case VIDIOC_S_CROP\n");
		break;
	}

	case VIDIOC_OVERLAY: {
		pr_err("   VIDIOC_OVERLAY\n");
		break;
	}

	case VIDIOC_G_FBUF: {
		pr_err("   case VIDIOC_G_FBUF\n");
		break;
	}

	case VIDIOC_S_FBUF: {
		pr_err("   case VIDIOC_S_FBUF\n");
		break;
	}

	case VIDIOC_G_PARM: {
		struct v4l2_streamparm *parm = (struct v4l2_streamparm *)arg;
		pr_err("   case VIDIOC_G_PARM\n");
                retval = v4l2_subdev_call(cam->subdev, video, g_parm, parm);
		break;
	}

	case VIDIOC_S_PARM:  {
		struct v4l2_streamparm *parm = (struct v4l2_streamparm *)arg;
		DEBUGMSG ("case VIDIOC_S_PARM: type %d\n", parm->type );
		retval = v4l2_subdev_call(cam->subdev, video, s_parm, parm);
		DEBUGMSG ("%s: retval %d\n", __func__, retval );
		break;
	}

	case VIDIOC_ENUMINPUT: {
                struct v4l2_input *input = (struct v4l2_input *)arg;
		DEBUGMSG ("case VIDIOC_ENUMINPUT: %d\n", input->index);
                if (0 == input->index) {
//			strcpy(input->name, cam->slave->name);
			input->type = V4L2_INPUT_TYPE_CAMERA ;
			input->audioset = 0 ;
			input->tuner = 0 ;
			input->status = V4L2_IN_ST_NO_POWER ;
			input->std = V4L2_STD_UNKNOWN ;
			retval = 0 ;
		}
		break;
	}

	case VIDIOC_G_INPUT: {
		pr_err("   case VIDIOC_G_INPUT\n");
		break;
	}

	case VIDIOC_S_INPUT: {
		pr_err("   case VIDIOC_S_INPUT\n");
		break;
	}

	case VIDIOC_TRY_FMT: {
                struct v4l2_format *a = (struct v4l2_format *)arg ;
		DEBUGMSG ("case VIDIOC_TRY_FMT\n");
                retval = v4l2_subdev_call(cam->subdev, video, try_fmt, a);
		break;
	}
	case VIDIOC_QUERYCTRL: {
                struct v4l2_queryctrl *a = (struct v4l2_queryctrl *)arg;
		DEBUGMSG ("case VIDIOC_QUERYCTRL\n");
                retval = v4l2_subdev_call(cam->subdev, core, queryctrl, a);
		break;
	}
	case VIDIOC_G_TUNER: {
		pr_err("   case VIDIOC_G_TUNER\n");
		break;
	}
	case VIDIOC_S_TUNER: {
		pr_err("   case VIDIOC_S_TUNER\n");
		break;
	}
	case VIDIOC_G_FREQUENCY: {
		pr_err("   case VIDIOC_G_FREQUENCY\n");
		break;
	}
	case VIDIOC_S_FREQUENCY: {
		pr_err("   case VIDIOC_S_FREQUENCY\n");
		break;
	}
	default:
		pr_err("   case default or not supported: %d\n", ioctlnr);
		retval = -EINVAL;
		break;
	}
	return retval;
}

static int boundary_camera_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size;

	size = vma->vm_end - vma->vm_start;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot); /* See ENGR00132971 */

	if (remap_pfn_range(vma, vma->vm_start,
			    vma->vm_pgoff, size, vma->vm_page_prot)) {
		pr_err("remap_pfn_range failed\n");
		return -ENOBUFS;
	}

	vma->vm_flags &= ~VM_IO;	/* using shared anonymous pages */

	return 0 ;
}

static unsigned int boundary_camera_poll
	( struct file *file,
	  struct poll_table_struct *wait )
{
	struct video_device *dev = video_devdata(file);
	struct camera_data_t *cam = video_get_drvdata(dev);
	int res = 0 ;

	if (list_empty(&cam->done_queue)) {
		wait_queue_head_t *queue = NULL;
		poll_wait(file, queue, wait);
		DEBUGMSG ("%s: woke up\n", __func__ );
		if (!list_empty(&cam->done_queue))
			res = POLLIN | POLLRDNORM ;
	} else
		res = POLLIN | POLLRDNORM ;

	return res ;
}

static struct v4l2_file_operations boundary_camera_fops = {
	.owner = THIS_MODULE,
	.open = boundary_camera_open,
	.release = boundary_camera_close,
	.read = boundary_camera_read,
	.ioctl = boundary_camera_ioctl,
	.mmap = boundary_camera_mmap,
	.poll = boundary_camera_poll,
};

static const struct v4l2_ioctl_ops bdcam_ioctl_ops = {
};

static struct video_device bdcam_template = {
	.name		= MODULE_NAME,
	.fops           = &boundary_camera_fops,
	.ioctl_ops 	= &bdcam_ioctl_ops,
	.minor		= -1,
	.release	= video_device_release,
};


static int boundary_camera_driver_probe(struct platform_device *pdev)
{
	int ret ;
	struct camera_data_t *cam ;
        struct video_device *vfd;
        struct i2c_adapter *i2c_adapter ;
        struct mxc_camera_platform_data *plat_data = pdev->dev.platform_data ;
        struct i2c_board_info board_info ;
	printk(KERN_ERR "%s\n", __func__ );
	printk(KERN_ERR "core_regulator == %s\n", plat_data->core_regulator);
	printk(KERN_ERR "io_regulator == %s\n", plat_data->io_regulator );
	printk(KERN_ERR "analog_regulator == %s\n", plat_data->analog_regulator );
	printk(KERN_ERR "gpo_regulator == %s\n", plat_data->gpo_regulator );
	printk(KERN_ERR "mclk == %d\n", plat_data->mclk );
	printk(KERN_ERR "csi == %d\n", plat_data->csi );
	printk(KERN_ERR "power_down == %d\n", plat_data->power_down );
	printk(KERN_ERR "reset == %d\n", plat_data->reset );

	cam = kzalloc(sizeof(*cam),GFP_KERNEL);
	if (cam) {
		int ret ;
		snprintf(cam->v4l2_dev.name, sizeof(cam->v4l2_dev.name),"%s-%03d", MODULE_NAME, plat_data->csi );
		ret = v4l2_device_register(NULL, &cam->v4l2_dev);
		if (ret)
			return ret ;
		INIT_LIST_HEAD(&cam->capture_queue);
		INIT_LIST_HEAD(&cam->done_queue);
                init_waitqueue_head(&cam->wait);
		spin_lock_init(&cam->lock);
		cam->platform_data = plat_data ;
	} else {
		printk (KERN_ERR "%s: Error allocating camera %d\n", __func__, plat_data->csi );
		return -ENOMEM ;
	}

        vfd = video_device_alloc();
	if (!vfd)
		goto unreg_dev;

	*vfd = bdcam_template ;
	ret = video_register_device(vfd, VFL_TYPE_GRABBER, -1);
	if (ret < 0)
		goto rel_dev;

	video_set_drvdata(vfd, cam);

	snprintf(vfd->name, sizeof(vfd->name), "%s (%i)",
			bdcam_template.name, vfd->num);

	cam->vfd = vfd;
	v4l2_info(&cam->v4l2_dev, "V4L2 device registered as /dev/video%d\n", vfd->num);

	printk(KERN_ERR "%s: registered device %d\n", __func__, plat_data->csi );
	list_add_tail(&cam->devlist, &devlist);

	i2c_adapter = i2c_get_adapter(plat_data->i2c_bus);
	printk(KERN_ERR "%s: adapter %p/%s/%d\n", __func__,
	       i2c_adapter,
	       i2c_adapter ? i2c_adapter->name : "...",
	       i2c_adapter ? i2c_adapter->id : -1 );
	printk(KERN_ERR "%s: driver %s\n", __func__,
	       i2c_adapter ?
			(i2c_adapter->dev.driver ? i2c_adapter->dev.driver->name
						 : "no driver")
			: "no adapter" );
#if 0
	cam->subdev = v4l2_i2c_new_subdev(&cam->v4l2_dev,
					  i2c_adapter,
					  plat_data->sensor_name,
					  plat_data->sensor_name,
					  plat_data->i2c_id);
#endif
        memset(&board_info,0,sizeof(board_info));
	strcpy(board_info.type,plat_data->sensor_name);
	board_info.addr = plat_data->i2c_id;
	board_info.platform_data = plat_data ;
	cam->subdev = v4l2_i2c_new_subdev_board(&cam->v4l2_dev,
						i2c_adapter,
						plat_data->sensor_name,
						&board_info,0);
	printk(KERN_ERR "%s: subdev %p\n", __func__, cam->subdev);
	ret = v4l2_subdev_call(cam->subdev, core, init,(u32)&vfd->dev);
	if (0 != ret) {
		printk (KERN_ERR "%s: error %d initializing device\n", __func__, ret );
	} else
		printk (KERN_ERR "%s: initializing camera sensor\n", __func__ );

	return 0 ;

rel_dev:
	kfree(vfd);

unreg_dev:
	v4l2_device_unregister(&cam->v4l2_dev);
	kfree(cam);
	return -EFAULT ;
}

int boundary_camera_driver_remove(struct platform_device *pdev)
{
	printk(KERN_ERR "%s\n", __func__ );

	while (!list_empty(&devlist)) {
		struct list_head *list = devlist.next;
		struct camera_data_t *cam = list_entry(list, struct camera_data_t, devlist);
		list_del(list);

		v4l2_info(&cam->v4l2_dev, "unregistering /dev/video%d\n", cam->vfd->num);
		video_unregister_device(cam->vfd);
		v4l2_device_unregister(&cam->v4l2_dev);
		kfree(cam);
	}
	return 0 ;
}

void boundary_camera_driver_shutdown(struct platform_device *pdev)
{
	printk(KERN_ERR "%s\n", __func__ );
}

int boundary_camera_driver_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk(KERN_ERR "%s\n", __func__ );
	return 0 ;
}

int boundary_camera_driver_suspend_late(struct platform_device *pdev, pm_message_t state)
{
	printk(KERN_ERR "%s\n", __func__ );
	return 0 ;
}

int boundary_camera_driver_resume_early(struct platform_device *pdev)
{
	printk(KERN_ERR "%s\n", __func__ );
	return 0 ;
}

int boundary_camera_driver_resume(struct platform_device *pdev)
{
	printk(KERN_ERR "%s\n", __func__ );
	return 0 ;
}

/*!
 * This structure contains pointers to the power management callback functions.
 */
static struct platform_driver boundary_camera_driver = {
	.driver = {
		   .name = "boundary_camera",
		   },
	.probe = boundary_camera_driver_probe,
	.remove = boundary_camera_driver_remove,
	.suspend = boundary_camera_driver_suspend,
	.resume = boundary_camera_driver_resume,
	.shutdown = boundary_camera_driver_shutdown,
};

/*!
 * Entry point for the V4L2
 *
 * @return  Error code indicating success or failure
 */
static __init int camera_init(void)
{
	int err ;
	printk(KERN_ERR "%s\n", __func__ );

	camera_phys = 0 ;
	camera_virt = 0 ;

        INIT_LIST_HEAD(&devlist);

	DEBUGMSG ("%s:camera_init\n", __FILE__);

	err = platform_driver_register(&boundary_camera_driver);
	if (err) {
		pr_err("%s:camera_init register fail", __FILE__ );
		return err;
	}

	return 0 ;
}

/*!
 * Exit and cleanup for the V4L2
 */
static void __exit camera_exit(void)
{
	printk(KERN_ERR "%s\n", __func__ );
        platform_driver_unregister(&boundary_camera_driver);
}

module_init(camera_init);
module_exit(camera_exit);

MODULE_AUTHOR("Boundary Devices, Inc.");
MODULE_DESCRIPTION("V4L2 Camera driver for i.MX5-based cameras");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("video");
