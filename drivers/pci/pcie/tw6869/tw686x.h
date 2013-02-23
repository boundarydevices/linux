/*
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
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/pci.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/videodev2.h>
#include <linux/kdev_t.h>
#include <linux/input.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#include <asm/io.h>
#include <linux/version.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
#include <media/v4l2-device.h>
#endif
#include <media/videobuf-dma-sg.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include "videobuf-dma-contig-tw.h"

#define TW686X_VERSION_CODE KERNEL_VERSION(0, 1, 4)
#define TW6869_RESETDMA     1

#include "i2c-sw.h"

#define I2C_SW_TW686X	0x0A0000 /* TW686X video decoder bus */

extern unsigned int tw686x_bcsi;	/* for BCSI */
extern unsigned int tw686x_restart_timer;	/* dma restart timer */
extern unsigned int tw686x_dmamode;
extern unsigned int tw686x_debug;
extern unsigned int tw686x_6865;	//zxx_20120712 added for TW6865
#define dprintk(level, chip, fmt, arg...)\
	do { if (tw686x_debug >= level)\
		printk(KERN_DEBUG "%s: " fmt, (chip)->name, ## arg);\
	} while (0)
#define dvprintk(level, dev, fmt, arg...)\
	do { if (tw686x_debug >= level)\
		printk(KERN_DEBUG "%s[v-%d]: " fmt, ((dev)->chip)->name, (dev)->channel_id, ## arg);\
	} while (0)
#define daprintk(level, dev, fmt, arg...)\
	do { if (tw686x_debug >= level)\
		printk(KERN_DEBUG "%s[a-%d]: " fmt, ((dev)->chip)->name, (dev)->channel_id, ## arg);\
	} while (0)

#define dvcount(level, dev, cnt, cnt_out)       \
{                                               \
    if(tw686x_debug >= level) {                 \
        static struct timeval _tv_begin_##cnt = {0,0};  \
        static long _cnt_##cnt = 0;             \
        struct timeval _tv_now_##cnt;           \
        long long _tv_diff_##cnt = 0;           \
        long _ms_diff_##cnt = 0;                \
        do_gettimeofday( &_tv_now_##cnt );      \
        _tv_diff_##cnt = 1000000 * ( _tv_now_##cnt.tv_sec - _tv_begin_##cnt.tv_sec ) +      \
                  _tv_now_##cnt.tv_usec - _tv_begin_##cnt.tv_usec;                          \
        _ms_diff_##cnt = (long)_tv_diff_##cnt;  \
        _ms_diff_##cnt /= 1000;                 \
        _cnt_##cnt++;                           \
        if(_ms_diff_##cnt >= 1000) {            \
            dvprintk(1, dev, "%s-%d %d\n", cnt_out, (int)_cnt_##cnt, (int)_ms_diff_##cnt);  \
            _tv_begin_##cnt = _tv_now_##cnt;    \
            _cnt_##cnt = 0;                     \
        }                                       \
    }                                           \
}

#define DPRT_LEVEL0        11

#define RESOURCE_VIDEO     2
#define BUFFER_TIMEOUT     (HZ)  /* 0.5 seconds */

/* ioctl for tw686x */
#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
#define TW686X_S_REGISTER           _IOWR('V', (BASE_VIDIOC_PRIVATE + 1), struct v4l2_dbg_register)
#define TW686X_G_REGISTER           _IOWR('V', (BASE_VIDIOC_PRIVATE + 2), struct v4l2_dbg_register)
#else
#define TW686X_S_REGISTER           _IOWR('V', (BASE_VIDIOC_PRIVATE + 1), struct v4l2_register)
#define TW686X_G_REGISTER           _IOWR('V', (BASE_VIDIOC_PRIVATE + 2), struct v4l2_register)
#endif
#define TW686X_S_FRAMERATE          _IOWR('V', (BASE_VIDIOC_PRIVATE + 3), long)
#define TW686X_G_FRAMERATE          _IOWR('V', (BASE_VIDIOC_PRIVATE + 4), long)
#define TW686X_G_SIGNAL             _IOWR('V', (BASE_VIDIOC_PRIVATE + 5), long)

#define TW686X_S_AUDIO              _IOWR('V', (BASE_VIDIOC_PRIVATE + 6), struct tw686x_aparam)
#define TW686X_G_AUDIO              _IOWR('V', (BASE_VIDIOC_PRIVATE + 7), struct tw686x_adata)

#define TW686X_DAEMON_PERIOD        2000    /*2s*/
#define TW686X_DAEMON_THRESHOLD     16      /*2s*/
#define TW686X_DAEMON_TOLERANCE     2       /*2 times*/
#define TW686X_DAEMON_MIN           15      /*min frame rate*/

#define TW686X_DMA_MODE_SG          0       /* 0--scatter gather, 1--block dma*/
#define TW686X_DMA_MODE_BLOCK       1
#define IS_TW686X_DMA_MODE_BLOCK    (tw686x_dmamode & TW686X_DMA_MODE_BLOCK)
#define TW686X_DMA_DESC_LEN         4096
#define TW686X_DMA_DESC_P           0
#define TW686X_DMA_DESC_B           1
#define TW686X_DMA_DESC_NUM         2
#define TW686X_DMA_DESC_UNIT		512
#define TW686X_DMA_DESC_UNIT_LEN	(TW686X_DMA_DESC_UNIT*8)
#define TW686X_DMA_DESC(dev, pb)     ((struct tw686x_dmadesc*)((unsigned long)(dev)->dma_desc[TW686X_DMA_DESC_##pb].cpu))
#define TW686X_SG_PBLEN             (TW686X_DMA_DESC_UNIT*TW686X_DMA_DESC_LEN)

#define TW686X_DECODER_COUNT        8
#define TW6864_MUX_COUNT            1
#define TW2865_MUX_COUNT            1
#define TW2864_MUX_COUNT            1
#define TW686X_VIDEO_COUNT          8
#define TW686X_AUDIO_COUNT          8
#define TW686X_AUDIO_BEGIN          8
#define TW686X_BUFFER_COUNT         9
#define TW686X_DEFAULT_STANDARD     V4L2_STD_NTSC
#define TW686X_DEFAULT_WIDTH        704
#define TW686X_DEFAULT_HEIGHT       480
#define TW686X_REMOVE_BLACKSTRIPE   0 //(1 << 31)
#define TW686X_VIDEO_SIZE(w,h)		((w) | ((h)<<16) | (1<<31))
#define TW686X_VIDEO_SIZE_F2(w,h)	((w) | ((h)<<16))	//Field2 size,  Available only for Rev.B or later
#define TW686X_MAX_FRAMERATE(std)   (IS_PAL(std) ? 25 : 30)
#define TW686X_MIN_WIDTH            160
#define TW686X_MIN_HEIGHT           120

/* Currently unsupported by the driver: PAL/H, NTSC/Kr*/
#define TW686X_NORMS                (V4L2_STD_NTSC |  V4L2_STD_PAL)

#define IS_PAL( std )               ( (std) & V4L2_STD_PAL )

struct tw686x_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
	int   flags;
};

struct tw686x_dmadesc {
	u32	  ctrl;   //DO NOT CHANGE SEQUENCE, and it is valid for little endian target PC only
	u32	  addr;
};

struct tw686x_fh {
	struct tw686x_vdev         *dev;
	enum v4l2_buf_type         type;
	u32                        resources;
	struct videobuf_queue      vidq;

	/* video capture */
	struct tw686x_fmt         *fmt;
	int                        width;
	int                        height;
	int                        bytesperline;
	int                        ctl_bright;
	int                        ctl_contrast;
	int                        ctl_hue;
	int                        ctl_saturation;
};

/* buffer for one video frame */
struct tw686x_buf {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer     vb;

	/* tw686x specific */
	struct tw686x_fmt*         fmt;
	struct list_head           active;

	int                        sg_bytes_to;
	int                        sg_bytes_pb;
	struct scatterlist         *sg;
	int                        sg_offset;
	int                        sg_ofo;      /*one p/b buffer for one frame*/

	int (*activate)(struct tw686x_vdev *dev, struct tw686x_buf *buf,
                    int dma_pb, bool queued);
};

struct tw686x_dmaqueue {
	struct list_head           active;
	struct list_head           queued;
	struct timer_list          timeout;
};

/* buffer for dma */
struct tw686x_dmabuf {
	unsigned int   size;
	__le32         *cpu;
	dma_addr_t     dma;
	struct vm_area_struct *vma;
};

struct tw686x_i2c {
    struct i2c_sw              i2c;
    u32                        control_word;
};

/*
 * audio main chip structure
 */
typedef struct snd_card_tw686x {
	struct snd_card *card;
//	struct pci_dev *pci;
	struct tw686x_adev *dev;

	u16 mute_was_on;

	spinlock_t lock;
} snd_card_tw686x_t;

struct tw686x_chip {
	struct list_head           chiplist;
	spinlock_t                 slock;

	/* pci i/o */
	char                       name[32];
	int                        nr;
	u32                        dma_restart;

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	struct v4l2_device         v4l2_dev;
#endif
	struct pci_dev             *pci;
	__u8                       __iomem *bmmio;

	/* i2c i/o */
	struct i2c_adapter         i2c_adap;
	struct i2c_client          i2c_client;
	struct tw686x_i2c          i2c_686x;

    struct tw686x_vdev         *vid_dev[TW686X_VIDEO_COUNT];
	int                        vid_count;
	struct timeval             daemon_tv;
	int                        daemon_nr;
	int                        daemon_tor;

    struct tw686x_adev         *aud_dev[TW686X_AUDIO_COUNT];
	int                        aud_count;

    //assist timer, for video decoder status checking
	struct timer_list          tm_assist;
};

/* global device status */
struct tw686x_vdev {
    struct tw686x_chip*        chip;

	struct list_head           devlist;
	struct mutex               lock;
	spinlock_t*                slock;
	struct v4l2_subdev 	       *sd_tw286x;

	/* various device info */
	unsigned int               resources;
	struct video_device        *video_dev;
	struct tw686x_dmaqueue     video_q;
	struct tw686x_dmabuf       dma_vb[TW686X_BUFFER_COUNT];
	struct tw686x_dmabuf       dma_desc[TW686X_DMA_DESC_NUM];
	struct timer_list          tm_restart;

	/* various v4l controls */
	v4l2_std_id	               tvnorm;
	unsigned int               input;

	/* other global state info */
	u32                        vstatus;

	/* for dma buffer queue */
	int                        buf_needs;
	int                        pb_next;
	int                        pb_curr;
	int                        dma_started;
	int                        channel_id;
	int                        field;
	int                        framerate;
};

/* dmasound dsp status */
struct tw686x_adev {
    struct tw686x_chip*        chip;
	int                        channel_id;
    snd_card_tw686x_t *        card;

	struct mutex               lock;
	spinlock_t*                slock;
	int                        minor_dsp;
	unsigned int               users_dsp;

	/* dsp */
	unsigned int               rate;
	unsigned int               blocks;
	unsigned int               blksize;
	unsigned int               bufsize;
	struct videobuf_dmabuf     dma;
	unsigned int               read_offset;
	struct snd_pcm_substream   *substream;

    unsigned int               dma_blk;
    unsigned int               pb_flag;
    unsigned int               period_idx;
    bool                        running;
};

struct tw686x_aparam {
    int sample_rate;
    int sample_bits;
    int channels;
    int blksize;
};

struct tw686x_adata {
    unsigned char data[4092];
    int len;
};

#if(LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
static inline struct tw686x_chip *to_tw686x(struct v4l2_device *v4l2_dev)
{
	return container_of(v4l2_dev, struct tw686x_chip, v4l2_dev);
}
#endif

/* ----------------------------------------------------------- */

#define tw_read(reg)             readl(chip->bmmio + (reg))
#define tw_write(reg, value)     writel((value), chip->bmmio + (reg))

#define tw_andor(reg, mask, value) \
  writel((readl(chip->bmmio+(reg)) & ~(mask)) |\
  ((value) & (mask)), chip->bmmio+(reg))

#define tw_set(reg, bit)          tw_andor((reg), (bit), (bit))
#define tw_clear(reg, bit)        tw_andor((reg), (bit), 0)

#define call_all(dev, o, f, args...) \
	v4l2_device_call_all(&dev->v4l2_dev, 0, o, f, ##args)

extern struct mutex tw686x_chiplist_lock;
extern struct list_head tw686x_chiplist;

/* ----------------------------------------------------------- */
/* tw686x-core.c                                               */
int tw686x_dmabuf_alloc(struct tw686x_chip *chip,
		       struct tw686x_dmabuf *buf, unsigned int size);
void tw686x_dmabuf_free(struct tw686x_chip *chip, struct tw686x_dmabuf *buf);

/* ----------------------------------------------------------- */
/* tw686x-video.c                                              */
/* Video */
extern int  tw686x_video_register(struct tw686x_vdev *dev);
extern void tw686x_video_unregister(struct tw686x_vdev *dev);
extern int  tw686x_video_irq(struct tw686x_vdev *dev, u32 dma_status, u32 pb_status);
extern int  tw686x_video_daemon(struct tw686x_chip *chip, u32 dma_error);
extern int  tw686x_video_status(struct tw686x_vdev *dev);
extern int 	tw686x_video_resetdma(struct tw686x_chip* chip, u32 dma_cmd);
extern int 	tw686x_video_restart(struct tw686x_vdev *dev, bool b_framerate);
extern void tw686x_video_start(struct tw686x_vdev* dev);

/* ----------------------------------------------------------- */
/* tw686x-alsa.c                                              */
/* Audio */
extern int  tw686x_alsa_create(struct tw686x_adev *dev);
extern int  tw686x_alsa_free(struct tw686x_adev *dev);
extern void tw686x_alsa_irq(struct tw686x_adev *dev, u32 dma_status, u32 pb_status);
extern int  tw686x_alsa_resetdma(struct tw686x_chip *chip, u32 dma_cmd);

/* tw686x-i2c.c                                                */
extern int  tw686x_i2c_register(struct tw686x_chip *chip);
extern int  tw686x_i2c_unregister(struct tw686x_chip *chip);

/* ----------------------------------------------------------- */
/* tw686x-audio.c                                              */
/* Audio */
extern int  tw686x_audio_create(struct tw686x_adev *dev);
extern int  tw686x_audio_free(struct tw686x_adev *dev);
extern void tw686x_audio_irq(struct tw686x_adev *dev, u32 dma_status, u32 pb_status);
extern int  tw686x_audio_param(struct tw686x_adev *dev, struct tw686x_aparam* ap);
extern int  tw686x_audio_data(struct tw686x_adev *dev, struct tw686x_adata* ap);
extern int  tw686x_audio_resetdma(struct tw686x_chip *chip, u32 dma_cmd);
extern int  tw686x_audio_trigger(struct tw686x_adev *dev, int cmd);

/* ----------------------------------------------------------- */
/* tv norms                                                    */

static unsigned int inline norm_maxw(v4l2_std_id norm)
{
	//return (norm & (V4L2_STD_MN & ~V4L2_STD_PAL_Nc)) ? 720 : 768;
	return 704;
}

static unsigned int inline norm_maxh(v4l2_std_id norm)
{
	return (norm & V4L2_STD_625_50) ? 576 : 480;
}
