/*
 *
 * device driver for TW6868 based PCIe analog video capture cards
 * video4linux video interface
 *
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
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sort.h>

#include <media/v4l2-common.h>
/// #include <media/rds.h>

#include "TW68.h"
#include "TW68_defines.h"

/* ------------------------------------------------------------------ */

static unsigned int gbuffers      = 8;
static unsigned int noninterlaced; /* 0 */
static unsigned int gbufsize      = 800*576*4;
static unsigned int gbufsize_max  = 800*576*4;
static char secam[] = "--";
static unsigned int VideoFrames_limit = 64;    //16

module_param(gbuffers, int, 0444);
MODULE_PARM_DESC(gbuffers,"number of capture buffers, range 2-32");
module_param(noninterlaced, int, 0644);
MODULE_PARM_DESC(noninterlaced,"capture non interlaced video");
module_param_string(secam, secam, sizeof(secam), 0644);
MODULE_PARM_DESC(secam, "force SECAM variant, either DK,L or Lc");

/* ------------------------------------------------------------------ */
/* data structs for video                                             */
/*
static int video_out[][9] = {
	[CCIR656] = { 0x00, 0xb1, 0x00, 0xa1, 0x00, 0x04, 0x06, 0x00, 0x00 },
};
*/

static struct TW68_format formats[] = {
	{
		.name     = "15 bpp RGB, le",
		.fourcc   = V4L2_PIX_FMT_RGB555,
		.depth    = 16,
		.pm       = 0x13 | 0x80,
	},{
		.name     = "16 bpp RGB, le",
		.fourcc   = V4L2_PIX_FMT_RGB565,
		.depth    = 16,
		.pm       = 0x10 | 0x80,
	},{
		.name     = "4:2:2 packed, YUYV",
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.depth    = 16,
		.pm       = 0x00,
		.bswap    = 1,
		.yuv      = 1,
	},{
		.name     = "4:2:2 packed, UYVY",
		.fourcc   = V4L2_PIX_FMT_UYVY,
		.depth    = 16,
		.pm       = 0x00,
		.yuv      = 1,
	}
};

#define FORMATS ARRAY_SIZE(formats)

#define NORM_625_50			\
		.h_start       = 0,	\
		.h_stop        = 719,	\
		.video_v_start = 24,	\
		.video_v_stop  = 311,	\
		.vbi_v_start_0 = 7,	\
		.vbi_v_stop_0  = 22,	\
		.vbi_v_start_1 = 319,   \
		.src_timing    = 4

#define NORM_525_60			\
		.h_start       = 0,	\
		.h_stop        = 719,   \
		.video_v_start = 23,	\
		.video_v_stop  = 262,	\
		.vbi_v_start_0 = 10,	\
		.vbi_v_stop_0  = 21,	\
		.vbi_v_start_1 = 273,	\
		.src_timing    = 7

static struct TW68_tvnorm tvnorms[] = {
	{
		.name          = "PAL", /* autodetect */
		.id            = V4L2_STD_PAL,
		NORM_625_50,

		.sync_control  = 0x18,
		.luma_control  = 0x40,
		.chroma_ctrl1  = 0x81,
		.chroma_gain   = 0x2a,
		.chroma_ctrl2  = 0x06,
		.vgate_misc    = 0x1c,

	},{
		.name          = "PAL-BG",
		.id            = V4L2_STD_PAL_BG,
		NORM_625_50,

		.sync_control  = 0x18,
		.luma_control  = 0x40,
		.chroma_ctrl1  = 0x81,
		.chroma_gain   = 0x2a,
		.chroma_ctrl2  = 0x06,
		.vgate_misc    = 0x1c,

	},{
		.name          = "PAL-I",
		.id            = V4L2_STD_PAL_I,
		NORM_625_50,

		.sync_control  = 0x18,
		.luma_control  = 0x40,
		.chroma_ctrl1  = 0x81,
		.chroma_gain   = 0x2a,
		.chroma_ctrl2  = 0x06,
		.vgate_misc    = 0x1c,

	},{
		.name          = "PAL-DK",
		.id            = V4L2_STD_PAL_DK,
		NORM_625_50,

		.sync_control  = 0x18,
		.luma_control  = 0x40,
		.chroma_ctrl1  = 0x81,
		.chroma_gain   = 0x2a,
		.chroma_ctrl2  = 0x06,
		.vgate_misc    = 0x1c,

	},{
		.name          = "NTSC",
		.id            = V4L2_STD_NTSC,
		NORM_525_60,

		.sync_control  = 0x59,
		.luma_control  = 0x40,
		.chroma_ctrl1  = 0x89,
		.chroma_gain   = 0x2a,
		.chroma_ctrl2  = 0x0e,
		.vgate_misc    = 0x18,

	},{
		.name          = "SECAM",
		.id            = V4L2_STD_SECAM,
		NORM_625_50,

		.sync_control  = 0x18,
		.luma_control  = 0x1b,
		.chroma_ctrl1  = 0xd1,
		.chroma_gain   = 0x80,
		.chroma_ctrl2  = 0x00,
		.vgate_misc    = 0x1c,

	},{
		.name          = "SECAM-DK",
		.id            = V4L2_STD_SECAM_DK,
		NORM_625_50,

		.sync_control  = 0x18,
		.luma_control  = 0x1b,
		.chroma_ctrl1  = 0xd1,
		.chroma_gain   = 0x80,
		.chroma_ctrl2  = 0x00,
		.vgate_misc    = 0x1c,

	},{
		.name          = "SECAM-L",
		.id            = V4L2_STD_SECAM_L,
		NORM_625_50,

		.sync_control  = 0x18,
		.luma_control  = 0x1b,
		.chroma_ctrl1  = 0xd1,
		.chroma_gain   = 0x80,
		.chroma_ctrl2  = 0x00,
		.vgate_misc    = 0x1c,

	},{
		.name          = "SECAM-Lc",
		.id            = V4L2_STD_SECAM_LC,
		NORM_625_50,

		.sync_control  = 0x18,
		.luma_control  = 0x1b,
		.chroma_ctrl1  = 0xd1,
		.chroma_gain   = 0x80,
		.chroma_ctrl2  = 0x00,
		.vgate_misc    = 0x1c,

	},{
		.name          = "PAL-M",
		.id            = V4L2_STD_PAL_M,
		NORM_525_60,

		.sync_control  = 0x59,
		.luma_control  = 0x40,
		.chroma_ctrl1  = 0xb9,
		.chroma_gain   = 0x2a,
		.chroma_ctrl2  = 0x0e,
		.vgate_misc    = 0x18,

	},{
		.name          = "PAL-Nc",
		.id            = V4L2_STD_PAL_Nc,
		NORM_625_50,

		.sync_control  = 0x18,
		.luma_control  = 0x40,
		.chroma_ctrl1  = 0xa1,
		.chroma_gain   = 0x2a,
		.chroma_ctrl2  = 0x06,
		.vgate_misc    = 0x1c,

	},{
		.name          = "PAL-60",
		.id            = V4L2_STD_PAL_60,

		.h_start       = 0,
		.h_stop        = 719,
		.video_v_start = 23,
		.video_v_stop  = 262,
		.vbi_v_start_0 = 10,
		.vbi_v_stop_0  = 21,
		.vbi_v_start_1 = 273,
		.src_timing    = 7,

		.sync_control  = 0x18,
		.luma_control  = 0x40,
		.chroma_ctrl1  = 0x81,
		.chroma_gain   = 0x2a,
		.chroma_ctrl2  = 0x06,
		.vgate_misc    = 0x1c,
	}
};
#define TVNORMS ARRAY_SIZE(tvnorms)

#define V4L2_CID_PRIVATE_INVERT      (V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_PRIVATE_Y_ODD       (V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_PRIVATE_Y_EVEN      (V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_PRIVATE_AUTOMUTE    (V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_PRIVATE_LASTP1      (V4L2_CID_PRIVATE_BASE + 4)

static const struct v4l2_queryctrl no_ctrl = {
	.name  = "42",
	.flags = V4L2_CTRL_FLAG_DISABLED,
};
static const struct v4l2_queryctrl video_ctrls[] = {
	/* --- video --- */
	{
		.id            = V4L2_CID_BRIGHTNESS,
		.name          = "Brightness",
		.minimum       = 0,
		.maximum       = 255,
		.step          = 1,
		.default_value = 125,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_CONTRAST,
		.name          = "Contrast",
		.minimum       = 0,
		.maximum       = 200,  //127
		.step          = 1,
		.default_value = 96,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_SATURATION,
		.name          = "Saturation",
		.minimum       = 0,
		.maximum       = 127,
		.step          = 1,
		.default_value = 64,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_HUE,
		.name          = "Hue",
		.minimum       = -124,   //-128,
		.maximum       = 125,    // 127,
		.step          = 1,
		.default_value = 0,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_HFLIP,
		.name          = "Mirror",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},
	/* --- audio --- */
	{
		.id            = V4L2_CID_AUDIO_MUTE,
		.name          = "Mute",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_AUDIO_VOLUME,
		.name          = "Volume",
		.minimum       = -15,
		.maximum       = 15,
		.step          = 1,
		.default_value = 0,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},
	/* --- private --- */
	{
		.id            = V4L2_CID_PRIVATE_INVERT,
		.name          = "Invert",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_PRIVATE_Y_ODD,
		.name          = "y offset odd field",
		.minimum       = 0,
		.maximum       = 128,
		.step          = 1,
		.default_value = 0,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_PRIVATE_Y_EVEN,
		.name          = "y offset even field",
		.minimum       = 0,
		.maximum       = 128,
		.step          = 1,
		.default_value = 0,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_PRIVATE_AUTOMUTE,
		.name          = "automute",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	}
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

static struct TW68_format* format_by_fourcc(unsigned int fourcc)
{
	unsigned int i;

	for (i = 0; i < FORMATS; i++) {
		if (formats[i].fourcc == fourcc) {
			pr_debug("%s:%d - %x\n", __func__, i, fourcc);
			return formats+i;
		}
	}
	pr_info("%s: %x not found\n", __func__, fourcc);
	return NULL;
}

/* ----------------------------------------------------------------------- */
/* resource management                                                     */

static int res_get(struct TW68_dev *dev, struct TW68_fh *fh, unsigned int bit)
{
	u32 nId;

	nId = fh->DMA_nCH;
	if (nId == 0xF)  nId = 0;

	//if (nId >4)   return 0;  // this for 6864/6868  4 Ch

	/* is it already allocated */
	if (fh->resources & bit)
		return 1;

	/* is it free? */
	mutex_lock(&dev->lock);
	if (dev->resources[nId] & bit) {
		/* no, someone else uses it */
		mutex_unlock(&dev->lock);

		pr_debug("%s: %d dev->resources[nId] %x\n", __func__, bit, dev->resources[nId]);
		return 0;
	}
	/* it's free, grab it */
	fh->resources  |= bit;

	//dev->resources[nId] |= bit;
	pr_debug("%s: %d\n", __func__, bit);
	mutex_unlock(&dev->lock);
	return 1;
}

static int res_check(struct TW68_fh *fh, unsigned int bit)
{
	return (fh->resources & bit);
}

static int res_locked(struct TW68_fh *fh, struct TW68_dev *dev, unsigned int bit)
{
	u32 nId = fh->DMA_nCH;

	if (nId == 0xF)  nId = 0;
	// if (nId >4)   return 0;

	return (dev->resources[nId] & bit);
}

static
void res_free(struct TW68_fh *fh, unsigned int bits)
{
	struct TW68_dev *dev = fh->dev;

	//BUG_ON((fh->resources & bits) != bits);

	u32 nId = fh->DMA_nCH;
	if (nId == 0xF)  nId = 0;
	//if (nId >4)   return 0;

	mutex_lock(&dev->lock);
	fh->resources  &= ~bits;
	dev->resources[nId] &= ~bits;
	pr_debug("%s: put %d\n", __func__, bits);
	mutex_unlock(&dev->lock);
}

/* ------------------------------------------------------------------ */

static void set_tvnorm(struct TW68_dev *dev, struct TW68_tvnorm *norm)
{
	int	framesize;

	pr_debug("%s: %s\n", __func__, norm->name);
	dev->tvnorm = norm;

	/* setup cropping */
	dev->crop_bounds.left    = norm->h_start;
	dev->crop_defrect.left   = norm->h_start;
	dev->crop_bounds.width   = norm->h_stop - norm->h_start +1;
	dev->crop_defrect.width  = norm->h_stop - norm->h_start +1;

	dev->crop_bounds.top     = (norm->vbi_v_stop_0+1)*2;
	dev->crop_defrect.top    = norm->video_v_start*2;
	dev->crop_bounds.height  = ((norm->id & V4L2_STD_525_60) ? 524 : 622) - dev->crop_bounds.top;
	dev->crop_defrect.height = (norm->video_v_stop - norm->video_v_start +1)*2;

	dev->crop_current = dev->crop_defrect;

	framesize = dev->crop_bounds.width * dev->crop_bounds.height * 16 >> 3;    // calculate byte size for 1 frame


	pr_debug("%s: crop setting  %s,  width%d   height%d  size %d\n",
		__func__, norm->name, dev->crop_bounds.width, dev->crop_bounds.height,  framesize);

}

#if 0
static void video_mux(struct TW68_dev *dev, int input)
{
	pr_debug("%s: %d input = %d [%s]\n", __func__, dev->ctl_input, input, card_in(dev, input).name);
	dev->ctl_input = input;

	pr_debug("%s: dev %p  dev->tvnorm=%p\n", __func__, dev, dev->tvnorm);
	if (dev->tvnorm)
		set_tvnorm(dev, dev->tvnorm);
	else
		set_tvnorm(dev, &tvnorms[0]);

}
#endif


/* ------------------------------------------------------------------ */

struct cliplist {
	__u16 position;
	__u8  enable;
	__u8  disable;
};



/* ------------------------------------------------------------------ */

static int buffer_activate(struct TW68_dev *dev, ///unsigned int nId,
			   struct TW68_buf *buf,
			   struct TW68_buf *next)
{
	/// pr_debug("buffer_activate buf=%p\n",buf);
	buf->vb.state = VIDEOBUF_ACTIVE;
	buf->top_seen = 0;

	return 0;  //-1;
}



static void free_buffer(struct videobuf_queue *q, struct TW68_buf *buf)
{
	// struct TW68_fh  *fh = q->priv_data;
	// struct TW68_dev *dev  = fh->dev;

	pr_debug("%s: state= %i\n", __func__, buf->vb.state);

	videobuf_vmalloc_free(&buf->vb);
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
	pr_debug("%s:  %p vb(%p)\n", __func__,  buf, &buf->vb );
}


static int buffer_prepare(struct videobuf_queue *q,
			  struct videobuf_buffer *vb,
			  enum v4l2_field field)
{
	struct TW68_fh *fh = q->priv_data;
	struct TW68_dev *dev = fh->dev;
	struct TW68_buf *buf = container_of(vb,struct TW68_buf,vb);
	unsigned int size;
	unsigned int DMA_nCH = fh->DMA_nCH;
	unsigned int nId = 0;

	int err;

	if (DMA_nCH == 0XF)
		///DMA_nCH =0;
		nId = 0;
        //pr_debug("  >>>>buffer_prepare DMA_nCH%x  %p  vb->baddr:%p,  vb->bsize:%d   \n",  DMA_nCH, vb, vb->baddr, vb->bsize );
	/* sanity checks */
	if (NULL == fh->fmt)
		return -EINVAL;
	////////////////////////////////////////////////////
	/*
	if (fh->width    < 48 ||
	    fh->height   < 32 ||
	    fh->width/4  > dev->crop_current.width  ||
	    fh->height/4 > dev->crop_current.height ||
	    fh->width    > dev->crop_bounds.width  ||
	    fh->height   > dev->crop_bounds.height)
		return -EINVAL;
	*/
	size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if (0 != buf->vb.baddr  &&  buf->vb.bsize < size)
		return -EINVAL;
	////  cause PAL stop
///////////////////////////////////////////////////////////////////////////////
	if (VIDEOBUF_NEEDS_INIT == buf->vb.state)
	{

		buf->vb.width  = fh->width;
		buf->vb.height = fh->height;
		buf->vb.size   = size;
		buf->vb.field  = field;
		buf->fmt       = fh->fmt;
		buf->pt        = &fh->pt_cap;
		nId = DMA_nCH +1;

		dev->video_dmaq[nId].curr = NULL;

		//pr_debug("buffer_prepare INIT  vb->baddr:%x,  vb->bsize:%d   \n", vb->baddr, vb->bsize );

		/*
		 * If I understand correctly, the videobuf_iolock function is
		 * responsible for mapping user pages to kernel. This works fine
		 * as long as user application allocates memory/buffer using
		 * some standard API (malloc/memalign).
		 *
		 * But it fails where; user application has allocated memory/
		 * buffer using ioremap from another driver. The use case
		 * scenario is something -
		 * 	- Open V4L2 device (which supports scatter gather DMA)
		 * 	- Configure the parameters
		 * 	  (especially .memory=V4L2_MEMORY_USERPTR)
		 * 	- Request and query the buffer
		 * 	- open another device which will be responsible for
		 * 	  allocating memory either using ioremap/
		 * 	  dma_alloc_coherent/get_free_pages
		 * 	- Queue the buffers with buffer address received from
		 * 	  previous step
		 *
		 * Here it internally calls drv_prepare function, which call
		 * videobuf_iolock API for mapping the user pages to kernel
		 * and will create scatter-gather (dma->sglist) list. But this
		 * API returns from videobuf_dma_init_user_locked with -EFAULT.
		 *
		 * I found the get_user_pages returns due to flag VM_IO and
		 * VM_PFNMAP for the corresponding vma.
		 *
		 * Any suggestions on how can I achieve this?
		 */
		err = videobuf_iolock(q,&buf->vb,&dev->ovbuf);  //
		if (err< 0)
			goto oops;
	}

	buf->vb.state = VIDEOBUF_PREPARED;
	buf->activate = buffer_activate;  //set activate fn ptr
	return 0;

 oops:
 	pr_info("%s: OOPS\n", __func__);
	free_buffer(q, buf);
	return err;
}


int buffer_setup(struct videobuf_queue *q, unsigned int *count, unsigned int *size)
{
	unsigned int ChannelOffset, nId, pgn;
	u32	m_dwCHConfig, dwReg, dwRegH, dwRegW, nScaler, dwReg2;
	u32  m_StartIdx, m_EndIdx, m_nVideoFormat, \
		 m_bHorizontalDecimate, m_bVerticalDecimate, m_nDropChannelNum, \
		 m_bDropMasterOrSlave, m_bDropField, m_bDropOddOrEven, m_nCurVideoChannelNum;

	struct TW68_dev *dev;
	struct TW68_fh *fh = q->priv_data;
	//struct TW68_buf *buf = container_of(vb,struct TW68_buf,vb);
	dev = fh->dev;

	ChannelOffset = (PAGE_SIZE <<1) /8 /8;
	// NTSC  FIELD entry number for 720*240*2
	// ChannelOffset = 128;   ///85;

	fh = q->priv_data;
	nId = fh ->DMA_nCH;   // DMA channel

	dwReg2 =  reg_readl(DMA_CH0_CONFIG+ 2);
	dwReg =  reg_readl(DMA_CH0_CONFIG+ nId);

	pr_debug("%s: CH%d::   dwReg2: 0x%X   deReg 0x%X  \n", __func__, nId, dwReg2, dwReg );


	if (nId == 0XF) {
		buffer_setup_QF(q, count, size);
		return 0;
	}

	*size = fh->fmt->depth * fh->width * fh->height >> 3;    // calculate byte size for 1 frame


	if (nId < 4) {
		dwReg =  reg_readl(DECODER0_SDT+ (nId*0x10));
		//pr_debug(" ####%%%%%%%%%%%%%%%%%%%%%%% buffer_setup::  DECODER0_SDT %d,  0X%X \n", nId, dwReg);
		reg_writel(DECODER0_SDT+ (nId*0x10), 7);		/// 0 NTSC
	} else {
		//pr_debug(" ####%%%%%%%%%%%%%%%%%%%%%%% buffer_setup::  DECODER0_SDT %d,  0X%X \n", nId, dwReg);
	}

////////////////////////////// decoder resize //////////////////////////////////////////


	DecoderResize(dev, nId, fh->height/2, fh->width);

	/// Fixed_SG_Mapping(dev, nId, *size);  //   nDMA_channel

	BFDMA_setup(dev, nId, (fh->height /2), (*size /fh->height));   // BFbuf setup  DMA mode ...

	dwReg2 =  reg_readl(DMA_CH0_CONFIG+ 2);
	dwReg =  reg_readl(DMA_CH0_CONFIG+ nId);

	pr_debug("%s: CH%d::   dwReg2: 0x%X   deReg 0x%X  H:%d W:%d\n",
		__func__, nId, dwReg2, dwReg, fh->height /2, (*size /fh->height) );


	if (0 == *count)
		*count = gbuffers;

	//pgn = (*size + PAGE_SIZE -1) /PAGE_SIZE;
	pgn = TW68_buffer_pages(*size /2) -1;  // page number for 1 field

	//pgn = (pages+1) /2;


	//*count = TW68_buffer_count(*size,*count);
	while (*size * *count > VideoFrames_limit * 1024 * 1024 *2 )
		(*count)--;

	m_nDropChannelNum = 0;
	m_bDropMasterOrSlave = 1;   // master
	m_bDropField = 0;      ////////////////////// 1
	m_bDropOddOrEven = 0;
	m_bHorizontalDecimate =0;
	m_bVerticalDecimate = 0;

	m_StartIdx = ChannelOffset * nId;
	m_EndIdx = m_StartIdx + pgn;    ///pgn;  85 :: 720 * 480
	m_nCurVideoChannelNum = 0;  // real-time video channel  starts 0
	m_nVideoFormat = dev -> nVideoFormat[nId];

	pr_debug("%s: W:%d H:%d  frame size: %d,  gbuffers: %d, ChannelOffset: %d  field pgn: %d  m_StartIdx %d  m_EndIdx %d \n",
		__func__, fh->width, fh->height, *size,  *count, ChannelOffset, pgn, m_StartIdx, m_EndIdx );


	m_dwCHConfig = (m_StartIdx & 0x3FF)		|	// 10 bits
			((m_EndIdx&0x3FF)<<10)		|	// 10 bits
			((m_nVideoFormat&7)<<20)	|
			((m_bHorizontalDecimate&1)<<23) |
			((m_bVerticalDecimate&1)<<24)	|
			((m_nDropChannelNum&3)<<25)	|
			((m_bDropMasterOrSlave&1)<<27)	|	// 1 bit
			((m_bDropField&1)<<28)		|
			((m_bDropOddOrEven&1)<<29)	|
			((m_nCurVideoChannelNum&3)<<30);

	reg_writel(DMA_CH0_CONFIG+ nId, m_dwCHConfig);
	dwReg =  reg_readl(DMA_CH0_CONFIG+ nId);
	pr_debug("%s: CH%d::  m_StartIdx 0X%x  pgn%d  m_dwCHConfig: 0x%X   dwReg: 0x%X    \n",
		__func__, nId, m_StartIdx, pgn,  m_dwCHConfig, dwReg );


//////external video decoder settings//////////////////////////////////////////////////////////////////////////

	dwRegW = fh->width;
	dwRegH = fh->height /2;  // frame height

	dwReg = dwRegW | (dwRegH<<16) | (1<<31);
	dwRegW = dwRegH = dwReg;

	//Video Size
	reg_writel(VIDEO_SIZE_REG,  dwReg);	    //for Rev.A backward compatible
	 ///  xxx dwReg = reg_readl(VIDEO_SIZE_REG);

	pr_debug("%s: VIDEO_SIZE_REG: 0x%X,   0x%X \n", __func__, VIDEO_SIZE_REG, dwReg);

	reg_writel(VIDEO_SIZE_REG0+nId,  dwReg);  //for Rev.B or later only

	//Scaler
	dwRegW &= 0x7FF;
	dwRegW = (720*256)/dwRegW;
	dwRegH = (dwRegH>>16)&0x1FF;
	// 60HZ video
	dwRegH = (240*256)/dwRegH;

// 0915  rev B  black ....
	nScaler = VSCALE1_LO ;  /// + (nId<<4); //VSCALE1_LO + 0|0x10|0x20|0x30

	dwReg = dwRegH & 0xFF;  //V
	//if(nId >= 4) DeviceWrite2864(nAddr,tmp);

	// reg_writel(nScaler,  dwReg);

	nScaler++;  //VH

	dwReg = (((dwRegH >> 8)& 0xF) << 4) | ((dwRegW>>8) & 0xF );
	//if(nId >= 4) DeviceWrite2864(nAddr,tmp);

	// reg_writel(nScaler,  dwReg);

	nScaler++;  //H
	dwReg = dwRegW & 0xFF;
	///if(nId >= 4) DeviceWrite2864(nAddr,tmp);

	/// reg_writel(nScaler,  dwReg);

	//setup for Black stripe remover
	dwRegW = fh->width; /// -12;  //EndPos
	dwRegH = 4;		   //StartPos
	dwReg = (dwRegW - dwRegH)*(1<<16)/ fh->width;
	dwReg = (dwRegH & 0x1F) |
		  ((dwRegH & 0x3FF) << 5) |
		  (dwReg <<15);

	reg_writel(DROP_FIELD_REG0+ nId,  0xBFFFFFFF);   //28 // B 30 FPS

	//reg_writel(DROP_FIELD_REG0+ nId,  0xBFFFCFFF);    // 28  // 26 FPS   last xx FC
	//reg_writel(DROP_FIELD_REG0+ nId,  0x8FFCFCFF);    // 28  // 26 FPS   last xx FC
	//reg_writel(DROP_FIELD_REG0+ nId,  0xBF3F3F3F);   // 24 FPS
	//reg_writel(DROP_FIELD_REG0+ nId,  0x8FCFCFCF);   // 24 FPS

	dwReg2 =  reg_readl(DMA_CH0_CONFIG+ 2);
	dwReg =  reg_readl(DMA_CH0_CONFIG+ nId);

	pr_debug("%s: CH%d:: dwReg2: 0x%X   deReg 0x%X  \n", __func__, nId, dwReg2, dwReg );
	return 0;
}


static void buffer_queue(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	struct TW68_fh *fh = q->priv_data;
	struct TW68_buf *buf = container_of(vb,struct TW68_buf,vb);
	int nId = fh->DMA_nCH;

	//TW68_buffer_queue(fh->dev,&fh->dev->video_q,buf);
	pr_debug("%s: for video DMA nId %x \n", __func__, nId);

	if (nId == 0XF)
		nId = 0;
	else
		nId++;

	TW68_buffer_queue(fh->dev,&fh->dev->video_dmaq[nId], buf);

}


static void buffer_release(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	struct TW68_buf *buf = container_of(vb,struct TW68_buf,vb);

	pr_debug("%s\n", __func__);
	free_buffer(q, buf);

}


static struct videobuf_queue_ops video_qops = {
	.buf_setup    = buffer_setup,
	.buf_prepare  = buffer_prepare,
	.buf_queue    = buffer_queue,
	.buf_release  = buffer_release,
};

/* ------------------------------------------------------------------ */


int TW68_g_ctrl_internal(struct TW68_dev *dev, struct TW68_fh *fh, struct v4l2_control *c)
{
	const struct v4l2_queryctrl* ctrl;
	int DMA_nCH = fh->DMA_nCH;
	int nId = (DMA_nCH +1)& 0xF;
	int regval =0;

	ctrl = ctrl_by_id(c->id);
	if (NULL == ctrl)
		return -EINVAL;


	switch (c->id) {
	case V4L2_CID_BRIGHTNESS:
		if (DMA_nCH == 0xF)  DMA_nCH = 0;
		if (DMA_nCH<4) {
			regval = reg_readl(CH1_BRIGHTNESS_REG + DMA_nCH *0x10 );
			regval = (regval + 0x80) &0xFF;
		} else {
			regval = reg_readl(CH1_BRIGHTNESS_REG + 0x100 + (DMA_nCH -4) *0x10 );
			regval = (regval + 0x80) &0xFF;
		}
		c->value = dev->video_param[nId].ctl_bright = regval;
		break;
	case V4L2_CID_HUE:  ///-128 +127
		if (DMA_nCH == 0xF)  DMA_nCH = 0;
		if (DMA_nCH<4) {
			regval = reg_readl(CH1_HUE_REG + DMA_nCH *0x10 );
		} else {
			regval = reg_readl(CH1_HUE_REG + 0x100 + (DMA_nCH -4) *0x10 );
		}
		if (regval < 0x80)
			c->value = dev->video_param[nId].ctl_hue = regval;
		else
			c->value = dev->video_param[nId].ctl_hue = (regval -0x100);
		break;
	case V4L2_CID_CONTRAST:
		if (DMA_nCH == 0xF)  DMA_nCH = 0;
		if (DMA_nCH<4) {
			regval = reg_readl(CH1_CONTRAST_REG + DMA_nCH *0x10 );
		} else {
			regval = reg_readl(CH1_CONTRAST_REG + 0x100 + (DMA_nCH -4) *0x10 );
		}
		c->value = dev->video_param[nId].ctl_contrast = regval;
		break;
	case V4L2_CID_SATURATION:
		if (DMA_nCH == 0xF)  DMA_nCH = 0;
		if (DMA_nCH<4) {
			regval = reg_readl(CH1_SAT_U_REG + DMA_nCH *0x10 );
		} else {
			regval = reg_readl(CH1_SAT_U_REG + 0x100 + (DMA_nCH -4) *0x10 );
		}
		c->value = dev->video_param[nId].ctl_saturation = regval /2;
		break;
	case V4L2_CID_AUDIO_MUTE:
		c->value = dev->video_param[nId].ctl_mute;
		break;
	case V4L2_CID_AUDIO_VOLUME:
		c->value = dev->video_param[nId].ctl_volume;
		break;
	/*
	case V4L2_CID_PRIVATE_INVERT:
		c->value = dev->video_param[k].ctl_invert;
		break;
	case V4L2_CID_HFLIP:
		c->value = dev->ctl_mirror;
		break;
	*/
	case V4L2_CID_PRIVATE_Y_EVEN:
		c->value = dev->video_param[nId].ctl_y_even;
		break;
	case V4L2_CID_PRIVATE_Y_ODD:
		c->value = dev->video_param[nId].ctl_y_odd;
		break;
	case V4L2_CID_PRIVATE_AUTOMUTE:
		c->value = dev->video_param[nId].ctl_automute;
		break;
	default:
		return -EINVAL;
	}
	pr_debug("%s:nId%d  TW68_g_ctrl_internal  Get_control name=%s val=%d  regval 0x%X \n",
		__func__, nId, ctrl->name,c->value, regval);
	return 0;
}


///EXPORT_SYMBOL_GPL(TW68_g_ctrl_internal);

static int TW68_g_ctrl(struct file *file, void *priv, struct v4l2_control *c)
{
	struct TW68_fh *fh = priv;

	return TW68_g_ctrl_internal(fh->dev, fh, c);
}

int TW68_s_ctrl_internal(struct TW68_dev *dev,  struct TW68_fh *fh, struct v4l2_control *c)
{
	const struct v4l2_queryctrl* ctrl;
	int restart_overlay = 0;
	int DMA_nCH, nId;
	int err = 0;
	int regval =0;;
	DMA_nCH = fh->DMA_nCH;
	nId = (DMA_nCH +1)& 0xF;


	ctrl = ctrl_by_id(c->id);
	if (NULL == ctrl)
		goto error;

	pr_debug("%s:set_control name=%s val=%d\n",
		__func__, ctrl->name,c->value);

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
		dev->video_param[nId].ctl_bright = c->value;
		regval = ((c->value - 0x80))  &0xFF;
		if (DMA_nCH<4) {
			reg_writel(CH1_BRIGHTNESS_REG + DMA_nCH *0x10, regval );
		} else {
			if (DMA_nCH<8) {
				reg_writel(CH1_BRIGHTNESS_REG + 0x100 +(DMA_nCH -4) *0x10, regval);
			} else if (DMA_nCH == 0xF) {
				reg_writel(CH1_BRIGHTNESS_REG, regval);
				reg_writel(CH2_BRIGHTNESS_REG, regval);
				reg_writel(CH3_BRIGHTNESS_REG, regval);
				reg_writel(CH4_BRIGHTNESS_REG, regval);
			}
		}
		break;
	case V4L2_CID_CONTRAST:
		dev->video_param[nId].ctl_contrast = c->value;
		if (DMA_nCH<4) {
			reg_writel(CH1_CONTRAST_REG + DMA_nCH *0x10, c->value);
		} else {
			if (DMA_nCH<8) {
				reg_writel(CH1_CONTRAST_REG + 0x100 +(DMA_nCH -4) *0x10, c->value);
			} else if (DMA_nCH == 0xF) {
				reg_writel(CH1_CONTRAST_REG, c->value);
				reg_writel(CH2_CONTRAST_REG, c->value);
				reg_writel(CH3_CONTRAST_REG, c->value);
				reg_writel(CH4_CONTRAST_REG, c->value);
			}
		}
		break;

	case V4L2_CID_HUE:
		dev->video_param[nId].ctl_hue = c->value;
		regval = c->value; //  &0xFF;
		if (DMA_nCH<4) {
			reg_writel(CH1_HUE_REG + DMA_nCH *0x10, regval);
		} else {
			if (DMA_nCH<8) {
				reg_writel(CH1_HUE_REG + 0x100 +(DMA_nCH -4) *0x10, regval);
			} else if (DMA_nCH == 0xF) {
				reg_writel(CH1_HUE_REG, regval);
				reg_writel(CH2_HUE_REG, regval);
				reg_writel(CH3_HUE_REG, regval);
				reg_writel(CH4_HUE_REG, regval);
			}
		}
		break;

	case V4L2_CID_SATURATION:
		dev->video_param[nId].ctl_saturation = c->value;
		regval = c->value *2;
		if (DMA_nCH<4) {
			reg_writel(CH1_SAT_U_REG + DMA_nCH *0x10, regval);
			reg_writel(CH1_SAT_V_REG + DMA_nCH *0x10, regval);
		} else {
			if (DMA_nCH<8) {
				reg_writel(CH1_SAT_U_REG + 0x100 + (DMA_nCH -4)*0x10, regval);
				reg_writel(CH1_SAT_V_REG + 0x100 + (DMA_nCH -4)*0x10, regval);
			} else if (DMA_nCH == 0xF) { // write wrong addr  hang
				reg_writel(CH1_SAT_U_REG, regval);
				reg_writel(CH1_SAT_V_REG, regval);
				reg_writel(CH2_SAT_U_REG, regval);
				reg_writel(CH2_SAT_V_REG, regval);
				reg_writel(CH3_SAT_U_REG, regval);
				reg_writel(CH3_SAT_V_REG, regval);
				reg_writel(CH4_SAT_U_REG, regval);
				reg_writel(CH4_SAT_V_REG, regval);
			}
		}
		break;
	case V4L2_CID_AUDIO_MUTE:
		dev->video_param[nId].ctl_mute = c->value;
		//TW68__setmute(dev);
		break;
	case V4L2_CID_AUDIO_VOLUME:
		dev->video_param[nId].ctl_volume = c->value;
		//TW68_tvaudio_setvolume(dev,dev->ctl_volume);
		break;

	case V4L2_CID_PRIVATE_Y_EVEN:
		dev->video_param[nId].ctl_y_even = c->value;
		restart_overlay = 1;
		break;
	case V4L2_CID_PRIVATE_Y_ODD:
		dev->video_param[nId].ctl_y_odd = c->value;
		restart_overlay = 1;
		break;
	case V4L2_CID_PRIVATE_AUTOMUTE:
	{
		dev->video_param[nId].ctl_automute = c->value;
		break;
	}
	default:
		err = -ENOTSUPP;
		goto error;
	}
	pr_debug("%s: set_control name=%s REAL val=%d   reg  0x%X\n",
		__func__, ctrl->name,c->value,  regval);

error:
	return err;
}


///EXPORT_SYMBOL_GPL(TW68_s_ctrl_internal);

static int TW68_s_ctrl(struct file *file, void *f, struct v4l2_control *c)
{
	struct TW68_fh *fh = f;

	return TW68_s_ctrl_internal(fh->dev, fh, c);
}

/* ------------------------------------------------------------------ */

static struct videobuf_queue* TW68_queue(struct TW68_fh *fh)
{
	struct videobuf_queue* q = NULL;

	switch (fh->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		q = &fh->cap;
		//pr_debug("%s: V4L2_BUF_TYPE_VIDEO_CAPTURE 0x%X\n", __func__, q);
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		q = &fh->vbi;
		//pr_debug("%s: V4L2_BUF_TYPE_VBI_CAPTURE .\n", __func__);
		break;
	default:
		pr_err("%s: not valid type,  break out\n", __func__);
		BUG();
	}
	return q;
}

////////////////////////////////////////////////////////////////////////////
static int TW68_resource(struct TW68_fh *fh)
{
	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return RESOURCE_VIDEO;

	if (fh->type == V4L2_BUF_TYPE_VBI_CAPTURE)
		return RESOURCE_VBI;

	BUG();
	return 0;
}
////////////////////////////////////////////////////////////////////////////

static int video_open(struct file *file)
{
	int minor = video_devdata(file)->minor;
	struct TW68_dev *dev;
	struct TW68_fh *fh;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	unsigned int request =0;
	unsigned int dmaCH;

	int k;  //, used;

	// return -ENODEV;
	mutex_lock(&TW686v_devlist_lock);

	list_for_each_entry(dev, &TW686v_devlist, devlist) {
		pr_debug("%s: minor=%d  type=%s  opened:%x\n",
			__func__, minor, v4l2_type_names[type], dev->video_opened);

		//if (dev->video_dev && (dev->video_dev->minor == minor))v->video_open
		//if (dev->video_device[0] && (dev->video_device[0]->minor == minor))

		for (k=0; k < ARRAY_SIZE(dev->video_device); k++) {
			struct video_device *vdev = dev->video_device[k];

			if (vdev) {
				pr_debug("%s: search  k=%d vdev=%p, vdev->minor:%d\n",
						__func__, k, vdev,  vdev->minor);

				if (vdev->minor == minor)
					goto found;
			}
		}
	}

	//if (dev->vbi_dev && (dev->vbi_dev->minor == minor)) {
	//	type = V4L2_BUF_TYPE_VBI_CAPTURE;
	//	goto found;
	//}

	pr_debug("%s: no real device found XXXX \n", __func__);

	mutex_unlock(&TW686v_devlist_lock);
	return -ENODEV;

found:
	mutex_unlock(&TW686v_devlist_lock);
	if (k ==0)   // QF output
		request = 0x0F;  // internal 4 decoders   mux QF
	else
		request = 1 << (k-1);

	// check video decoder video standard and change default tvnormf
	dmaCH =0xF;
	if (k>0) dmaCH = k-1;

	pr_debug("%s: ID:%d  dmaCH %x   request %X \n", __func__, k, dmaCH, request);


	if (dev->video_opened & request) {
		mutex_unlock(&TW686v_devlist_lock);
		pr_debug("%s: EBUSY    dev->video_opened %x  request %x \n",
				__func__, dev->video_opened, request);
		return -EBUSY;
	}

	dev->video_opened = dev->video_opened |request;

	if (k)
		dev->video_dmaq[k].DMA_nCH = k-1;
	else
		dev->video_dmaq[k].DMA_nCH = 0x0F;   // 0X0F;

	/* allocate + initialize per filehandle data */
	fh = kzalloc(sizeof(*fh),GFP_KERNEL);
	if (NULL == fh)
		return -ENOMEM;

	pr_debug("%s: kzalloc  successful!\n", __func__);

	if (VideoDecoderDetect(dev, dmaCH) == 50) {
		dev->tvnormf[k] = &tvnorms[0];
		dev->PAL50[k] = 1;
		fh->dW = PAL_default_width;
		fh->dH = PAL_default_height;
	} else {
		dev->tvnormf[k] = &tvnorms[4];
		dev->PAL50[k] = 0;
		fh->dW = NTSC_default_width;
		fh->dH = NTSC_default_height;
	}

	pr_debug("%s: 0X%p  open /dev/video%d  minor%d type=%s  video_opened=0x%X  k:%d   tvnorm::%s  %d\n",
		__func__, dev, dev->video_device[k]->num,  minor, //  0901 array
		v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->video_opened, k, dev->tvnormf[k]->name, dev->PAL50[k]);

	file->private_data = fh;
	fh->dev      = dev;
	fh->DMA_nCH    = dev->video_dmaq[k].DMA_nCH;  ///  k;    /// DMA index   +1
	fh->type     = type;
	/// fh->fmt      = format_by_fourcc(V4L2_PIX_FMT_UYVY);   /// RGB24
	fh->fmt      = format_by_fourcc(V4L2_PIX_FMT_YUYV);   /// YUY2 by default
	fh->width    = fh->dW;  //704;  //720;
	fh->height   = fh->dH;  //576;

	pr_debug("%s: minor=%d  fh->DMA_nCH= 0x%X type=%s\n",
		__func__, minor, fh->DMA_nCH, v4l2_type_names[type]);

	v4l2_prio_open(&dev->prio,&fh->prio);
	pr_debug("%s: v4l2_prio_open() successful!\n", __func__);

//6.1 add mutex *
	videobuf_queue_vmalloc_init(&fh->cap, &video_qops,
		NULL, &dev->slock,
		V4L2_BUF_TYPE_VIDEO_CAPTURE,
		V4L2_FIELD_INTERLACED,
		sizeof(struct TW68_buf),
		fh
		//);   //6.0
		, NULL);
	// 6.1

	//video_mux(dev,dev->ctl_input);
	return 0;
}

static ssize_t video_read(struct file *file, char __user *data, size_t count,
		loff_t *ppos)
{
	struct TW68_fh *fh = file->private_data;

	switch (fh->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (res_locked(fh, fh->dev,RESOURCE_VIDEO))
			return -EBUSY;
		return videobuf_read_one(TW68_queue(fh),
					 data, count, ppos,
					 file->f_flags & O_NONBLOCK);
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		if (!res_get(fh->dev,fh,RESOURCE_VBI))
			return -EBUSY;
		return videobuf_read_stream(TW68_queue(fh),
					    data, count, ppos, 1,
					    file->f_flags & O_NONBLOCK);
		break;
	default:
		BUG();
		return 0;
	}
}

static unsigned int
video_poll(struct file *file, struct poll_table_struct *wait)
{
	struct TW68_fh *fh = file->private_data;
	struct videobuf_buffer *buf = NULL;
	unsigned int rc = 0;

	//pr_debug("%s\n", __func__);
	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	///return videobuf_poll_stream(file, q, wait);

	if (V4L2_BUF_TYPE_VBI_CAPTURE == fh->type)
		return videobuf_poll_stream(file, &fh->vbi, wait);

	if (res_check(fh,RESOURCE_VIDEO)) {
		//mutex_lock(&fh->cap.vb_lock);
		if (!list_empty(&fh->cap.stream))
			buf = list_entry(fh->cap.stream.next, struct videobuf_buffer, stream);
	} else {
		mutex_lock(&fh->cap.vb_lock);
		if (UNSET == fh->cap.read_off) {
			/* need to capture a new frame */
			if (res_locked(fh, fh->dev,RESOURCE_VIDEO))
				goto err;
			if (0 != fh->cap.ops->buf_prepare(&fh->cap,fh->cap.read_buf,fh->cap.field))
				goto err;
			fh->cap.ops->buf_queue(&fh->cap,fh->cap.read_buf);
			fh->cap.read_off = 0;
		}
		mutex_unlock(&fh->cap.vb_lock);
		buf = fh->cap.read_buf;
	}

	if (!buf)
		goto err;

	poll_wait(file, &buf->done, wait);
	if (buf->state == VIDEOBUF_DONE ||
	    buf->state == VIDEOBUF_ERROR)
		rc = POLLIN|POLLRDNORM;
	mutex_unlock(&fh->cap.vb_lock);
	return rc;

err:
	mutex_unlock(&fh->cap.vb_lock);
	return POLLERR;
}


static int video_release(struct file *file)
{
	int minor = video_devdata(file)->minor;

	struct TW68_fh  *fh  = file->private_data;
	struct TW68_dev *dev = fh->dev;
	int DMA_nCH = fh->DMA_nCH;
	int nId = DMA_nCH +1;

	pr_debug("%s: minor:%d  DMA_nCH: %X    video_fieldcount 0x%X ! \n",
		__func__, minor, DMA_nCH, dev->video_fieldcount[DMA_nCH]);    /// 0-8


	if  (DMA_nCH ==0x0F) {
		dev->video_opened &= ~(DMA_nCH );
		dev->video_dmaq[0].DMA_nCH = 0;
		dev->video_fieldcount[0] =0;
		stop_video_DMA(dev, 0);
		stop_video_DMA(dev, 1);
		stop_video_DMA(dev, 2);
		stop_video_DMA(dev, 3);

		dev->video_fieldcount[0] =0;
		stop_video_DMA(dev, 0);
		del_timer(&dev->video_dmaq[0].timeout);

	} else {
		dev->video_opened &= ~(1 << DMA_nCH );   /// set opened flag free
		stop_video_DMA(dev, DMA_nCH );				//  fh->DMA_nCH  = DMA ID
		dev->video_dmaq[nId].DMA_nCH = 0;
		dev->video_fieldcount[nId] =0;
		del_timer(&dev->video_dmaq[nId].timeout);

	}

	pr_debug("%s: dev->video_opened: 0x%x \n", __func__, dev->video_opened);
	synchronize_irq(dev->pci->irq);
	videobuf_streamoff(&fh->cap);

	//pr_debug(" stop video capture  CALLED   ! \n");

	if (fh->cap.read_buf) {
		buffer_release(&fh->cap,fh->cap.read_buf);
		kfree(fh->cap.read_buf);
	}

	//v4l2_prio_close(&dev->prio,  (enum v4l2_priority *)(&fh->prio));
	file->private_data = NULL;
	kfree(fh);
	return 0;
}

static int video_mmap(struct file *file, struct vm_area_struct * vma)
{
	struct TW68_fh *fh = file->private_data;

	pr_debug("%s: vma= %lx  %lx  %lx \n", __func__, vma->vm_start,
			vma->vm_end, (vma->vm_end - vma->vm_start));
	return videobuf_mmap_mapper(TW68_queue(fh), vma);
}

/* ------------------------------------------------------------------ */

static int TW68_try_get_set_fmt_vbi_cap(struct file *file, void *priv,
						struct v4l2_format *f)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;
	struct TW68_tvnorm *norm = dev->tvnorm;

	f->fmt.vbi.sampling_rate = 6750000 * 4;
	f->fmt.vbi.samples_per_line = 2048 /* VBI_LINE_LENGTH */;
	f->fmt.vbi.sample_format = V4L2_PIX_FMT_GREY;
	f->fmt.vbi.offset = 64 * 4;
	f->fmt.vbi.start[0] = norm->vbi_v_start_0;
	f->fmt.vbi.count[0] = norm->vbi_v_stop_0 - norm->vbi_v_start_0 +1;
	f->fmt.vbi.start[1] = norm->vbi_v_start_1;
	f->fmt.vbi.count[1] = f->fmt.vbi.count[0];
	f->fmt.vbi.flags = 0; /* VBI_UNSYNC VBI_INTERLACED */

	return 0;
}

static int TW68_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct TW68_fh *fh = priv;

	pr_debug("%s: fh(%p) v4l2_format(%p)\n", __func__, priv, f);

	f->fmt.pix.width        = fh->width;
	f->fmt.pix.height       = fh->height;
	f->fmt.pix.field        = fh->cap.field;
	f->fmt.pix.pixelformat  = fh->fmt->fourcc;
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	pr_debug("%s: width %d  height %d \n", __func__, f->fmt.pix.width, f->fmt.pix.height);
	return 0;
}

static int TW68_g_fmt_vid_overlay(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct TW68_fh *fh = priv;

	TW68_g_fmt_vid_cap(file, priv, f);
	return 0;


	/*
	if (TW68_no_overlay > 0) {
		pr_err("V4L2_BUF_TYPE_VIDEO_OVERLAY: no_overlay\n");
		return -EINVAL;
	}
	f->fmt.win = fh->win;
	*/

	f->fmt.pix.width        = fh->width;
	f->fmt.pix.height       = fh->height;
	f->fmt.pix.field        = fh->cap.field;
	f->fmt.pix.pixelformat  = fh->fmt->fourcc;
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	pr_debug("%s: width %d  height %d \n", __func__, f->fmt.pix.width, f->fmt.pix.height);
	return 0;
}

static int TW68_try_fmt_vid_cap(struct file *file, void *priv,
						struct v4l2_format *f)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;
	struct TW68_format *fmt;
	enum v4l2_field field;
	unsigned int maxw, maxh;
	u32     k;
	u32	nId = fh ->DMA_nCH;

	fmt = format_by_fourcc(f->fmt.pix.pixelformat);
	pr_debug("TW68 input  nId:%x   try_fmt:: %x  | width %d  height %d\n",
			nId, f->fmt.pix.pixelformat, f->fmt.pix.width, f->fmt.pix.height  );

	if (NULL == fmt ) {
		pr_debug("TW68 fmt:: no valid pixel format \n");
		return -EINVAL;
	}


	if ((V4L2_PIX_FMT_YUYV) != fmt->fourcc) {
		if ((V4L2_PIX_FMT_UYVY) != fmt->fourcc) { /// Only allow UYVY  0727))
			pr_debug("TW68 fmt:: not YUV422 !!!!!!!!!!!!!!!!!!! \n");
			return -EINVAL;
		} else {
			if (nId<8) {
				dev -> nVideoFormat[nId] = VIDEO_FORMAT_UYVY;
			} else {
				dev -> nVideoFormat[0] = VIDEO_FORMAT_UYVY;
				dev -> nVideoFormat[1] = VIDEO_FORMAT_UYVY;
				dev -> nVideoFormat[2] = VIDEO_FORMAT_UYVY;
				dev -> nVideoFormat[3] = VIDEO_FORMAT_UYVY;
			}
		}
	} else {
		if (nId<8) {
			dev -> nVideoFormat[nId] =  VIDEO_FORMAT_YUYV;
		} else {
			dev -> nVideoFormat[0] = VIDEO_FORMAT_YUYV;
			dev -> nVideoFormat[1] = VIDEO_FORMAT_YUYV;
			dev -> nVideoFormat[2] = VIDEO_FORMAT_YUYV;
			dev -> nVideoFormat[3] = VIDEO_FORMAT_YUYV;
		}
	}

	if (nId > 8)
		k =0;
	else
		k = nId+1;

	if (dev->tvnormf[k]->id & V4L2_STD_525_60) {
		maxw  = 720;
		maxh  = 480;
	} else {
		maxw  = 720;
		maxh  = 576;
	}

	pr_debug("%s: tvnormf %d ->name %s  id %X   maxh %d \n", __func__, nId,
		dev->tvnormf[nId]->name, (unsigned int)dev->tvnormf[nId]->id, maxh);

	field = f->fmt.pix.field;

	if (V4L2_FIELD_ANY == field) {
		field = (f->fmt.pix.height > maxh/2)
			? V4L2_FIELD_INTERLACED
			: V4L2_FIELD_BOTTOM;
	}
	switch (field) {
	case V4L2_FIELD_TOP:
	case V4L2_FIELD_BOTTOM:
	case V4L2_FIELD_ALTERNATE:
		maxh = maxh / 2;
		break;
	case V4L2_FIELD_INTERLACED:
		break;
	default:
		return -EINVAL;
	}

	f->fmt.pix.field = field;

	pr_debug("%s: pixelformat %x  field: %d _fmt: width %d  height %d \n",
		__func__, fmt->fourcc, field, f->fmt.pix.width, f->fmt.pix.height);

	v4l_bound_align_image(&f->fmt.pix.width, 128, maxw, 2,    // 4 pixel  test 360  4,
			      &f->fmt.pix.height, 60, maxh, 0, 0);

	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	pr_debug("%s: width %d  height %d      %d  %d \n", __func__, f->fmt.pix.width, f->fmt.pix.height, maxw, maxh);
	return 0;
}


static int TW68_try_fmt_vid_overlay(struct file *file, void *priv,
						struct v4l2_format *f)
{
	//struct TW68_fh *fh = priv;
	//struct TW68_dev *dev = fh->dev;

	TW68_try_fmt_vid_cap(file, priv, f);
	return 0;

	if (TW68_no_overlay > 0) {
		pr_err("V4L2_BUF_TYPE_VIDEO_OVERLAY: no_overlay\n");
		return -EINVAL;
	}
	return 0; //verify_preview(dev, &f->fmt.win);
}

static int TW68_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct TW68_fh *fh = priv;
	int err;

	pr_debug("%s: %x  W:%d H:%d  field:%X\n", __func__, f->fmt.pix.pixelformat, fh->width, fh->height, fh->cap.field);

	err = TW68_try_fmt_vid_cap(file, priv, f);
	if (0 != err)
		return err;

	fh->fmt       = format_by_fourcc(f->fmt.pix.pixelformat);
	fh->width     = f->fmt.pix.width;
	fh->height    = f->fmt.pix.height;
	fh->cap.field = f->fmt.pix.field;

	pr_debug("%s: fh->fmt   W:%d H:%d  field:%X\n", __func__, fh->width, fh->height, fh->cap.field);
	return 0;
}


static int TW68_s_fmt_vid_overlay(struct file *file, void *priv,
					struct v4l2_format *f)
{
	pr_debug("%s\n", __func__);
	TW68_try_fmt_vid_cap(file, priv, f);
	return 0;
}

int TW68_queryctrl(struct file *file, void *priv, struct v4l2_queryctrl *c)
{
	const struct v4l2_queryctrl *ctrl;

	if ((c->id <  V4L2_CID_BASE ||
	     c->id >= V4L2_CID_LASTP1) &&
	    (c->id <  V4L2_CID_PRIVATE_BASE ||
	     c->id >= V4L2_CID_PRIVATE_LASTP1))
		return -EINVAL;
	ctrl = ctrl_by_id(c->id);
	*c = (NULL != ctrl) ? *ctrl : no_ctrl;
	return 0;
}
///EXPORT_SYMBOL_GPL(TW68_queryctrl);

static int TW68_enum_input(struct file *file, void *priv,
					struct v4l2_input *i)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;
	unsigned int n;

	n = i->index;
	pr_debug("%s: i=%p  n:%x\n", __func__, i, n);

	if (n >= 4)
		return -EINVAL;
	if (NULL == card_in(dev, i->index).name)
		return -EINVAL;
	memset(i, 0, sizeof(*i));
	i->index = n;
	i->type  = V4L2_INPUT_TYPE_CAMERA;
	strcpy(i->name, card_in(dev, n).name);
	/*
	if (card_in(dev, n).tv)
		i->type = V4L2_INPUT_TYPE_TUNER;
	i->audioset = 1;
	*/
	if (n == dev->ctl_input) {
	}
	i->std = TW68_NORMS;

	i->type  = V4L2_INPUT_TYPE_CAMERA;
	sprintf(i->name, "Composite%d", n);

	/*
	///if(n == dev->input) {
	if (n == dev->ctl_input) {
		vstatus = dev->vstatus;
		if (0 != (vstatus & BIT(7)))
		    i->status |= V4L2_IN_ST_NO_SIGNAL;
		if (0 == (vstatus & BIT(6)))
		    i->status |= V4L2_IN_ST_NO_H_LOCK;
		if (0 != (vstatus & BIT(1)))
		    i->status |= V4L2_IN_ST_NO_COLOR;
	}
	*/
	return 0;
}

static int TW68_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;

	*i = dev->ctl_input;
	return 0;
}

static int TW68_s_input(struct file *file, void *priv, unsigned int i)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;

	pr_debug("%s: i=%d\n", __func__, i);

	if (i < 0  ||  i >= TW68_INPUT_MAX)
		return -EINVAL;
	if (NULL == card_in(dev, i).name)
		return -EINVAL;

	pr_debug("%s: card in name: %s  i=%d\n",
			__func__, card_in(dev, i).name, i);
	return 0;
}

static int TW68_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;

	// unsigned int tuner_type = dev->tuner_type;

	pr_debug("%s\n", __func__);

	strcpy(cap->driver, "TW--6868");
	strlcpy(cap->card, TW68_boards[dev->board].name,
		sizeof(cap->card));
	sprintf(cap->bus_info, "PCI:%s", pci_name(dev->pci));
	cap->version = TW68_VERSION_CODE;
	cap->capabilities =
		V4L2_CAP_VIDEO_CAPTURE |
		V4L2_CAP_VBI_CAPTURE |
		V4L2_CAP_READWRITE |
		V4L2_CAP_STREAMING |
		V4L2_CAP_TUNER;
	if (TW68_no_overlay <= 0)
		cap->capabilities |= V4L2_CAP_VIDEO_OVERLAY;

	pr_debug("%s exit\n", __func__);
	return 0;
}

int TW68_s_std_internal(struct TW68_dev *dev, struct TW68_fh *fh, v4l2_std_id id)
{
	unsigned int i, nId;
	v4l2_std_id fixup;

	for (i = 0; i < TVNORMS; i++)
		if (id == tvnorms[i].id)
			break;

	if (i == TVNORMS)
		for (i = 0; i < TVNORMS; i++)
			if (id & tvnorms[i].id)
				break;
	if (i == TVNORMS)
		return -EINVAL;

	if ((id & V4L2_STD_SECAM) && (secam[0] != '-')) {
		if (secam[0] == 'L' || secam[0] == 'l') {
			if (secam[1] == 'C' || secam[1] == 'c')
				fixup = V4L2_STD_SECAM_LC;

			else
				fixup = V4L2_STD_SECAM_L;
		} else {
			if (secam[0] == 'D' || secam[0] == 'd')
				fixup = V4L2_STD_SECAM_DK;
			else
				fixup = V4L2_STD_SECAM;
		}
		for (i = 0; i < TVNORMS; i++)
			if (fixup == tvnorms[i].id)
				break;
	}

	id = tvnorms[i].id;

	pr_debug("%s: id=%x i=%x\n", __func__, (int)id, i);
	nId = fh->DMA_nCH;

	if (nId == 0XF)
		nId = 0;

	mutex_lock(&dev->lock);
		set_tvnorm(dev, &tvnorms[i]);
		dev ->tvnormf[nId] = &tvnorms[i];
	mutex_unlock(&dev->lock);
	return 0;
}


static int TW68_s_std(struct file *file, void *priv, v4l2_std_id id)
{
	struct TW68_fh *fh = priv;
	// struct TW68_dev *dev = fh->dev;
	int nId = fh->DMA_nCH;

	if (nId == 0XF)
		nId = 0;

	pr_debug("%s:id=%d\n", __func__, (int)id);
	return TW68_s_std_internal(fh->dev, fh, id);
}


static int TW68_g_std(struct file *file, void *priv, v4l2_std_id *id)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;
	int nId = fh->DMA_nCH;

	*id = dev->tvnorm->id;

	if (nId == 0XF)
		nId = 0;


	*id = dev->tvnormf[nId]->id;

	pr_debug("%s: id=%d\n", __func__, (int)*id);
	return 0;
}

static int TW68_cropcap(struct file *file, void *priv,
					struct v4l2_cropcap *cap)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;

	pr_debug("%s\n", __func__);

	if (cap->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    cap->type != V4L2_BUF_TYPE_VIDEO_OVERLAY)
		return -EINVAL;

	cap->bounds  = dev->crop_bounds;
	cap->defrect = dev->crop_defrect;
	cap->pixelaspect.numerator   = 1;
	cap->pixelaspect.denominator = 1;

	pr_debug("%s dev->tvnorm->id:0x%x\n", __func__, (unsigned)dev->tvnorm->id);
	if (dev->tvnorm->id & V4L2_STD_525_60) {
		cap->pixelaspect.numerator   = 11;
		cap->pixelaspect.denominator = 10;
	}
	if (dev->tvnorm->id & V4L2_STD_625_50) {
		cap->pixelaspect.numerator   = 54;
		cap->pixelaspect.denominator = 59;
	}

	pr_debug("%s success\n", __func__);
	return 0;
}

static int TW68_g_crop(struct file *file, void *f, struct v4l2_crop *crop)
{
	struct TW68_fh *fh = f;
	struct TW68_dev *dev = fh->dev;

	pr_debug("%s\n", __func__);

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    crop->type != V4L2_BUF_TYPE_VIDEO_OVERLAY)
		return -EINVAL;
	crop->c = dev->crop_current;
	return 0;
}

static int TW68_s_crop(struct file *file, void *f, const struct v4l2_crop *crop)
{
	struct TW68_fh *fh = f;
	struct TW68_dev *dev = fh->dev;
	struct v4l2_rect *b = &dev->crop_bounds;
	struct v4l2_rect c;

	pr_debug("%s\n", __func__);

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    crop->type != V4L2_BUF_TYPE_VIDEO_OVERLAY)
		return -EINVAL;
	if (crop->c.height < 0)
		return -EINVAL;
	if (crop->c.width < 0)
		return -EINVAL;

	if (res_locked(fh, fh->dev, RESOURCE_OVERLAY))
		return -EBUSY;
	if (res_locked(fh, fh->dev, RESOURCE_VIDEO))
		return -EBUSY;

	c = crop->c;
	if (c.top < b->top)
		c.top = b->top;
	if (c.top > b->top + b->height)
		c.top = b->top + b->height;
	if (c.height > b->top - crop->c.top + b->height)
		c.height = b->top - crop->c.top + b->height;

	if (c.left < b->left)
		c.left = b->left;
	if (c.left > b->left + b->width)
		c.left = b->left + b->width;
	if (c.width > b->left - crop->c.left + b->width)
		c.width = b->left - crop->c.left + b->width;

	dev->crop_current = c;
	return 0;
}



static int TW68_g_tuner(struct file *file, void *priv,
					struct v4l2_tuner *t)
{
	if (t->index != 0)
		return -EINVAL;
	return 0;
}

static int TW68_s_tuner(struct file *file, void *priv,
					const struct v4l2_tuner *t)
{
	return 0;
}

static int TW68_g_frequency(struct file *file, void *priv,
					struct v4l2_frequency *f)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;

	f->frequency = dev->ctl_freq;
	return 0;
}


static int TW68_s_frequency(struct file *file, void *priv,
					const struct v4l2_frequency *f)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;

	if (0 != f->tuner)
		return -EINVAL;
	mutex_lock(&dev->lock);
	dev->ctl_freq = f->frequency;

	//reg_call_all(dev, tuner, s_frequency, f);

	mutex_unlock(&dev->lock);
	return 0;
}


static int TW68_g_audio(struct file *file, void *priv, struct v4l2_audio *a)
{
	strcpy(a->name, "audio");
	return 0;
}

static int TW68_s_audio(struct file *file, void *priv, const struct v4l2_audio *a)
{
	return 0;
}


static int TW68_g_priority(struct file *file, void *f, enum v4l2_priority *p)
{
	struct TW68_fh *fh = f;
	struct TW68_dev *dev = fh->dev;

	*p = v4l2_prio_max(&dev->prio);
	return 0;
}


static int TW68_s_priority(struct file *file, void *f,
					enum v4l2_priority prio)
{
	return  0;
}

static int TW68_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{

	if (f->index >= FORMATS)
		return -EINVAL;

	strlcpy(f->description, formats[f->index].name,
		sizeof(f->description));

	f->pixelformat = formats[f->index].fourcc;

	pr_debug("%s: description %s \n", __func__, f->description);

	return 0;
}

static int TW68_enum_fmt_vid_overlay(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	pr_debug("%s\n", __func__);

	if (TW68_no_overlay > 0) {
		pr_err("V4L2_BUF_TYPE_VIDEO_OVERLAY: no_overlay\n");
		return -EINVAL;
	}

	if ((f->index >= FORMATS) || formats[f->index].planar)
		return -EINVAL;

	strlcpy(f->description, formats[f->index].name,
		sizeof(f->description));

	f->pixelformat = formats[f->index].fourcc;
	return 0;
}

static int TW68_g_fbuf(struct file *file, void *f,
				struct v4l2_framebuffer *fb)
{
	struct TW68_fh *fh = f;
	struct TW68_dev *dev = fh->dev;

	pr_debug("%s\n", __func__);
	*fb = dev->ovbuf;
	fb->capability = V4L2_FBUF_CAP_LIST_CLIPPING;
	return 0;
}

static int TW68_s_fbuf(struct file *file, void *f,
					const struct v4l2_framebuffer *fb)
{
	struct TW68_fh *fh = f;
	struct TW68_dev *dev = fh->dev;
	struct TW68_format *fmt;

	pr_debug("%s\n", __func__);

	if (!capable(CAP_SYS_ADMIN) &&
	   !capable(CAP_SYS_RAWIO))
		return -EPERM;

	/* check args */
	fmt = format_by_fourcc(fb->fmt.pixelformat);
	if (NULL == fmt)
		return -EINVAL;

	/* ok, accept it */
	dev->ovbuf = *fb;
	dev->ovfmt = fmt;
	if (0 == dev->ovbuf.fmt.bytesperline)
		dev->ovbuf.fmt.bytesperline =
			dev->ovbuf.fmt.width*fmt->depth/8;
	return 0;
}

static int TW68_overlay(struct file *file, void *f, unsigned int on)
{
	struct TW68_fh *fh = f;
	struct TW68_dev *dev = fh->dev;
	// unsigned long flags;

	pr_debug("%s\n", __func__);
	if (on) {
		if (TW68_no_overlay > 0) {
			pr_debug("no_overlay\n");
			return -EINVAL;
		}

		if (!res_get(dev, fh, RESOURCE_OVERLAY))
			return -EBUSY;
	}
	if (!on) {
		if (!res_check(fh, RESOURCE_OVERLAY))
			return -EINVAL;
		res_free(fh, RESOURCE_OVERLAY);
	}
	return 0;
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct TW68_fh *fh = file->private_data;
	return videobuf_cgmbuf(TW68_queue(fh), mbuf, 8);
}
#endif

static int TW68_reqbufs(struct file *file, void *priv,
					struct v4l2_requestbuffers *p)
{
	struct TW68_fh *fh = priv;
	return videobuf_reqbufs(TW68_queue(fh), p);
}

static int TW68_querybuf(struct file *file, void *priv,
					struct v4l2_buffer *b)
{
	struct TW68_fh *fh = priv;
	return videobuf_querybuf(TW68_queue(fh), b);
}

static int TW68_qbuf(struct file *file, void *priv, struct v4l2_buffer *b)
{
	struct TW68_fh *fh = priv;

	struct videobuf_queue* q = NULL;
	q = TW68_queue(fh);

	//pr_debug("%s: videobuf_queue: 0X%p   v4l2_buffer: 0X%p.\n", __func__, q, b);

	return videobuf_qbuf(q, b);
}

static int TW68_dqbuf(struct file *file, void *priv, struct v4l2_buffer *b)
{
	struct TW68_fh *fh = priv;

	struct videobuf_queue* q = NULL;
	q = &fh->cap;

	//pr_debug("%s: videobuf_queue: 0X%p  0X%p  v4l2_buffer: 0X%p.\n", __func__, q, TW68_queue(fh), b);
	//TW68_queue(fh)

	return videobuf_dqbuf(q, b,		file->f_flags & O_NONBLOCK);
}

static int TW68_streamon(struct file *file, void *priv,
					enum v4l2_buf_type type)
{
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;
	struct videobuf_queue *q;
	 int res = TW68_resource(fh);
	int streaming;
	u32  nId;


	if (!res_get(dev, fh, res)) {
		return -EBUSY;
	}

	// fh->resources |= res;

	nId = fh->DMA_nCH;
	if (nId == 0XF)
		nId = 0;

	q = TW68_queue(fh);

	//TW68_buffer_requeue(dev, &dev->video_dmaq[nId]);
	streaming = videobuf_streamon(q);
	pr_debug("%s: %s: DMA %d  q->streaming:%X  streaming:%x.\n",
		__func__, dev->name, fh->DMA_nCH,  q->streaming, streaming);

	///////////////////////////////////////////

	//dwReg2 =  reg_readl(DMA_CH0_CONFIG+ 2);
	//dwReg =  reg_readl(DMA_CH0_CONFIG+ nId);

	//pr_debug("%s: CH%d::   dwReg2: 0x%X   deReg 0x%X  \n", __func__, nId, dwReg2, dwReg );

	//read dma config
	if  (fh->DMA_nCH == 0XF) {
		dev->video_dmaq[0].DMA_nCH = 0xF;
		dev->video_dmaq[0].DMA_nCH = 0xF;    // mark in use

		dev->video_DMA_1st_started += 4;   //++
		dev->videoCap_ID |= 0xF ;
		dev->videoDMA_ID |= 0xF ;
	} else {
		TW68_set_dmabits(dev, fh->DMA_nCH);
	}

	return streaming;
}


static int TW68_streamoff(struct file *file, void *priv,
					enum v4l2_buf_type type)
{
	int err;
	struct TW68_fh *fh = priv;
	struct TW68_dev *dev = fh->dev;
	int DMA_nCH = fh->DMA_nCH;
	int nId = DMA_nCH +1;
	struct videobuf_queue *q;
	int res = TW68_resource(fh);
	q=TW68_queue(fh);


	if  (DMA_nCH ==0x0F) {
		dev->video_fieldcount[0] =0;
		stop_video_DMA(dev, 0);				//
		stop_video_DMA(dev, 1);				//
		stop_video_DMA(dev, 2);				//
		stop_video_DMA(dev, 3);				//

		dev->video_fieldcount[0] =0;
		stop_video_DMA(dev, 0);				//
		del_timer(&dev->video_dmaq[0].timeout);
	} else {
		nId = DMA_nCH +1;
		dev->video_fieldcount[nId] =0;
		stop_video_DMA(dev, DMA_nCH );				//
		del_timer(&dev->video_dmaq[nId].timeout);
	}

	pr_debug("%s: %s:  DMA_nCH:%x   videobuf_streamoff delete video timeout    \n",
		__func__, dev->name, fh->DMA_nCH);
	stop_video_DMA(dev, fh->DMA_nCH);

	synchronize_irq(dev->pci->irq);
	err = videobuf_streamoff(q);
	res_free(fh, res);
	pr_debug("%s: %s:%d q->streaming:%x  return err:%x \n",
		__func__, dev->name, fh->DMA_nCH, q->streaming, err );

	return 0;
}


static int TW68_g_parm(struct file *file, void *fh,
				struct v4l2_streamparm *parm)
{
	return 0;
}



static const struct v4l2_file_operations video_fops =
{
	.owner	  = THIS_MODULE,
	.open	  = video_open,
	.release  = video_release,
	.read	  = video_read,
	.poll     = video_poll,
	.mmap	  = video_mmap,
	.ioctl	  = video_ioctl2,
};

static const struct v4l2_ioctl_ops video_ioctl_ops = {
	.vidioc_querycap		= TW68_querycap,
	.vidioc_enum_fmt_vid_cap	= TW68_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= TW68_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap		= TW68_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap		= TW68_s_fmt_vid_cap,
	.vidioc_enum_fmt_vid_overlay	= TW68_enum_fmt_vid_overlay,
	.vidioc_g_fmt_vid_overlay	= TW68_g_fmt_vid_overlay,
	.vidioc_try_fmt_vid_overlay	= TW68_try_fmt_vid_overlay,
	.vidioc_s_fmt_vid_overlay	= TW68_s_fmt_vid_overlay,
	.vidioc_g_fmt_vbi_cap		= TW68_try_get_set_fmt_vbi_cap,
	.vidioc_try_fmt_vbi_cap		= TW68_try_get_set_fmt_vbi_cap,
	.vidioc_s_fmt_vbi_cap		= TW68_try_get_set_fmt_vbi_cap,
	.vidioc_g_audio			= TW68_g_audio,
	.vidioc_s_audio			= TW68_s_audio,
	.vidioc_cropcap			= TW68_cropcap,
	.vidioc_reqbufs			= TW68_reqbufs,
	.vidioc_querybuf		= TW68_querybuf,
	.vidioc_qbuf			= TW68_qbuf,
	.vidioc_dqbuf			= TW68_dqbuf,
	.vidioc_s_std			= TW68_s_std,
	.vidioc_g_std			= TW68_g_std,
	.vidioc_enum_input		= TW68_enum_input,
	.vidioc_g_input			= TW68_g_input,
	.vidioc_s_input			= TW68_s_input,
	.vidioc_queryctrl		= TW68_queryctrl,
	.vidioc_g_ctrl			= TW68_g_ctrl,
	.vidioc_s_ctrl			= TW68_s_ctrl,
	.vidioc_streamon		= TW68_streamon,
	.vidioc_streamoff		= TW68_streamoff,
	.vidioc_g_tuner			= TW68_g_tuner,
	.vidioc_s_tuner			= TW68_s_tuner,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf			= vidiocgmbuf,
#endif
	.vidioc_g_crop			= TW68_g_crop,
	.vidioc_s_crop			= TW68_s_crop,
	.vidioc_g_fbuf			= TW68_g_fbuf,
	.vidioc_s_fbuf			= TW68_s_fbuf,
	.vidioc_overlay			= TW68_overlay,
	.vidioc_g_priority		= TW68_g_priority,
	.vidioc_s_priority		= TW68_s_priority,
	.vidioc_g_parm			= TW68_g_parm,
	.vidioc_g_frequency		= TW68_g_frequency,
	.vidioc_s_frequency		= TW68_s_frequency,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.vidioc_g_register              = vidioc_g_register,
	.vidioc_s_register              = vidioc_s_register,
#endif
};


/* ----------------------------------------------------------- */
/* exported stuff                                              */

struct video_device TW68_video_template = {
	.name				= "TW686v-video",
	.fops				= &video_fops,
	.ioctl_ops			= &video_ioctl_ops,
	.minor				= -1,
	.tvnorms			= TW68_NORMS,
};

void TW68_video_variable_init(struct TW68_dev *dev)
{
	INIT_LIST_HEAD(&dev->video_q.queued);

	init_timer(&dev->delay_resync);
	dev->delay_resync.function = resync;
	dev->delay_resync.data     = (unsigned long)dev;
}


int TW68_video_init1(struct TW68_dev *dev)
{
	int k, m, n;
	__le32      *cpu;
	dma_addr_t   dma_addr;
	/* sanitycheck insmod options */
	if (gbuffers < 2 || gbuffers > VIDEO_MAX_FRAME)
		gbuffers = 2;
	if (gbufsize < 0 || gbufsize > gbufsize_max)
		gbufsize = gbufsize_max;
	gbufsize = (gbufsize + PAGE_SIZE - 1) & PAGE_MASK;

// pci_alloc_consistent   32 4 * 8  continuous field memory buffer

	for (n = 0; n < 8; n++) {
		for (m = 0; m < 4; m++) {
			cpu = pci_alloc_consistent(dev->pci, 800*300*2, &dma_addr);   // 8* 4096 contiguous  //*2
			dev->BDbuf[n][m].cpu = cpu;
			dev->BDbuf[n][m].dma_addr = dma_addr;
			//pr_debug("%s: n:%dm:%d  cpu:%x    dma:%x   \n", __func__, n, m, cpu, dma_addr);
			// assume aways successful   480k each field   total 32  <16MB
		}
	}
	/* put some sensible defaults into the data structures ... */
	dev->ctl_bright     = ctrl_by_id(V4L2_CID_BRIGHTNESS)->default_value;
	dev->ctl_contrast   = ctrl_by_id(V4L2_CID_CONTRAST)->default_value;
	dev->ctl_hue        = ctrl_by_id(V4L2_CID_HUE)->default_value;
	dev->ctl_saturation = ctrl_by_id(V4L2_CID_SATURATION)->default_value;
	dev->ctl_volume     = ctrl_by_id(V4L2_CID_AUDIO_VOLUME)->default_value;
	dev->ctl_mute       = 1; // ctrl_by_id(V4L2_CID_AUDIO_MUTE)->default_value;
	dev->ctl_invert     = ctrl_by_id(V4L2_CID_PRIVATE_INVERT)->default_value;
	dev->ctl_automute   = ctrl_by_id(V4L2_CID_PRIVATE_AUTOMUTE)->default_value;

	////////////////////////////////////////////////////////xxxxxxxxxxx
	mod_timer(&dev->delay_resync, jiffies+ msecs_to_jiffies(30));


	////////////////////////////////////////////////////////xxxxxxxxxxx

	for (k=0; k<9; k++) {
		INIT_LIST_HEAD(&dev->video_dmaq[k].queued);

		INIT_LIST_HEAD(&dev->video_dmaq[k].active);

		dev->video_dmaq[k].dev              = dev;
		init_timer(&dev->video_dmaq[k].timeout);

		dev->video_dmaq[k].timeout.function = TW68_buffer_timeout;
		dev->video_dmaq[k].timeout.data     = (unsigned long)(&dev->video_dmaq[k]);

		if (0) {
			dev->video_param[k].ctl_bright     = ctrl_by_id(V4L2_CID_BRIGHTNESS)->default_value;
			dev->video_param[k].ctl_contrast   = ctrl_by_id(V4L2_CID_CONTRAST)->default_value;
			dev->video_param[k].ctl_hue        = ctrl_by_id(V4L2_CID_HUE)->default_value;
			dev->video_param[k].ctl_saturation = ctrl_by_id(V4L2_CID_SATURATION)->default_value;
			dev->video_param[k].ctl_volume     = ctrl_by_id(V4L2_CID_AUDIO_VOLUME)->default_value;
			dev->video_param[k].ctl_mute       = 1; // ctrl_by_id(V4L2_CID_AUDIO_MUTE)->default_value;
			dev->video_param[k].ctl_automute   = ctrl_by_id(V4L2_CID_PRIVATE_AUTOMUTE)->default_value;
		}

		if (k<4) {
			dev->video_param[k].ctl_bright     = reg_readl(CH1_BRIGHTNESS_REG + k *0x10);
			dev->video_param[k].ctl_contrast   = reg_readl(CH1_CONTRAST_REG + k *0x10);
			dev->video_param[k].ctl_hue        = reg_readl(CH1_HUE_REG + k *0x10);
			dev->video_param[k].ctl_saturation = reg_readl(CH1_SAT_U_REG + k *0x10) /2;
			dev->video_param[k].ctl_mute	   = reg_readl(CH1_SAT_V_REG + k *0x10) /2;
		} else if (k<8) {
			dev->video_param[k].ctl_bright     = reg_readl(CH1_BRIGHTNESS_REG + (k-4) *0x10 +0x100);
			dev->video_param[k].ctl_contrast   = reg_readl(CH1_CONTRAST_REG + (k-4) *0x10 +0x100);
			dev->video_param[k].ctl_hue        = reg_readl(CH1_HUE_REG + (k-4) *0x10 +0x100);
			dev->video_param[k].ctl_saturation = reg_readl(CH1_SAT_U_REG + (k-4) *0x10 +0x100) /2;
			dev->video_param[k].ctl_mute	   = reg_readl(CH1_SAT_V_REG + (k-4) *0x10 +0x100) /2;
		}

		pr_debug("%s: get decoder %d default AMP: BRIGHTNESS %d  CONTRAST %d  HUE_ %d  SAT_U_%d SAT_V_%d\n",
			__func__, k, dev->video_param[k].ctl_bright,
			dev->video_param[k].ctl_contrast,
			dev->video_param[k].ctl_hue,
			dev->video_param[k].ctl_saturation,
			dev->video_param[k].ctl_mute);

	}

	for (k=8; k>0; k--) {
		dev->video_param[k].ctl_bright     = dev->video_param[k-1].ctl_bright;
		dev->video_param[k].ctl_contrast   = dev->video_param[k-1].ctl_contrast;
		dev->video_param[k].ctl_hue        = dev->video_param[k-1].ctl_hue;
		dev->video_param[k].ctl_saturation = dev->video_param[k-1].ctl_saturation;
		dev->video_param[k].ctl_mute	   = dev->video_param[k-1].ctl_mute;
		pr_debug("%s: get decoder %d default AMP: BRIGHTNESS %d  CONTRAST %d  HUE_ %d  SAT_U_%d SAT_V_%d\n",
			__func__, k, dev->video_param[k].ctl_bright,
			dev->video_param[k].ctl_contrast,
			dev->video_param[k].ctl_hue,
			dev->video_param[k].ctl_saturation,
			dev->video_param[k].ctl_mute);
	}

	// Normalize the reg value to standard value range
	for (k=0; k<9; k++) {
		dev->video_param[k].ctl_bright		   = (dev->video_param[k].ctl_bright + 0x80) & 0xFF;
		dev->video_param[k].ctl_contrast   = dev->video_param[k].ctl_contrast & 0x7F;
		dev->video_param[k].ctl_hue        = dev->video_param[k].ctl_hue & 0xFF;
		dev->video_param[k].ctl_saturation = dev->video_param[k].ctl_saturation & 0xFF;
		dev->video_param[k].ctl_mute	   = dev->video_param[k].ctl_mute & 0xFF;
		pr_debug("%s: remap decoder %d default AMP: BRIGHTNESS %d  CONTRAST %d  HUE_ %d  SAT_U_%d SAT_V_%d\n",
			__func__, k, dev->video_param[k].ctl_bright,
			dev->video_param[k].ctl_contrast,
			dev->video_param[k].ctl_hue,
			dev->video_param[k].ctl_saturation,
			dev->video_param[k].ctl_mute);
	}
	return 0;
}


int TW68_video_init2(struct TW68_dev *dev)
{
	/* init video hw */

	int k;
	pr_debug("%s\n", __func__);
	set_tvnorm(dev,&tvnorms[0]);

	for (k=0; k<9; k++)
		dev->tvnormf[k] = &tvnorms[0];


	//video_mux(dev,0); ///0 3
	//TW68_tvaudio_setmute(dev);
	//TW68_tvaudio_setvolume(dev,dev->ctl_volume);
	return 0;
}

void TW68_irq_video_done(struct TW68_dev *dev, unsigned int nId, u32 dwRegPB)
{
	enum v4l2_field field;
	static int Done;
	int Fn, PB;   /// d0, w0,

	Fn = (dwRegPB >>24) & (1<< (nId-1));
	PB = (dwRegPB ) & (1<< (nId-1));

	if ((dev->video_dmaq[0].DMA_nCH == 0xF ) && ((nId-1) < 4) ) {
		if ((dev->video_dmaq[0].curr)) {
			//if (dev->QFbit ^ (1 << nId)) {
			dev->video_dmaq[0].FieldPB = dwRegPB;
			QF_Field_Copy(dev, (nId-1), Fn, PB);
			dev->QFbit |= (1 << (nId-1));
			//}
			pr_debug("%s: dev->QFbit=0X%x nId=0X%x\n", __func__, dev->QFbit, nId);

			if ((dev->QFbit & 0xF) == 0xF) {
				dev->QFbit =0;
				TW68_buffer_finish(dev, &dev->video_dmaq[0], VIDEOBUF_DONE);
				TW68_buffer_next(dev,&(dev->video_dmaq[0]));
			}
		}
		return;
	}


	pr_debug("%s: nId=0x%x .curr(%p) dwRegPB=0x%x\n", __func__, nId, dev->video_dmaq[nId].curr, dwRegPB);
	if (dev->video_dmaq[nId].curr) {
		dev->video_fieldcount[nId]++;
		field = dev->video_dmaq[nId].curr->vb.field;
		//pr_debug("%s: video_dmaq[nId].curr   nId  %x  dwRegPB 0x%X\n", __func__, nId, dwRegPB);
		dev->video_dmaq[nId].curr->vb.field_count = dev->video_fieldcount[nId];

		Fn = (dwRegPB >>24) & (1<< (nId-1));
		PB = (dwRegPB ) & (1<< (nId-1));
		//pr_debug("%s: d0:%d   w0:%d \n", __func__, d0, w0);

		// skip a field
#if 0
		if (d0 == w0) {
			//dev->video_dmaq[nId].FieldPB = 0;
			pr_debug("%s: dropped a field    d0:%d   w0:%d \n", __func__, d0, w0);
			goto done;
		}
#endif

		//if (dev->video_fieldcount[nId] %2)
		//  weave frame output
		if (Fn ==0) {
			// field 0 interrupt  program update  P field mapping
			Done = BF_Copy(dev, nId-1, Fn, PB);
			//Done =1;
			//pr_debug("%s: nId%x  Fn:%d  field count = %d\n", __func__, nId, Fn, dev->video_fieldcount[nId]);
			goto done;
		} else {
			// copy bottom field
			//dev->video_dmaq[nId].FieldPB &= (~1);
			if (Done)
				BF_Copy(dev, nId-1, Fn, PB);
			//pr_debug("%s: nId%x  Fn:%d BBB  field count = %d \n", __func__, nId, Fn, dev->video_fieldcount[nId]);
			if(!Done)
				goto done;
			TW68_buffer_finish(dev, &dev->video_dmaq[nId], VIDEOBUF_DONE);
		}
	}
	// B field interrupt  program update  P field mapping
	TW68_buffer_next(dev,&(dev->video_dmaq[nId]));
 done:
	return;
}


int buffer_setup_QF(struct videobuf_queue *q, unsigned int *count, unsigned int *size)
{
	unsigned int  ChannelOffset, nId, pgn;
	u32	m_dwCHConfig, dwReg, dwRegH, dwRegW, nScaler, nW, nH, nSize;
	u32  m_StartIdx, m_EndIdx, m_nVideoFormat, \
		 m_bHorizontalDecimate, m_bVerticalDecimate, m_nDropChannelNum, \
		 m_bDropMasterOrSlave, m_bDropField, m_bDropOddOrEven, m_nCurVideoChannelNum;

	struct TW68_dev *dev;
	struct TW68_fh *fh = q->priv_data;
	dev = fh->dev;

	dev->video_dmaq[0].DMA_nCH = 0xF;
	dev->video_dmaq[0].curr = 0;
	dev->QFbit = 0;

	ChannelOffset = (PAGE_SIZE <<1) /8 /8;
    // NTSC  FIELD entry number for 720*240*2
	/// ChannelOffset = 128;   ///85;

	fh = q->priv_data;

	*size = fh->fmt->depth * fh->width * fh->height >> 3;    // calculate byte size for 1 frame

	nW = fh->width /2;
	nH = fh->height /2;
	nSize = *size /4;			// field size

	for (nId = 0; nId < 4; nId++) {
		if (nId < 4) {
			dwReg =  reg_readl(DECODER0_SDT+ (nId*0x10));
			pr_debug("%s: nId %d,  0X%X    nW%d nH%d nSize%d\n",
				__func__, nId, dwReg, nW, nH, nSize);
			reg_writel(DECODER0_SDT+ (nId*0x10), 7);		/// 0 NTSC
		}
		////////////////////////////// decoder resize //////////////////////////////////////////

		DecoderResize(dev, nId, nH, nW);    // Field size

		//Fixed_SG_Mapping(dev, nId, nSize*2);  //   nDMA_channel

		BFDMA_setup(dev, nId, fh->height /2, (*size /fh->height /2));   // BFbuf setup  DMA mode ...

		if (0 == *count)
			*count = gbuffers;

		//pgn = (*size + PAGE_SIZE -1) /PAGE_SIZE;
		pgn = TW68_buffer_pages(nSize) -1;  // page number for 1 field

		m_nDropChannelNum = 0;
		m_bDropMasterOrSlave = 1;   // master
		m_bDropField = 0;      ////////////////////// 1
		m_bDropOddOrEven = 0;
		m_bHorizontalDecimate = 0;
		m_bVerticalDecimate = 0;

		m_StartIdx = ChannelOffset * nId;
		m_EndIdx = m_StartIdx + pgn;    ///pgn;  85 :: 720 * 480
		m_nCurVideoChannelNum = 0;  // real-time video channel  starts 0
		m_nVideoFormat = dev -> nVideoFormat[nId];
		//if (m_nVideoFormat != dev -> nVideoFormat[nId])
		pr_debug("%s: N%d   F%d  to %d\n", __func__, nId,
				m_nVideoFormat, dev -> nVideoFormat[nId]);

		pr_debug("%s: W:%d H:%d  frame size: %d,  gbuffers: %d, ChannelOffset: %d  field pgn: %d  m_StartIdx %d  m_EndIdx %d   m_nVideoFormat %x\n",
			__func__, fh->width, fh->height, *size,  *count,
			ChannelOffset, pgn, m_StartIdx, m_EndIdx, m_nVideoFormat );

		m_dwCHConfig = (m_StartIdx & 0x3FF)		|	// 10 bits
				((m_EndIdx&0x3FF)<<10)		|	// 10 bits
				((m_nVideoFormat&7)<<20)	|
				((m_bHorizontalDecimate&1)<<23)	|
				((m_bVerticalDecimate&1)<<24)	|
				((m_nDropChannelNum&3)<<25)	|
				((m_bDropMasterOrSlave&1)<<27)	|	// 1 bit
				((m_bDropField&1)<<28)		|
				((m_bDropOddOrEven&1)<<29)	|
				((m_nCurVideoChannelNum&3)<<30);

		reg_writel(DMA_CH0_CONFIG+ nId, m_dwCHConfig);
		dwReg =  reg_readl(DMA_CH0_CONFIG+ nId);
		pr_debug("%s: CH%d::  m_StartIdx 0X%x  pgn%d  dwReg: 0x%X   m_dwCHConfig 0x%X  \n",
			__func__, nId, m_StartIdx, pgn,  m_dwCHConfig, dwReg );


		/* external video decoder settings */

		dwRegW = nW;
		dwRegH = nH;  // field height

		dwReg = dwRegW | (dwRegH<<16) | (1<<31);
		dwRegW = dwRegH = dwReg;

		//Video Size
		reg_writel(VIDEO_SIZE_REG,  dwReg);	    //for Rev.A backward compatible
		//xxx dwReg = reg_readl(VIDEO_SIZE_REG);

		pr_debug("%s: VIDEO_SIZE_REG: 0x%X,   0x%X \n", __func__, VIDEO_SIZE_REG, dwReg);

		reg_writel(VIDEO_SIZE_REG0+nId,  dwReg);  //for Rev.B or later only

		//Scaler
		dwRegW &= 0x7FF;
		dwRegW = (720*256)/dwRegW;
		dwRegH = (dwRegH>>16)&0x1FF;
		// 60HZ video
		dwRegH = (240*256)/dwRegH;

	/// 0915  rev B  black ....
		nScaler = VSCALE1_LO ;  /// + (nId<<4); //VSCALE1_LO + 0|0x10|0x20|0x30


		dwReg = dwRegH & 0xFF;  //V
		nScaler++;  //VH

		dwReg = (((dwRegH >> 8)& 0xF) << 4) | ((dwRegW>>8) & 0xF );
		///if(nId >= 4) DeviceWrite2864(nAddr,tmp);

		/// reg_writel(nScaler,  dwReg);

		nScaler++;  //H
		dwReg = dwRegW & 0xFF;

		//setup for Black stripe remover
		dwRegW = fh->width;  ///-12;  //EndPos
		dwRegH = 4;		   //StartPos
		dwReg = (dwRegW - dwRegH)*(1<<16)/ fh->width;
		dwReg = (dwRegH & 0x1F) |
			((dwRegH & 0x3FF) << 5) |
			(dwReg <<15);
		reg_writel(DROP_FIELD_REG0+ nId,  0xBFFFFFFF);   //28 // B 30 FPS
	}
	return 0;
}
