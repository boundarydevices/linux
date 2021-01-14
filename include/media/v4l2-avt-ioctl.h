#ifndef __AVT_CSI2_IOCTL_H
#define __AVT_CSI2_IOCTL_H

#include <linux/videodev2.h>

/* Driver interface version */
#define	MAJOR_DRV_IF        1
#define	MINOR_DRV_IF        0
#define	PATCH_DRV_IF        7

struct v4l2_i2c {
	__u32			reg;
	__u32			timeout;
	const char		*buffer;
	__u32			size;
	__u32			count;
};

struct v4l2_gencp_buffer_sizes
{
    __u32      nGenCPInBufferSize;      // Size in bytes of the GenCP In buffer
    __u32      nGenCPOutBufferSize;     // Size in bytes of the GenCP Out buffer
};

enum v4l2_statistics_capability
{
    V4L2_STATISTICS_CAPABILITY_FrameCount           = 0x1,
    V4L2_STATISTICS_CAPABILITY_PacketCRCError       = 0x2,
    V4L2_STATISTICS_CAPABILITY_FramesUnderrun       = 0x4,
    V4L2_STATISTICS_CAPABILITY_FramesIncomplete     = 0x8,
    V4L2_STATISTICS_CAPABILITY_CurrentFrameCount    = 0x10,
    V4L2_STATISTICS_CAPABILITY_CurrentFrameInterval = 0x20,
};

struct v4l2_statistics_capabilities
{
    __u64 nStatisticsCapability;    // Bitmask with statistics capabilities enum (v4l2_statistics_capability)
};

struct v4l2_stats_t
{
    __u64    FramesCount;           // Total number of frames received
    __u64    PacketCRCError;        // Number of packets with CRC errors
    __u64    FramesUnderrun;        // Number of frames dropped because of buffer underrun
    __u64    FramesIncomplete;      // Number of frames that were not completed
    __u64    CurrentFrameCount;     // Number of frames received within CurrentFrameInterval (nec. to calculate fps value)
    __u64    CurrentFrameInterval;  // Time interval between frames in µs
};

struct v4l2_range
{
    __u8  nValid;               // Indicates, if values are valid (1) or invalid (0)
    __u32 nMin;                 // Minimum allowed value
    __u32 nMax;                 // Maximum allowed value
};

struct v4l2_csi_host_clock_freq_ranges
{
    struct v4l2_range lane_range_1;    // Min and max value for 1 lane
    struct v4l2_range lane_range_2;    // Min and max value for 2 lanes
    struct v4l2_range lane_range_3;    // Min and max value for 3 lanes
    struct v4l2_range lane_range_4;    // Min and max value for 4 lanes
};

enum v4l2_lane_counts
{
    V4L2_LANE_COUNT_1_LaneSupport  = 0x1,
    V4L2_LANE_COUNT_2_LaneSupport  = 0x2,
    V4L2_LANE_COUNT_3_LaneSupport  = 0x4,
    V4L2_LANE_COUNT_4_LaneSupport  = 0x8,
};

struct v4l2_supported_lane_counts
{
    __u32 nSupportedLaneCounts;  // Bitfield with the supported lane counts from v4l2_lane_counts
};

#define FRAMESIZE_MIN_W 32
#define FRAMESIZE_MIN_H 16
#define FRAMESIZE_MAX_W 4096
#define FRAMESIZE_MAX_H 4096
#define FRAMESIZE_INC_W 16
#define FRAMESIZE_INC_H 1

struct v4l2_restriction
{
    __u8 nValid;                // Indicates, if values are valid (1) or invalid (0)
    __u32 nMin;                 // Minimum value
    __u32 nMax;                 // Maximum value
    __u32 nInc;                 // Increment value
};

struct v4l2_ipu_restrictions
{
    struct v4l2_restriction    ipu_x;      // X restrictions
    struct v4l2_restriction    ipu_y;      // Y restrictions
};

// Support only 0x31 datatype
# define DATA_IDENTIFIER_INQ_1 0x0002000000000000ull
# define DATA_IDENTIFIER_INQ_2 0x0
# define DATA_IDENTIFIER_INQ_3 0x0
# define DATA_IDENTIFIER_INQ_4 0x0

struct v4l2_csi_data_identifiers_inq
{
    __u64 nDataIdentifiersInq1;      // Inquiry for data identifiers 0-63
    __u64 nDataIdentifiersInq2;      // Inquiry for data identifiers 64-127
    __u64 nDataIdentifiersInq3;      // Inquiry for data identifiers 128-191
    __u64 nDataIdentifiersInq4;      // Inquiry for data identifiers 192-255
};

#define MIN_ANNOUNCED_FRAMES 3

struct v4l2_min_announced_frames
{
    __u32 nMinAnnouncedFrames;      // Minimum number of announced frames
};

struct v4l2_dma_mem
{

    __u32           index;              // index of the buffer
    __u32           type;               // enum v4l2_buf_type
    __u32           memory;             // enum v4l2_memory
};

struct v4l2_streamon_ex
{
    __u32      nBufferType;         // Buffer type (v4l2_buf_type)
    __u32      nIPU_X;              // IPU X value
    __u32      nIPU_Y;              // IPU Y value
    __u32      nTotalDataSize;      // Total data size in bytes
    __u8       nDataIdentifier;     // Data identifier according to MIPI spec (Bit 0..5=DataType. Bit 6..7=VirtualChannel)
    __u8       nLaneCount;          // Lane count as negotiated with camera
    __u32      nLaneClockFrequency; // CSI-2 lane clock frequency in Hz.
};

struct v4l2_streamoff_ex
{
    __u32 nBufferType;              // Buffer type (v4l2_buf_type)
    __u32 timeout;                  // Timeout value in ms
};

/* Available driver capability flags */
#define AVT_DRVCAP_USRPTR   0x00000001
#define AVT_DRVCAP_MMAP     0x00000002

struct v4l2_csi_driver_info
{
    union _id
    {
        __u32 board_id;
        struct
        {
            __u8 manufacturer_id;
            __u8 soc_family_id;
            __u8 driver_id;
            __u8 reserved;
        };
    }id;

    __u32 driver_version;               // Driver version
    __u32 driver_interface_version;     // Used driver specification version
    __u32 driver_caps;                  // Driver capabilities flags
    __u32 usrptr_alignment;             // Buffer alignment for user pointer mode in bytes
};

enum manufacturer_id
{
    MANUFACTURER_ID_NXP         = 0x00,
    MANUFACTURER_ID_NVIDIA      = 0x01,
};

enum soc_family_id
{
    SOC_FAMILY_ID_IMX6          = 0x00,
    SOC_FAMILY_ID_TEGRA         = 0x01,
    SOC_FAMILY_ID_IMX8          = 0x02,
    SOC_FAMILY_ID_IMX8M         = 0x03,
    SOC_FAMILY_ID_IMX8X         = 0x04,
};

enum imx6_driver_id
{
    IMX6_DRIVER_ID_NITROGEN     = 0x00,
    IMX6_DRIVER_ID_WANDBOARD    = 0x01,
};

enum tegra_driver_id
{
    TEGRA_DRIVER_ID_DEFAULT     = 0x00,
};

enum imx8_driver_id
{
    IMX8_DRIVER_ID_DEFAULT      = 0x00,
};

enum imx8m_driver_id
{
    IMX8M_DRIVER_ID_DEFAULT     = 0x00,
};

enum imx8x_driver_id
{
    IMX8X_DRIVER_ID_DEFAULT     = 0x00,
};

struct v4l2_csi_config
{
    __u8 lane_count;
    __u32 csi_clock;
};

#define VIDIOC_R_I2C                        _IOWR('V', BASE_VIDIOC_PRIVATE + 0,  struct v4l2_i2c)
#define VIDIOC_W_I2C                        _IOWR('V', BASE_VIDIOC_PRIVATE + 1,  struct v4l2_i2c)
#define VIDIOC_MEM_ALLOC                    _IOWR('V', BASE_VIDIOC_PRIVATE + 2,  struct v4l2_dma_mem)
#define VIDIOC_MEM_FREE                     _IOWR('V', BASE_VIDIOC_PRIVATE + 3,  struct v4l2_dma_mem)
#define VIDIOC_FLUSH_FRAMES                 _IO('V', BASE_VIDIOC_PRIVATE + 4)
#define VIDIOC_STREAMSTAT                   _IOR('V', BASE_VIDIOC_PRIVATE + 5,  struct v4l2_stats_t)
#define VIDIOC_RESET_STREAMSTAT             _IO('V', BASE_VIDIOC_PRIVATE + 6)
#define VIDIOC_STREAMON_EX                  _IOWR('V', BASE_VIDIOC_PRIVATE + 7,  struct v4l2_streamon_ex)
#define VIDIOC_STREAMOFF_EX                 _IOWR('V', BASE_VIDIOC_PRIVATE + 8,  struct v4l2_streamoff_ex)
#define VIDIOC_G_STATISTIC_CAPABILITIES     _IOR('V', BASE_VIDIOC_PRIVATE + 9,  struct v4l2_statistics_capabilities)
#define VIDIOC_G_MIN_ANNOUNCED_FRAMES       _IOR('V', BASE_VIDIOC_PRIVATE + 10, struct v4l2_min_announced_frames)
#define VIDIOC_G_SUPPORTED_LANE_COUNTS      _IOR('V', BASE_VIDIOC_PRIVATE + 11, struct v4l2_supported_lane_counts)
#define VIDIOC_G_CSI_HOST_CLK_FREQ          _IOR('V', BASE_VIDIOC_PRIVATE + 12, struct v4l2_csi_host_clock_freq_ranges)
#define VIDIOC_G_IPU_RESTRICTIONS           _IOR('V', BASE_VIDIOC_PRIVATE + 13, struct v4l2_ipu_restrictions)
#define VIDIOC_G_GENCP_BUFFER_SIZES         _IOWR('V', BASE_VIDIOC_PRIVATE + 14, struct v4l2_gencp_buffer_sizes)
#define VIDIOC_G_SUPPORTED_DATA_IDENTIFIERS _IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct v4l2_csi_data_identifiers_inq)
#define VIDIOC_G_I2C_CLOCK_FREQ             _IOWR('V', BASE_VIDIOC_PRIVATE + 16, int)
#define VIDIOC_G_DRIVER_INFO                _IOR('V', BASE_VIDIOC_PRIVATE + 17, struct v4l2_csi_driver_info)
#define VIDIOC_G_CSI_CONFIG                 _IOR('V', BASE_VIDIOC_PRIVATE + 18, struct v4l2_csi_config)
#define VIDIOC_S_CSI_CONFIG                 _IOWR('V', BASE_VIDIOC_PRIVATE + 19, struct v4l2_csi_config)

#endif  /* __AVT_CSI2_IOCTL_H */


