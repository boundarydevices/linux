#define CODEC_CLOCK 16500000
/* SSI clock sources */
#define IMX_SSP_SYS_CLK		0

#define MIN_FPS 30
#define MAX_FPS 75
#define DEFAULT_FPS 60

#define TC358743_XCLK_MIN 27000000
#define TC358743_XCLK_MAX 42000000

#define TC358743_CHIP_ID_HIGH_BYTE		0x0
#define TC358743_CHIP_ID_LOW_BYTE		0x0
#define TC3587430_HDMI_DETECT			0x0f //0x10

#define TC_VOLTAGE_DIGITAL_IO           1800000
#define TC_VOLTAGE_DIGITAL_CORE         1500000
#define TC_VOLTAGE_DIGITAL_GPO		1500000
#define TC_VOLTAGE_ANALOG               2800000

#define MAX_COLORBAR	tc358743_mode_INIT6
#define IS_COLORBAR(a) (a <= MAX_COLORBAR)

enum tc358743_mode {
	tc358743_mode_INIT,
	tc358743_mode_INIT1,
	tc358743_mode_INIT2,
	tc358743_mode_INIT3,
	tc358743_mode_INIT4,
	tc358743_mode_INIT5,
	tc358743_mode_INIT6,
	tc358743_mode_480P_720_480,
	tc358743_mode_720P_60_1280_720,
	tc358743_mode_480P_640_480,
	tc358743_mode_1080P_1920_1080,
	tc358743_mode_720P_1280_720,
	tc358743_mode_1024x768,
	tc358743_mode_MAX,
};

enum tc358743_frame_rate {
	tc358743_60_fps,
	tc358743_30_fps,
	tc358743_75_fps,
	tc358743_max_fps
};


#define DET_WORK_TIMEOUT_DEFAULT 100
#define DET_WORK_TIMEOUT_DEFERRED 2000
#define MAX_BOUNCE 5


struct reg_value {
	u16 u16RegAddr;
	u32 u32Val;
	u32 u32Mask;
	u8  u8Length;
	u32 u32Delay_ms;
};

struct tc358743_mode_info {
	const char *name;
	enum tc358743_mode mode;
	u32 width;
	u32 height;
	u32 vformat;
	u32 fps;
	u32 lanes;
	u32 freq;
	const struct reg_value *init_data_ptr;
	u32 init_data_size;
	__u32 flags;
};

#define TXOPTIONCNTRL                         0x0238
#define MASK_CONTCLKMODE                      0x00000001

#define VI_MUTE                               0x857F
#define MASK_AUTO_MUTE                        0xc0
#define MASK_VI_MUTE                          0x10

#define CONFCTL                               0x0004
#define MASK_PWRISO                           0x8000
#define MASK_ACLKOPT                          0x1000
#define MASK_AUDCHNUM                         0x0c00
#define MASK_AUDCHNUM_8                       0x0000
#define MASK_AUDCHNUM_6                       0x0400
#define MASK_AUDCHNUM_4                       0x0800
#define MASK_AUDCHNUM_2                       0x0c00
#define MASK_AUDCHSEL                         0x0200
#define MASK_I2SDLYOPT                        0x0100
#define MASK_YCBCRFMT                         0x00c0
#define MASK_YCBCRFMT_444                     0x0000
#define MASK_YCBCRFMT_422_12_BIT              0x0040
#define MASK_YCBCRFMT_COLORBAR                0x0080
#define MASK_YCBCRFMT_422_8_BIT               0x00c0
#define MASK_INFRMEN                          0x0020
#define MASK_AUDOUTSEL                        0x0018
#define MASK_AUDOUTSEL_CSI                    0x0000
#define MASK_AUDOUTSEL_I2S                    0x0010
#define MASK_AUDOUTSEL_TDM                    0x0018
#define MASK_AUTOINDEX                        0x0004
#define MASK_ABUFEN                           0x0002
#define MASK_VBUFEN                           0x0001

/* mipi data type */
#define MIPI_DT_YUV420		0x18 /* YYY.../UYVY.... */
#define MIPI_DT_YUV420_LEGACY	0x1a /* UYY.../VYY...   */
#define MIPI_DT_YUV422		0x1e /* UYVY...         */
#define MIPI_DT_RGB444		0x20
#define MIPI_DT_RGB555		0x21
#define MIPI_DT_RGB565		0x22
#define MIPI_DT_RGB666		0x23
#define MIPI_DT_RGB888		0x24
#define MIPI_DT_RAW6		0x28
#define MIPI_DT_RAW7		0x29
#define MIPI_DT_RAW8		0x2a
#define MIPI_DT_RAW10		0x2b
#define MIPI_DT_RAW12		0x2c
#define MIPI_DT_RAW14		0x2d
