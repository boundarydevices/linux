/*
 * Copyright 2011 Boundary Devices. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <mach/clock.h>
#include <linux/ipu.h>
#include <linux/fsl_devices.h>

#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf-vmalloc.h>

static char *tfp401_mode __devinitdata = "unspecified";
module_param(tfp401_mode, charp, 0);
MODULE_PARM_DESC(tfp401_mode, "video mode");

#define dprintk(dev, fmt, arg...) \
	v4l2_dbg(1, 1, &dev->v4l2_dev, "%s: " fmt, __func__, ## arg)

MODULE_DESCRIPTION("HDMI input driver for TFP-401");
MODULE_AUTHOR("Boundary Devices <eric.nelson@boundarydevices.com>");
MODULE_LICENSE("GPL");

#define V4L2_BUF_FLAG_STATE (V4L2_BUF_FLAG_MAPPED|V4L2_BUF_FLAG_QUEUED|V4L2_BUF_FLAG_DONE)
#define IDMAC_NFACK_0	(2*32+0)		// IDMAC_NFACK_0

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

struct tfp401_data_t {
	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd;

	unsigned		height ;
	unsigned		width ;
	int			hsync_acth ;
	int			vsync_acth ;
	unsigned		hoffs ;
	unsigned		voffs ;
	unsigned		hmarg ;
	unsigned		vmarg ;

	struct mxc_camera_platform_data const *platform_data;

	atomic_t		num_inst;
        spinlock_t 		lock ;

	int			streaming ;
	int			num_frames ;
	struct camera_frame_t  *buf_queued[2];
	struct list_head 	capture_queue;
	struct list_head 	done_queue;
        wait_queue_head_t 	wait ;
	struct camera_frame_t  *frames ;
	unsigned		framesize ;
};

struct resolution_t {
	unsigned w ;
	unsigned h ;
	unsigned hmarg ;
	unsigned hoffs ;
	unsigned vmarg ;
	unsigned voffs ;
};
static struct resolution_t const supported_sizes[] = {
	{  640,  480,   0,   0,   0,   0 }	/* sort by height */
,	{  800,  600, 224, 188,   0,  16 }
,	{ 1280,  720, 320, 204,   0,   4 }
,	{ 1600, 1200, 336, 286,  48,  48 }	/* hacth, vacth for 30Hz */
};

#define TFP401_NAME		"tfp401"
#define SAMPLE_SECONDS 1
#define TRANSITIONS_PER_CYCLE 2

#define MAKE_GP(port, bit) ((port - 1) * 32 + bit)

#define HSYNCPIN	0
#define VSYNCPIN	1
#define NUMPINS		(VSYNCPIN+1)

static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	printk (KERN_ERR "%s\n", __func__ );
	strncpy(cap->driver, TFP401_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, TFP401_NAME, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(0, 1, 0);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	printk (KERN_ERR "%s\n", __func__ );
	return -EINVAL;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	printk (KERN_ERR "%s\n", __func__ );
	return -EINVAL ;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	printk (KERN_ERR "%s\n", __func__ );
	return -EINVAL;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct tfp401_data_t *dev = video_drvdata(file);
	printk (KERN_ERR "%s\n", __func__ );
	if (V4L2_PIX_FMT_YVU420 != f->fmt.pix.pixelformat) {
		printk (KERN_ERR "%s: only support 0x%x\n", __func__, V4L2_PIX_FMT_YVU420);
		return -EINVAL ;
	}

	if (V4L2_FIELD_ANY != f->fmt.pix.field) {
		printk (KERN_ERR "%s: only support field 0x%x, not 0x%x\n", __func__, V4L2_FIELD_ANY,f->fmt.pix.field);
		return -EINVAL ;
	}

	/* V4L2 specification suggests the driver corrects the format struct
	 * if any of the dimensions is unsupported */
	f->fmt.pix.height = dev->height ;
	f->fmt.pix.width = dev->width ;

	f->fmt.pix.width &= ~7 ;
	f->fmt.pix.bytesperline = f->fmt.pix.width ;
	f->fmt.pix.sizeimage = dev->width*dev->height;

	return 0;
}

static int enable_camera
	( struct tfp401_data_t *dev,
	  unsigned csi)
{
	int err ;
	ipu_channel_params_t enc;
	ipu_csi_signal_cfg_t csi_param ;

	ipu_csi_set_window_size(dev->width,dev->height, csi);
	ipu_csi_set_window_pos(dev->hoffs, dev->voffs, csi);

	csi_param.sens_clksrc = 0 ; // never used
	csi_param.clk_mode = 0 ;
	csi_param.data_pol = 0 ;
	csi_param.ext_vsync = 1 ;
	csi_param.pack_tight = 0 ;
	csi_param.force_eof = 0 ;
	csi_param.data_en_pol = 0 ;
	csi_param.mclk = 0 ;
	csi_param.pixclk_pol = 0 ;
	csi_param.csi = csi;
	csi_param.data_width = IPU_CSI_DATA_WIDTH_8;
	csi_param.Vsync_pol = dev->vsync_acth^1 ;
	csi_param.Hsync_pol = dev->hsync_acth^1 ;
	csi_param.data_fmt = IPU_PIX_FMT_GENERIC ;

	printk(KERN_ERR "init_csi: %ux%u +%u+%u\n", dev->width, dev->height, dev->hmarg, dev->vmarg);
	err = ipu_csi_init_interface(dev->width+dev->hmarg,dev->height+dev->vmarg,IPU_PIX_FMT_GENERIC,csi_param);
	if (err) {
		printk (KERN_ERR "%s: error %d from ipu_csi_init_interface\n", __func__, err );
	}

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

	printk(KERN_ERR "init channel buffer %x\n", CSI_MEM );
	printk(KERN_ERR "%s: forcing NV12 output\n", __func__ );
	err = ipu_init_channel_buffer(CSI_MEM, IPU_OUTPUT_BUFFER,
				      IPU_PIX_FMT_GENERIC,
				      dev->width,
				      dev->height,
				      dev->width,
				      0,
				      camera_phys,
				      camera_phys,
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

static int allocate_frames (struct tfp401_data_t *dev,
			    unsigned count)
{
	int i;
	unsigned size = (dev->width*dev->height*3)/2 ;
	unsigned frame_size = PAGE_ALIGN(size);
	unsigned total_size = count*frame_size ;
	unsigned next_phys ;
	void 	*next_virt ;

	pr_err("%s: %ux%u: %u frames of %u bytes each\n", __func__, dev->width, dev->height, count, size );

	BUG_ON(dev->streaming);
	BUG_ON(dev->frames);
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

	dev->frames = kmalloc(count*sizeof(dev->frames[0]),GFP_KERNEL);
	if (0 == dev->frames)
		return -ENOBUFS ;
	memset(dev->frames,0,count*sizeof(dev->frames[0]));
	dev->num_frames = count ;
	dev->framesize = frame_size ;
	for (i = 0; i < count; i++) {
		dev->frames[i].vaddress = next_virt ;
		dev->frames[i].paddress = next_phys ;
		next_virt = (char *)next_virt + frame_size ;
		next_phys += frame_size ;
		dev->frames[i].buffer.index = i ;
		dev->frames[i].buffer.flags = V4L2_BUF_FLAG_MAPPED;
		dev->frames[i].buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		dev->frames[i].buffer.length = size ;
		dev->frames[i].buffer.memory = V4L2_MEMORY_MMAP;
		dev->frames[i].buffer.m.offset = dev->frames[i].paddress;
		INIT_LIST_HEAD(&dev->frames[i].queue);
		pr_err("%u: %08x: virt %p: size 0x%x, page 0x%x\n",i,dev->frames[i].paddress,dev->frames[i].vaddress, size,frame_size);
	}

	return 0;
}

static void feed_frames(struct tfp401_data_t *dev)
{
	int i ;
	unsigned long flags ;

	if (list_empty(&dev->capture_queue))
		return ;

	spin_lock_irqsave(&dev->lock,flags);

	for (i = 0; i < ARRAY_SIZE(dev->buf_queued); i++) {
		if (!dev->buf_queued[i]) {
			if (!list_empty(&dev->capture_queue)) {
				int err ;
				struct camera_frame_t *frame = list_entry(dev->capture_queue.next, struct camera_frame_t, queue);
				if (!frame) {
					pr_err ("%s: no more buffers %u\n", __func__, i );
					break;
				}
				pr_err ("%s: queue buffer %u\n", __func__, i );
				list_del(dev->capture_queue.next);
				err = ipu_update_channel_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, i, frame->buffer.m.offset);
				if (err != 0)
					printk (KERN_ERR "%s: err %d buffer %d/0x%x\n", __func__, err, i,frame->buffer.m.offset);
				ipu_select_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, i);
				dev->buf_queued[i] = frame ;
			}
			else
				break;
		} else {
			pr_err ("%s: buffers %u already queued\n", __func__, i );
		}
	}
	spin_unlock_irqrestore(&dev->lock,flags);
}

static void deliver_frame(struct tfp401_data_t *dev,unsigned buf)
{
	int was_ready = ipu_check_buffer_busy(CSI_MEM,IPU_OUTPUT_BUFFER,buf);
	struct camera_frame_t *frame = dev->buf_queued[buf];
	BUG_ON(buf >= ARRAY_SIZE(dev->buf_queued));
	BUG_ON(!dev->buf_queued[buf]);
	dev->buf_queued[buf] = 0 ;
	frame->buffer.flags |= V4L2_BUF_FLAG_DONE ;
	frame->buffer.flags &= ~V4L2_BUF_FLAG_QUEUED ;
        do_gettimeofday(&frame->buffer.timestamp);
	list_add_tail(&frame->queue,&dev->done_queue);
	wake_up_all(&dev->wait);
	// pr_err ("%s: woke up all\n", __func__ );
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
        struct tfp401_data_t *dev = (struct tfp401_data_t *)dev_id ;
	int curbuf = ipu_get_cur_buffer_idx(CSI_MEM, IPU_OUTPUT_BUFFER);
	int prevbuf = curbuf^1 ;
	unsigned curmask = 1<<curbuf ;
	unsigned prevmask = curmask^3 ;
	int bufsready = ipu_buffers_ready(CSI_MEM, IPU_OUTPUT_BUFFER);
	if ((0 == (bufsready & prevmask))
	    &&
	    !list_empty(&dev->capture_queue)){
		int err ;
		unsigned long flags ;
		struct camera_frame_t *frame ;
		spin_lock_irqsave(&dev->lock,flags);
		deliver_frame(dev,prevbuf);
		frame = list_entry(dev->capture_queue.next, struct camera_frame_t, queue);
		list_del(dev->capture_queue.next);
		err = ipu_update_channel_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, prevbuf, frame->buffer.m.offset);
		if (err != 0)
			printk (KERN_ERR "%s: err %d buffer %d/0x%x\n", __func__, err, prevbuf,frame->buffer.m.offset);
		ipu_select_buffer(CSI_MEM, IPU_OUTPUT_BUFFER, prevbuf);
		dev->buf_queued[prevbuf] = frame ;
		spin_unlock_irqrestore(&dev->lock,flags);
	}
	return IRQ_HANDLED;
}

static void stop_streaming(struct tfp401_data_t *data)
{
	unsigned csi = 0 ; /* TODO: fix csi */

	pr_err ("%s\n", __func__ );
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

/*!
 * Free frame buffers status
 *
 * @param dev    Structure cam_data *
 *
 * @return none
 */
static void free_frames(struct tfp401_data_t *dev)
{
	int i;

	pr_err("%s: %d frames\n", __func__, dev ? dev->num_frames : -1 );

	BUG_ON(dev->streaming);
	if (0 == dev->frames)
		return ;
	BUG_ON(0 == camera_virt);
	BUG_ON(0 == camera_phys);

	for (i = 0 ; i < dev->num_frames ; i++) {
		struct camera_frame_t *f = dev->frames+i ;
		if ((f->vaddress != 0) && (f->paddress != 0)){
			f->vaddress = 0;
		} else {
			pr_err ("%s: frame %d has no vaddr\n", __func__, i );
		}
	}
	dev->num_frames = 0 ;
	kfree(dev->frames);
	dev->frames = 0 ;
	dev->framesize = 0 ;
	iounmap(camera_virt);
	camera_virt = 0 ;
	camera_phys = 0 ;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *req)
{
	struct tfp401_data_t *dev = video_drvdata(file);
	int retval = -EINVAL ;
	printk (KERN_ERR "%s\n", __func__ );

	pr_err("   case VIDIOC_REQBUFS\n");
	if (dev->streaming) {
		pr_err( "%s: can't allocate buffers while streaming\n", __func__ );
		retval = -EINVAL ;
	} else if ((req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) ||
	           (req->memory != V4L2_MEMORY_MMAP)) {
		pr_err("wrong buffer type\n");
		retval = -EINVAL;
	} else {
		retval = allocate_frames(dev,req->count);
	}

	return retval ;
}

static int vidioc_querybuf(struct file *file, void *priv,
			   struct v4l2_buffer *buf)
{
	struct tfp401_data_t *dev = video_drvdata(file);
	int retval = -EINVAL ;
	unsigned i = buf->index ;

	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		pr_err("wrong buffer type\n");
		retval = -EINVAL;
	} else if (i >= dev->num_frames) {
		pr_err("Invalid index %d\n", i);
		retval = -EINVAL ;
	} else {
		memcpy(buf, &(dev->frames[i].buffer), sizeof(*buf));
		retval = 0 ;
	}
	return retval;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct tfp401_data_t *dev = video_drvdata(file);
	int retval = -EINVAL ;
	int i = buf->index;
	struct v4l2_buffer *ourbuf = &dev->frames[i].buffer ;
	BUG_ON ((unsigned)i >= dev->num_frames);

	if (buf->m.offset && (ourbuf->m.offset != buf->m.offset)) {
		pr_err( "Invalid offset 0x%x != 0x%x\n", ourbuf->m.offset, buf->m.offset);
	} else if ((ourbuf->flags & V4L2_BUF_FLAG_STATE) == V4L2_BUF_FLAG_MAPPED) {
		unsigned long flags ;
		spin_lock_irqsave(&dev->lock,flags);
		ourbuf->flags |= V4L2_BUF_FLAG_QUEUED;

		list_add_tail(&dev->frames[i].queue,&dev->capture_queue);

		spin_unlock_irqrestore(&dev->lock,flags);
		retval = 0 ;
	} else if (ourbuf->flags & V4L2_BUF_FLAG_QUEUED) {
		pr_err("buffer already queued\n");
	} else if (ourbuf->flags & V4L2_BUF_FLAG_DONE) {
		pr_err("overwrite done buffer.\n");
	} else {
		pr_err("unknown flag combination: %x\n", ourbuf->flags);
	}

	buf->flags = ourbuf->flags;
	return retval ;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct tfp401_data_t *dev = video_drvdata(file);
	int retval = -EINVAL ;

	if (list_empty(&dev->done_queue) && (file->f_flags & O_NONBLOCK)) {
		retval = -EAGAIN;
	} else {
		wait_event_interruptible(dev->wait,!list_empty(&dev->done_queue));
		if (signal_pending(current)) {
			pr_err ("interrupt received\n");
			retval = -ERESTARTSYS;
		} else if( !list_empty(&dev->done_queue) ){
			unsigned long flags ;
			struct camera_frame_t *frame ;

			spin_lock_irqsave(&dev->lock,flags);
			frame = (struct camera_frame_t *)dev->done_queue.next ;
			list_del(dev->done_queue.next);
			spin_unlock_irqrestore(&dev->lock,flags);

			if (frame->buffer.flags & V4L2_BUF_FLAG_DONE) {
				frame->buffer.flags &= ~V4L2_BUF_FLAG_DONE;
				buf->bytesused = dev->framesize ;
				buf->index = frame->buffer.index;
				buf->flags = frame->buffer.flags;
				buf->m.offset = frame->buffer.m.offset ;
				buf->timestamp = frame->buffer.timestamp;
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
	}

	return retval ;
}

static int vidioc_streamon(struct file *file, void *priv,
			   enum v4l2_buf_type type)
{
	struct tfp401_data_t *dev = video_drvdata(file);
	int retval = -EINVAL ;
	printk (KERN_ERR "%s:%p\n", __func__, dev );
	do {
		if (dev->streaming) {
			pr_err( "already streaming\n" );
			break;
		}
		pr_err ("enabling camera\n");
		retval = enable_camera(dev,dev->platform_data->csi);
		if (0 != retval) {
			pr_err("Error %d enabling camera\n", retval );
			break;
		}

		retval = ipu_request_irq(IPU_IRQ_CSI0_OUT_EOF, eof_callback, 0, "camera", dev);
		if (0 != retval) {
			pr_err("Error %d setting IRQ handler\n", retval );
			break;
		}

		retval = ipu_request_irq(IDMAC_NFACK_0, sof_callback, 0, "camera", dev);
		if (0 != retval) {
			pr_err("Error %d setting IRQ handler\n", retval );
			break;
		}

		feed_frames(dev);
		dev->streaming = 1 ;
		ipu_enable_csi(dev->platform_data->csi);
		printk (KERN_ERR "%s: enabled csi\n", __func__ );
	} while ( 0 );

	return retval;
}

static int vidioc_streamoff(struct file *file, void *priv,
			    enum v4l2_buf_type type)
{
	printk (KERN_ERR "%s\n", __func__ );
	return -EINVAL;
}

static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	printk (KERN_ERR "%s\n", __func__ );
	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	printk (KERN_ERR "%s\n", __func__ );
	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	printk (KERN_ERR "%s\n", __func__ );
	return -EINVAL;
}


static const struct v4l2_ioctl_ops tfp401_ioctl_ops = {
	.vidioc_querycap	= vidioc_querycap,

	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap	= vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap	= vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap	= vidioc_s_fmt_vid_cap,

	.vidioc_reqbufs		= vidioc_reqbufs,
	.vidioc_querybuf	= vidioc_querybuf,

	.vidioc_qbuf		= vidioc_qbuf,
	.vidioc_dqbuf		= vidioc_dqbuf,

	.vidioc_streamon	= vidioc_streamon,
	.vidioc_streamoff	= vidioc_streamoff,

	.vidioc_queryctrl	= vidioc_queryctrl,
	.vidioc_g_ctrl		= vidioc_g_ctrl,
	.vidioc_s_ctrl		= vidioc_s_ctrl,
};

/*
 * File operations
 */
static int tfp401_open(struct file *file)
{
	struct tfp401_data_t *dev = video_drvdata(file);
	int usage = atomic_inc_return(&dev->num_inst);

	printk (KERN_ERR "%s: usage %u\n", __func__, usage);
	return 0;
}

static int tfp401_release(struct file *file)
{
	struct tfp401_data_t *dev = video_drvdata(file);
	int usage = atomic_dec_return(&dev->num_inst);
	printk (KERN_ERR "%s: usage %u\n", __func__, usage);
	if (0 == usage) {
		if(dev->streaming)
			stop_streaming(dev);
		free_frames(dev);
	}
	return 0;
}

static unsigned int tfp401_poll(struct file *file,
				 struct poll_table_struct *wait)
{
	struct tfp401_data_t *dev = video_drvdata(file);
	int res = 0 ;

	if (list_empty(&dev->done_queue)) {
		poll_wait(file, &dev->wait, wait);
		if (!list_empty(&dev->done_queue))
			res = POLLIN | POLLRDNORM ;
	} else
		res = POLLIN | POLLRDNORM ;

	return res ;
}

static int tfp401_mmap(struct file *file, struct vm_area_struct *vma)
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
	return 0;
}

static const struct v4l2_file_operations tfp401_fops = {
	.owner		= THIS_MODULE,
	.open		= tfp401_open,
	.release	= tfp401_release,
	.poll		= tfp401_poll,
	.ioctl		= video_ioctl2,
	.mmap		= tfp401_mmap,
};

static struct video_device tfp401_videodev = {
	.name		= TFP401_NAME,
	.fops		= &tfp401_fops,
	.ioctl_ops	= &tfp401_ioctl_ops,
	.minor		= -1,
	.release	= video_device_release,
};

static unsigned gpios[NUMPINS] = {
	MAKE_GP (5,19)			// HSYNC
,	MAKE_GP (5,21)			// VSYNC
};

static char const * const gpio_names[NUMPINS] = {
	"HSYNC   "
,	"VSYNC   "
};

static char const * const active_high[] = {
	"low"
,	"high"
};

static int count_transitions (struct tfp401_data_t *dev) {
        int rval = -1 ;
	unsigned prevvals[NUMPINS] = {
		2
	,	2
	};
	unsigned samecount[NUMPINS] = {
		0
	,	0
	};
	unsigned transitions[NUMPINS] = {
		0
	,	0
	};
	unsigned zeros[NUMPINS] = {
		0
	,	0
	};
	unsigned ones[NUMPINS] = {
		0
	,	0
	};
	unsigned long start = jiffies ;
	unsigned long end=start+(SAMPLE_SECONDS*HZ);
	unsigned pin ;
	while (0 > (int)(jiffies-end)) {
		unsigned i ;
		for (i = 0 ; i < ARRAY_SIZE(gpios); i++) {
			int val = gpio_get_value(gpios[i]);
			if (val != prevvals[i]) {
				++transitions[i];
			} else {
				++samecount[i];
			}
			prevvals[i] = val ;
			if (val) {
				++zeros[i];
			} else {
				++ones[i];
			}
		}
//		yield();
	}
	for (pin = 0 ; pin < ARRAY_SIZE(gpios); pin++) {
		printk (KERN_ERR "%s: pin %s: %u transitions, %u same\n", __func__, gpio_names[pin], transitions[pin], samecount[pin]);
		printk (KERN_ERR "%7u zeros, %7u ones\n", zeros[pin], ones[pin]);
		printk (KERN_ERR "current state %d\n", prevvals[pin]);
	}
	if (0 < transitions[VSYNCPIN]) {
		int i ;
		int lines = transitions[HSYNCPIN]/transitions[VSYNCPIN];
                struct resolution_t const *best = 0 ;
		unsigned rows_per_vsync = transitions[HSYNCPIN]/transitions[VSYNCPIN];
		printk (KERN_ERR "%s: %u Hz refresh\n",__func__, transitions[VSYNCPIN]/(SAMPLE_SECONDS*TRANSITIONS_PER_CYCLE));
		printk (KERN_ERR "%s: %u rows per screen\n", __func__, rows_per_vsync);
		for (i = 0; i < ARRAY_SIZE(supported_sizes); i++ ) {
			if (lines > supported_sizes[i].h) {
				best = &supported_sizes[i];
			}
		}
		if (best) {
			printk (KERN_ERR "%s: selected resolution %ux%u margins %ux%u\n", __func__, best->w, best->h, best->hmarg, best->vmarg);
			dev->width = best->w ;
			dev->height = best->h ;
			dev->vmarg = rows_per_vsync-best->h ;
			dev->hmarg = best->hmarg ;
			dev->hoffs = best->hoffs ;
			dev->voffs = best->voffs ;

			/*
			 * HSYNC/VSYNC pulses are shorter than the data
			 */
			dev->hsync_acth = (ones[HSYNCPIN] < zeros[HSYNCPIN]);
			dev->vsync_acth = (ones[VSYNCPIN] < zeros[VSYNCPIN]);
			printk ("%s: hsync active %s, vsync active %s\n", __func__,
				active_high[dev->hsync_acth],
				active_high[dev->vsync_acth]);

			if (tfp401_mode && ('0'<=*tfp401_mode) && ('9' >= *tfp401_mode)) {
				unsigned w, h, hacth,hmarg,hoffs,vacth,vmarg,voffs ;
				printk (KERN_ERR "override with command-line argument %s here\n", tfp401_mode);
				if (8 == sscanf(tfp401_mode,"%ux%u:%u,%u,%u/%u,%u,%u",
						&w,&h,&hacth,&hmarg,&hoffs,&vacth,&vmarg,&voffs)) {
					printk( KERN_ERR "use settings %ux%u\n"
							 "   hsync active %s, margin %u, offs %u\n"
							 "   vsync active %s, margin %u, offs %u\n",
						w,h,
                                                active_high[0 != hacth], hmarg,hoffs,
                                                active_high[0 != vacth], vmarg,voffs);
					dev->width = w ;
					dev->height =h ;
					dev->vmarg = vmarg ;
					dev->hmarg = hmarg ;
					dev->hoffs = hoffs ;
					dev->voffs = voffs ;
				}
			}
			rval = 0 ;
		}
	} else {
		printk (KERN_ERR "%s: not connected\n", __func__);
	}

	return rval ;
}

static int tfp401_probe(struct platform_device *pdev)
{
	struct tfp401_data_t *dev;
	struct video_device *vfd;
	int ret;
	struct mxc_camera_platform_data *plat_data = pdev->dev.platform_data ;

	printk (KERN_ERR "%s\n", __func__ );

	dev = kzalloc(sizeof *dev, GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	dev->platform_data = plat_data ;
	count_transitions(dev);

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret)
		goto free_dev;

	atomic_set(&dev->num_inst, 0);

	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto unreg_dev;
	}

	*vfd = tfp401_videodev;

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, 0);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		goto rel_vdev;
	}

	spin_lock_init(&dev->lock);
	INIT_LIST_HEAD(&dev->capture_queue);
	INIT_LIST_HEAD(&dev->done_queue);
        init_waitqueue_head(&dev->wait);
	video_set_drvdata(vfd, dev);
	snprintf(vfd->name, sizeof(vfd->name), "%s", tfp401_videodev.name);
	dev->vfd = vfd;
	v4l2_info(&dev->v4l2_dev, "device registered as /dev/video%d\n", vfd->num);

	platform_set_drvdata(pdev, dev);

	return 0;

rel_vdev:
	video_device_release(vfd);
unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
free_dev:
	kfree(dev);

	return ret;
}

static int tfp401_remove(struct platform_device *pdev)
{
	struct tfp401_data_t *dev =
		(struct tfp401_data_t *)platform_get_drvdata(pdev);

	v4l2_info(&dev->v4l2_dev, "removing module\n");
	video_unregister_device(dev->vfd);
	v4l2_device_unregister(&dev->v4l2_dev);
	kfree(dev);

	return 0;
}

static struct platform_driver tfp401_pdrv = {
	.probe		= tfp401_probe,
	.remove		= tfp401_remove,
	.driver		= {
		.name	= TFP401_NAME,
		.owner	= THIS_MODULE,
	},
};

#ifndef MODULE
/*
 * Parse user specified options (`tfp=')
 */
static int __init tfp401_setup(char *options)
{
	printk (KERN_ERR "%s: mode=%s\n", __func__, tfp401_mode );
	return 0 ;
}

__setup("tfp401_mode", tfp401_setup);
#endif

static void __exit tfp401_exit(void)
{
	printk (KERN_ERR "%s: mode=%s\n", __func__, tfp401_mode );
	platform_driver_unregister(&tfp401_pdrv);
}

static int __init tfp401_init(void)
{
	int ret;

	printk (KERN_ERR "%s\n", __func__ );

	ret = platform_driver_register(&tfp401_pdrv);
	if (ret)
		printk (KERN_ERR "%s: failed: %d\n", __func__, ret);

	return ret;
}

module_init(tfp401_init);
module_exit(tfp401_exit);


