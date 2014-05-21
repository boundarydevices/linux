#ifndef _GS2971_H
#define _GS2971_H


#ifdef __KERNEL__
#include <linux/spi/spi.h>
#include <linux/device.h>
//#include <linux/videodev.h>
#include <linux/gpio.h>
#include <linux/videodev2.h>
#endif				/* __KERNEL__ */

//#include <media/davinci/videohd.h> // For HD std (V4L2_STD_1080I, etc)

typedef enum {
     GS2971_VID_STD_1920_1080_60FPS = 0x2B,
     GS2971_VID_STD_1920_1080_50FPS = 0x2D,
     GS2971_VID_STD_1920_1080_60FPS_DBLSAMPLED = 0x2A,
     GS2971_VID_STD_1920_1080_50FPS_DBLSAMPLED = 0x2C,
     GS2971_VID_STD_1280_720_60FPS = 0x20,
     GS2971_VID_STD_1280_720_50FPS = 0x24,

     GS2971_VID_STD_1920_1080_30FPS = 0x0B,
     GS2971_VID_STD_1920_1080_25FPS = 0x0D,
} gs2971_vid_std_t;


#define GS2971_NUM_CHANNELS  1

#define GS2971_MAX_NO_CONTROLS  0
#define GS2971_MAX_NO_INPUTS    1
#define GS2971_MAX_NO_STANDARDS 11


/* Video core registers */

#define GS2971_REG_VIDEO_CONFIG           0x00

#define GS2971_REG_VIDEO_CONFIG_TRS_REMAP_DISABLE_MASK       0x4000
#define GS2971_REG_VIDEO_CONFIG_EDH_FLAG_DISABLE_MASK        0x1000
#define GS2971_REG_VIDEO_CONFIG_EDH_CRC_DISABLE_MASK         0x0800
#define GS2971_REG_VIDEO_CONFIG_H_CONFIG_MASK                0x0400 /* 1=TRS-based blanking, 0=active-line blanking */

#define GS2971_REG_VIDEO_CONFIG_ANC_DATA_DISABLE_MASK        0x0200
#define GS2971_REG_VIDEO_CONFIG_AUDIO_DISABLE_MASK           0x0100
#define GS2971_REG_VIDEO_CONFIG_861_PIN_DISABLE_MASK         0x0080
#define GS2971_REG_VIDEO_CONFIG_TIMING_861_MASK              0x0040 /* 1=CEA-861 timing, 0=F/V/H timing */
#define GS2971_REG_VIDEO_CONFIG_ILLEGAL_REMAP_DISABLE_MASK   0x0010
#define GS2971_REG_VIDEO_CONFIG_ANC_CRC_DISABLE_MASK         0x0008
#define GS2971_REG_VIDEO_CONFIG_CRC_INS_DISABLE_MASK         0x0004
#define GS2971_REG_VIDEO_CONFIG_LINENUM_INS_DISABLE_MASK     0x0002
#define GS2971_REG_VIDEO_CONFIG_TRS_INS_DISABLE_MASK         0x0001




#define GS2971_REG_ERROR_STATUS_LATCHED   0x02


#define GS2971_REG_VID_STD_DETECT         0x06

#define GS2971_REG_VID_STD_DETECT_MASK    0x3f00
#define GS2971_REG_VID_STD_DETECT_SHIFT        8

#define GS2971_REG_STAT_2_1_0_CONFIG      0x08
#define GS2971_REG_STAT_5_4_3_CONFIG      0x09

#define GS2971_STAT_CONFIG_HORIZ          0x00 // horiz blank signal or HSYNC
#define GS2971_STAT_CONFIG_VERT           0x01 // vert blank signal or VSYNC
#define GS2971_STAT_CONFIG_F_DE           0x02 // field signal or DE
#define GS2971_STAT_CONFIG_LOCKED         0x03 // locked indication
#define GS2971_STAT_Y_ANC                 0x04 // Luma anciallary indication
#define GS2971_STAT_C_ANC                 0x05 // Chroma anciallary indication
#define GS2971_STAT_DATA_ERROR            0x06 // Data error
#define GS2971_STAT_VIDEO_ERROR           0x07 // Video error
#define GS2971_STAT_AUDIO_ERROR           0x08 // Audio error
#define GS2971_STAT_EDH_DETECT            0x09 // EDH detected
#define GS2971_STAT_CARRIER_DETECT        0x0A // Carrier detected
#define GS2971_STAT_RATE_DET0             0x0B // Rate detect bit 0
#define GS2971_STAT_RATE_DET1             0x0C // Rate detect bit 1

#define GS2971_REG_ANC_CONFIG             0x0a

#define GS2971_ANC_CONFIG_ANC_DATA_SWITCH 0x08

#define GS2971_REG_ANC_LINEA              0x0b
#define GS2971_REG_ANC_LINEB              0x0c
#define GS2971_REG_ANC_TYPE1              0x0f
#define GS2971_REG_ANC_TYPE2              0x10
#define GS2971_REG_ANC_TYPE3              0x11
#define GS2971_REG_ANC_TYPE4              0x12
#define GS2971_REG_ANC_TYPE5              0x13

#define GS2971_REG_WORDS_PER_ACTLINE      0x1f // words per active line
#define GS2971_REG_WORDS_PER_ACTLINE_MASK 0x3fff

#define GS2971_REG_WORDS_PER_LINE         0x20 // words per line (including blanking)
#define GS2971_REG_WORDS_PER_LINE_MASK    0x3fff

#define GS2971_REG_LINES_PER_FRAME        0x21 // lines per frame (including blanking)
#define GS2971_REG_LINES_PER_FRAME_MASK   0x7ff

#define GS2971_REG_STD_LOCK               0x22

#define GS2971_REG_STD_LOCK_RATESEL_MASK  0xc000

#define GS2971_REG_STD_LOCK_RATESEL_HD     0x0000
#define GS2971_REG_STD_LOCK_RATESEL_SD1    0x1000
#define GS2971_REG_STD_LOCK_RATESEL_FULLHD 0x2000
#define GS2971_REG_STD_LOCK_RATESEL_SD2    0x3000

#define GS2971_REG_STD_LOCK_RATESEL_SHIFT     14
#define GS2971_REG_STD_LOCK_M_MASK        0x2000
#define GS2971_REG_STD_LOCK_M_SHIFT           13
#define GS2971_REG_STD_LOCK_MASK          0x1000
#define GS2971_REG_STD_LOCK_SHIFT             12
#define GS2971_REG_STD_LOCK_INT_PROG_MASK  0x0800
#define GS2971_REG_STD_LOCK_INT_PROG_SHIFT     11

#define GS2971_REG_STD_LOCK_ACT_LINES_MASK 0x7ff

#define GS2971_REG_HVLOCK                 0x23
#define GS2971_REG_HVLOCK_VLOCK_MASK      0x0002
#define GS2971_REG_HVLOCK_HLOCK_MASK      0x0001

#define GS2971_REG_SYNC_POL                   0x26
#define GS2971_REG_SYNC_POL_VSYNC_INV_MASK    0x0004
#define GS2971_REG_SYNC_POL_VSYNC_INV_SHIFT        2
#define GS2971_REG_SYNC_POL_HSYNC_INV_MASK    0x0002
#define GS2971_REG_SYNC_POL_HSYNC_INV_SHIFT        1
#define GS2971_REG_SYNC_POL_TRS_861_MASK      0x0001 // BT.656 timing (7 extra lines of blanks) for 525i


#define GS2971_REG_AUDIO_CONFIG                   0x39
#define GS2971_REG_AUDIO_CONFIG_SCLK_INV_MASK   0x0010
#define GS2971_REG_AUDIO_CONFIG_SCLK_INV_SHIFT       4
#define GS2971_REG_AUDIO_CONFIG_MCLK_INV_MASK   0x0008
#define GS2971_REG_AUDIO_CONFIG_MCLK_INV_SHIFT       3

#define GS2971_REG_AUDIO_CONFIG_MCLK_SEL_MASK   0x0003
#define GS2971_REG_AUDIO_CONFIG_MCLK_SEL_128FS  0x0000
#define GS2971_REG_AUDIO_CONFIG_MCLK_SEL_256FS  0x0001
#define GS2971_REG_AUDIO_CONFIG_MCLK_SEL_512FS  0x0002

#define GS2971_REG_DRIVE_STRENGTH                 0x6d

#define GS2971_REG_DRIVE_STRENGTH_4MA             0x00
#define GS2971_REG_DRIVE_STRENGTH_6MA             0x01
#define GS2971_REG_DRIVE_STRENGTH_8MA             0x02
#define GS2971_REG_DRIVE_STRENGTH_10MA            0x03

/* HD Audio registers - regs 0x200-0x297*/

#define GS2971_REG_HD_AUDIO_CONFIG                0x200

#define GS2971_REG_HD_AUDIO_MUTE                  0x205
#define GS2971_REG_HD_AUDIO_STATUS                0x206


/* SD Audio registers - regs 0x400-0x496*/

#define GS2971_REG_SD_AUDIO_MUTE                  0x400
#define GS2971_REG_SD_AUDIO_STATUS                0x406
#define GS2971_REG_SD_AUDIO_CONFIG                0x408


/* ANC data extraction - regs 0x800-0xBFF */
#define GS2971_ANC_BANK_REG                      0x800

#define GS2971_ANC_BANK_SIZE                     0x400

// Control pins
#define SMPTE_BYPASS		IMX_GPIO_NR(5, 8)
#define DVB_ASI		IMX_GPIO_NR(5, 9)

#define GS2971_SPI_TRANSFER_MAX 1024

struct gs2971_spidata {
	struct spi_device	*spi;

	struct mutex		buf_lock;
	unsigned		users;
	u8			*buffer;
	u16 txbuf[GS2971_SPI_TRANSFER_MAX+1];
	u16 rxbuf[GS2971_SPI_TRANSFER_MAX+1];
};

struct gs2971_params {
	v4l2_std_id std;
	int         inputidx;
	struct v4l2_format fmt;
};

struct gs2971_channel {
    // struct v4l2_subdev    sd;
     struct gs2971_spidata *spidata;
     struct gs2971_params   params;
     u16    anc_buf[GS2971_ANC_BANK_SIZE];
};

#endif
