#define OV5640_VOLTAGE_ANALOG               2800000
#define OV5640_VOLTAGE_DIGITAL_CORE         1500000
#define OV5640_VOLTAGE_DIGITAL_IO           1800000

#define MIN_FPS 15
#define MAX_FPS 30
#define DEFAULT_FPS 30

#define MAX_I2C_SIZE	255

#define OV5640_XCLK_MIN 6000000
#define OV5640_XCLK_MAX 24000000
#define OV5640_XCLK_20MHZ 20000000

#define OV5640_CHIP_ID_HIGH_BYTE	0x300A
#define OV5640_CHIP_ID_LOW_BYTE		0x300B

#define OV5640_TIMING_TC_REG20		0x3820
#define OV5640_TIMING_TC_REG20_VFLIP	0x06
#define OV5640_TIMING_TC_REG21		0x3821
#define OV5640_TIMING_TC_REG21_MIRROR	0x06

enum ov5640_mode {
	ov5640_mode_MIN = 0,
	ov5640_mode_VGA_640_480 = 0,
	ov5640_mode_QVGA_320_240 = 1,
	ov5640_mode_NTSC_720_480 = 2,
	ov5640_mode_PAL_720_576 = 3,
	ov5640_mode_720P_1280_720 = 4,
	ov5640_mode_1080P_1920_1080 = 5,
	ov5640_mode_QSXGA_2592_1944 = 6,
	ov5640_mode_QCIF_176_144 = 7,
	ov5640_mode_XGA_1024_768 = 8,
	ov5640_mode_MAX = 8,
	ov5640_mode_INIT = 0xff, /*only for sensor init*/
};

enum ov5640_frame_rate {
	ov5640_15_fps,
	ov5640_30_fps
};

/* image size under 1280 * 960 are SUBSAMPLING
 * image size upper 1280 * 960 are SCALING
 */
enum ov5640_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct reg_value {
	u16 u16RegAddr;
	u8 u8Val;
	u8 u8Mask;
	u32 u32Delay_ms;
};

struct ov5640_mode_info {
	enum ov5640_mode mode;
	enum ov5640_downsize_mode dn_mode;
	u32 width;
	u32 height;
	const struct reg_value *init_data_ptr;
	u32 init_data_size;
};

/* AF-related registers */
#define OV5640_REG_FW_START	0x8000
#define OV5640_REG_SYS_RESET	0x3000
#define OV5640_REG_CLK_EN00	0x3004
#define OV5640_REG_AF_MODE	0x3022
#define OV5640_REG_AF_ACK	0x3023
#define OV5640_REG_AF_PARAM0	0x3024
#define OV5640_REG_AF_PARAM1	0x3025
#define OV5640_REG_AF_PARAM2	0x3026
#define OV5640_REG_AF_PARAM3	0x3027
#define OV5640_REG_AF_RESULT	0x3028
#define OV5640_REG_AF_STATUS	0x3029

#define OV5640_REG_SYS_RESET_MCU	BIT(5)

/* Firmware load timeout (ms) */
#define OV5640_AF_FW_TIMEOUT	1000

/* OV5640 focus area values */
#define OV5640_AF_ZONE_ARRAY_WIDTH	80
#define OV5640_AF_ZONE_ARRAY_HEIGHT	60

/* OV5640_REG_AF_MODE command codes */
#define OV5640_AF_MODE_SINGLE		0x03
#define OV5640_AF_MODE_CONTINUOUS	0x04
#define OV5640_AF_MODE_PAUSE		0x06
#define OV5640_AF_MODE_RELEASE		0x08
#define OV5640_AF_MODE_RELAUNCH		0x80
#define OV5640_AF_MODE_SET_ZONE		0x81

/* OV5640_REG_AF_STATUS values */
#define OV5640_AF_STATUS_DL	0x7f
#define OV5640_AF_STATUS_INIT	0x7e
#define OV5640_AF_STATUS_IDLE	0x70
#define OV5640_AF_STATUS_RUN	0x00
#define OV5640_AF_STATUS_FINISH	0x10

/* OV5640_REG_AF_ACK values */
#define OV5640_AF_ACK_SET	0x01
#define OV5640_AF_ACK_DONE	0x00

struct ov5640_datafmt {
	u32	pixelformat;
	u32	reg4300;
	enum v4l2_colorspace	colorspace;
	const char* name;
	u32	code;
};

struct ov5640 *sensor;
static void OV5640_stream_on(struct ov5640 *sensor);
static void OV5640_stream_off(struct ov5640 *sensor);
static int ov5640_power_off(struct ov5640 *sensor);
static void ov5640_reset(struct ov5640 *sensor);
static int ov5640_dev_init(struct ov5640 *sensor);
