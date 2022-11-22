/*
 * Allied Vision CSI2 Camera
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/lcm.h>
#include <linux/crc32.h>
#include <linux/dma-mapping.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-avt-ioctl.h>

#include "avt_csi2.h"

static int debug;
MODULE_PARM_DESC(debug, "debug");
module_param(debug, int, 0600);/* S_IRUGO */

#define avt_dbg(lvl, dev, fmt, args...) \
		v4l2_dbg(lvl, debug, dev, "%s:%d: " fmt "", \
				__func__, __LINE__, ##args) \

#define avt_err(dev, fmt, args...) \
		v4l2_err(dev, "%s:%d: " fmt "", __func__, __LINE__, ##args) \

#define avt_info(dev, fmt, args...) \
		v4l2_info(dev, "%s:%d: " fmt "", __func__, __LINE__, ##args) \

#define DEFAULT_FPS 60

#define AV_CAM_DEFAULT_FMT	MEDIA_BUS_FMT_VYUY8_2X8

#define I2C_IO_LIMIT	256
#define BCRM_WAIT_HANDSHAKE_TIMEOUT	3000

static int avt_set_selection(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_selection *sel);

static int avt_reg_read(struct i2c_client *client, __u32 reg,
		__u32 reg_size, __u32 count, char *buffer);

static int avt_init_mode(struct v4l2_subdev *sd);

static int avt_init_frame_param(struct avt_csi2_priv *priv);

static bool common_range(uint32_t nMin1, uint32_t nMax1, uint32_t nInc1,
				uint32_t nMin2, uint32_t nMax2, uint32_t nInc2,
				uint32_t *rMin, uint32_t *rMax, uint32_t *rInc);

static void swapbytes(void *_object, size_t _size)
{
	switch (_size) {
	case 2:
		cpu_to_be16s((uint16_t *)_object);
		break;
	case 4:
		cpu_to_be32s((uint32_t *)_object);
		break;
	case 8:
		cpu_to_be64s((uint64_t *)_object);
		break;
	}
}

static struct avt_csi2_priv *to_avt_csi2(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct avt_csi2_priv, subdev);
}

static uint32_t i2c_read(struct i2c_client *client, uint32_t reg,
			uint32_t size, uint32_t count, char *buf)
{
	struct i2c_msg msg[2];
	u8 msgbuf[8];
	int ret = 0, i = 0, j = 0, reg_size_bkp;

	reg_size_bkp = size;

	/* clearing i2c msg with 0's */
	memset(msg, 0, sizeof(msg));

	if (count > I2C_IO_LIMIT) {
		dev_err(&client->dev, "Limit excedded! i2c_reg->count = %d > I2C_IO_LIMIT = %d\n", count, I2C_IO_LIMIT);
		count = I2C_IO_LIMIT;
	}

	/* find start address of buffer */
	for (i = --size; i >= 0; i--, j++)
		msgbuf[i] = ((reg >> (8*j)) & 0xFF);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = reg_size_bkp;
	msg[0].buf = msgbuf;
	msg[1].addr = client->addr; /* slave address */
	msg[1].flags = I2C_M_RD; /* read flag setting */
	msg[1].len = count;
	msg[1].buf = buf; /* dest buffer */

	ret = i2c_transfer(client->adapter, msg, 2);

	return ret;
}

static bool bcrm_get_write_handshake_availibility(struct i2c_client *client)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	u8 value = 0;
	int status;

	/* Reading the device firmware version from camera */
	status = avt_reg_read(client,
					priv->cci_reg.bcrm_addr +
					WRITE_HANDSHAKE_REG_8RW,
				 	AV_CAM_REG_SIZE,
					AV_CAM_DATA_SIZE_8,
					(char *) &value);

	if ((status >= 0) && (value & 0x80)) {
		dev_dbg(&client->dev, "BCRM write handshake supported!");
		return true;
	} else {
		dev_warn(&client->dev, "BCRM write handshake NOT supported!");
		return false;
	}
}

/**
 * @brief Since the camera needs a few ms to process written data, we need to poll
   the handshake register to make sure to continue not too early with the next write access.
 *
 * @param timeout_ms : Timeout value in ms
 * @return uint64_t : Duration in ms
 */
static uint64_t wait_for_bcrm_write_handshake(struct i2c_client *client, uint64_t timeout_ms)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	static const int poll_interval_ms = 10;
	static const int default_wait_time_ms = 50;
	int status = 0;
	u8 buffer[3] = {0};
	u8 handshake_val = 0;
	bool handshake_valid = false;
	struct timespec64 tstart;
	struct timespec64 tend;
	uint64_t start_time_ms = 0;
	uint64_t duration_ms = 0;

	ktime_get_real_ts64(&tstart);
	start_time_ms = (tstart.tv_sec * (uint64_t)1000) + (tstart.tv_nsec / 1000000);

	if (priv->write_handshake_available) {
		/* We need to poll the handshake register and wait until the camera has processed the data */
		avt_dbg(3, sd, " Wait for 'write done' bit (0x81)...\n");
		do {
			msleep(poll_interval_ms);
			/* Read handshake register */
			status = avt_reg_read(client,
					priv->cci_reg.bcrm_addr +
					WRITE_HANDSHAKE_REG_8RW,
					AV_CAM_REG_SIZE,
					AV_CAM_DATA_SIZE_8,
				(char *)&handshake_val);

			ktime_get_real_ts64(&tend);
			duration_ms = ((tend.tv_sec * (uint64_t)1000) + (tend.tv_nsec / 1000000)) - start_time_ms;

			if (status >= 0) {
				/* Check, if handshake bit is set */
				if ((handshake_val & 0x01) == 1) {
					/* Handshake set by camera. We should to reset it */
					buffer[0] = (priv->cci_reg.bcrm_addr + WRITE_HANDSHAKE_REG_8RW) >> 8;
					buffer[1] = (priv->cci_reg.bcrm_addr + WRITE_HANDSHAKE_REG_8RW) & 0xff;
					buffer[2] = (handshake_val & 0xFE);	/* Reset LSB (handshake bit)*/
					status = i2c_master_send(client, buffer, sizeof(buffer));

					/* Since the camera needs a few ms for every write access to finish, we need to poll here too */
					avt_dbg(3, sd, " Wait for reset of 'write done' bit (0x80)...\n");
					do{
						msleep(poll_interval_ms);
						/* We need to wait again until the bit is reset */
						status = avt_reg_read(client,
								priv->cci_reg.bcrm_addr +
								WRITE_HANDSHAKE_REG_8RW,
				 				AV_CAM_REG_SIZE,
								AV_CAM_DATA_SIZE_8,
								(char *) &handshake_val);

						ktime_get_real_ts64(&tend);
						duration_ms = ((tend.tv_sec * (uint64_t)1000) + (tend.tv_nsec / 1000000)) - start_time_ms;

						if (status >= 0) {
							if ((handshake_val & 0x01) == 0)	/* Verify write */
							{
								handshake_valid = true;
								break;
							}
						} else {
							dev_err(&client->dev, " Error while reading WRITE_HANDSHAKE_REG_8RW register.");
							break;
						}
					}
					while (duration_ms <= timeout_ms);

					break;
				}
			} else {
				dev_err(&client->dev, " Error while reading WRITE_HANDSHAKE_REG_8RW register.");
				break;
			}
		}
		while (duration_ms <= timeout_ms);

		if (!handshake_valid) {
			dev_warn(&client->dev, " Write handshake timeout!");
		}
	} else {
		/* Handshake not supported. Use static sleep at least once as fallback */
		msleep(default_wait_time_ms);
		duration_ms = (uint64_t)default_wait_time_ms;
	}

	return duration_ms;
}

static int avt_reg_read(struct i2c_client *client, __u32 reg,
			__u32 reg_size, __u32 count, char *buffer)
{
	int ret;

	ret = i2c_read(client, reg, reg_size, count, buffer);
	if (ret < 0)
		return ret;

	swapbytes(buffer, count);

	return ret;
}

static int avt_reg_write(struct i2c_client *client, u16 reg, u8 val)
{
	int ret = 0;
	u8 au8Buf[3] = {0};
	uint64_t duration = 0;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	ret = i2c_master_send(client, au8Buf, 3);
	if (ret < 0)
		dev_err(&client->dev, "%s, i2c write failed reg=%x,val=%x error=%d\n",
			__func__, reg, val, ret);

	duration = wait_for_bcrm_write_handshake(client,
				BCRM_WAIT_HANDSHAKE_TIMEOUT);
	avt_dbg(3, sd, "i2c write success reg=0x%x, duration=%lldms, ret=%d\n", reg, duration, ret);

	return ret;
}

static struct avt_csi2_priv *avt_get_priv(struct v4l2_subdev *sd)
{
	struct i2c_client *client;

	if (sd == NULL)
		return NULL;

	client = v4l2_get_subdevdata(sd);
	if (client == NULL)
		return NULL;

	return to_avt_csi2(client);
}

static struct v4l2_ctrl *avt_get_control(struct v4l2_subdev *sd, u32 id)
{
	int i;
	struct avt_csi2_priv *priv = avt_get_priv(sd);

	for (i = 0; i < AVT_MAX_CTRLS; i++) {
		if (priv->ctrls[i] == NULL)
			continue;
		if (priv->ctrls[i]->id == id)
			return priv->ctrls[i];
	}

	return NULL;
}

static int ioctl_gencam_i2cwrite_reg(struct i2c_client *client, uint32_t reg,
		uint32_t size, uint32_t count, const char *buf)
{
	int j = 0, i = 0;
	char *i2c_w_buf;
	int ret = 0;
	uint64_t duration = 0;
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	avt_dbg(3, sd, "i2c attempt write reg=0x%x\n", reg);

	/* count exceeds writting IO_LIMIT characters */
	if (count > I2C_IO_LIMIT) {
		dev_err(&client->dev, "limit excedded! i2c_reg->count > I2C_IO_LIMIT\n");
		count = I2C_IO_LIMIT;
	}

	i2c_w_buf = kzalloc(count + size, GFP_KERNEL);
	if (!i2c_w_buf)
		return -ENOMEM;

	/* Fill the address in buffer upto size of address want to write */
	for (i = size - 1, j = 0; i >= 0; i--, j++)
		i2c_w_buf[i] = ((reg >> (8 * j)) & 0xFF);

	/* Append the data value in the same buffer */
	memcpy(i2c_w_buf + size, buf, count);

	ret = i2c_master_send(client, i2c_w_buf, count + size);

	if (ret < 0)
		dev_err(&client->dev, "%s:%d: i2c write failed ret %d\n",
				__func__, __LINE__, ret);


	/* Wait for write handshake register only for BCM registers */
	if ((reg >= priv->cci_reg.bcrm_addr) && (reg <= priv->cci_reg.bcrm_addr + _BCRM_LAST_ADDR)) {
		duration = wait_for_bcrm_write_handshake(client,
					BCRM_WAIT_HANDSHAKE_TIMEOUT);
		avt_dbg(3, sd, "i2c write success reg=0x%x, duration=%lldms, ret=%d\n", reg, duration, ret);
	}

	kfree(i2c_w_buf);

	return ret;
}

static int ioctl_bcrm_i2cwrite_reg(struct i2c_client *client,
		struct v4l2_ext_control *vc,
		unsigned int reg,
		int length)
{
	uint32_t i2c_reg;
	uint32_t i2c_reg_size;
	uint32_t i2c_reg_count;
	char *i2c_reg_buf;

	ssize_t ret;
	__u64 temp = 0;

	if (length > AV_CAM_DATA_SIZE_32) {
		temp = vc->value;
		swapbytes(&temp, length);
	} else
		swapbytes(&vc->value, length);

	i2c_reg = reg;
	i2c_reg_size = AV_CAM_REG_SIZE;
	i2c_reg_count = length;

	if (length > AV_CAM_DATA_SIZE_32)
		i2c_reg_buf = (char *) &temp;
	else
		i2c_reg_buf = (char *) &vc->value;

	ret = ioctl_gencam_i2cwrite_reg(client, i2c_reg, i2c_reg_size,
			i2c_reg_count, i2c_reg_buf);

	if (ret < 0)
		dev_err(&client->dev, "%s:%d i2c write failed\n",
				__func__, __LINE__);

	return ret;
}

static int set_bayer_format(struct i2c_client *client, __u8 value)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	int ret = 0;

	uint32_t i2c_reg;
	uint32_t i2c_reg_size;
	uint32_t i2c_reg_count;

	char *i2c_reg_buf;

	CLEAR(i2c_reg);
	i2c_reg = priv->cci_reg.bcrm_addr + IMG_BAYER_PATTERN_8RW;
	i2c_reg_size = AV_CAM_REG_SIZE;
	i2c_reg_count = AV_CAM_DATA_SIZE_8;
	i2c_reg_buf = (char *) &value;

	ret = ioctl_gencam_i2cwrite_reg(client, i2c_reg, i2c_reg_size,
					i2c_reg_count, i2c_reg_buf);

	if (ret < 0) {
		dev_err(&client->dev, "%s:%d i2c write failed\n",
				__func__, __LINE__);
		return ret;
	}

	return 0;
}

static bool avt_check_fmt_available(struct i2c_client *client, u32 media_bus_fmt)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	u64 avail_mipi = 0;
	unsigned char bayer_val = 0;
	union bcrm_avail_mipi_reg feature_inquiry_reg;
	union bcrm_bayer_inquiry_reg bayer_inquiry_reg;
	int ret;

	/* read the MIPI format register to check whether the camera
	 * really support the requested pixelformat format
	 */
	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr +
			IMG_AVAILABLE_MIPI_DATA_FORMATS_64R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
			(char *) &avail_mipi);

	if (ret < 0) {
		dev_err(&client->dev, "i2c read failed (%d)\n", ret);
		return false;
	}

	feature_inquiry_reg.value = avail_mipi;

	/* read the Bayer Inquiry register to check whether the camera
	 * really support the requested RAW format
	 */
	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr +
			IMG_BAYER_PATTERN_INQUIRY_8R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
			(char *) &bayer_val);

	if (ret < 0) {
		dev_err(&client->dev, "i2c read failed (%d)\n", ret);
		return false;
	}

	bayer_inquiry_reg.value = bayer_val;

	switch (media_bus_fmt) {
	case MEDIA_BUS_FMT_RGB444_1X12:
		return feature_inquiry_reg.avail_mipi.rgb444_avail;
	case MEDIA_BUS_FMT_RGB565_1X16:
		return feature_inquiry_reg.avail_mipi.rgb565_avail;
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_BGR888_1X24:
		return feature_inquiry_reg.avail_mipi.rgb888_avail;
	case MEDIA_BUS_FMT_VYUY8_2X8:
		return feature_inquiry_reg.avail_mipi.yuv422_8_avail;
	/* RAW 8 */
	case MEDIA_BUS_FMT_Y8_1X8:
		return feature_inquiry_reg.avail_mipi.raw8_avail &&
			bayer_inquiry_reg.bayer_pattern.monochrome_avail;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
		return feature_inquiry_reg.avail_mipi.raw8_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_BG_avail;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
		return feature_inquiry_reg.avail_mipi.raw8_avail
			&& bayer_inquiry_reg.bayer_pattern.bayer_GB_avail;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
		return feature_inquiry_reg.avail_mipi.raw8_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_GR_avail;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return feature_inquiry_reg.avail_mipi.raw8_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_RG_avail;
	/* RAW 10 */
	case MEDIA_BUS_FMT_Y10_1X10:
		return feature_inquiry_reg.avail_mipi.raw10_avail &&
			bayer_inquiry_reg.bayer_pattern.monochrome_avail;
	case MEDIA_BUS_FMT_SGBRG10_1X10:
		return feature_inquiry_reg.avail_mipi.raw10_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_GB_avail;
	case MEDIA_BUS_FMT_SGRBG10_1X10:
		return feature_inquiry_reg.avail_mipi.raw10_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_GR_avail;
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return feature_inquiry_reg.avail_mipi.raw10_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_RG_avail;
	/* RAW 12 */
	case MEDIA_BUS_FMT_Y12_1X12:
		return feature_inquiry_reg.avail_mipi.raw12_avail &&
			bayer_inquiry_reg.bayer_pattern.monochrome_avail;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
		return feature_inquiry_reg.avail_mipi.raw12_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_BG_avail;
	case MEDIA_BUS_FMT_SGBRG12_1X12:
		return feature_inquiry_reg.avail_mipi.raw12_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_GB_avail;
	case MEDIA_BUS_FMT_SGRBG12_1X12:
		return feature_inquiry_reg.avail_mipi.raw12_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_GR_avail;
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return feature_inquiry_reg.avail_mipi.raw12_avail &&
			bayer_inquiry_reg.bayer_pattern.bayer_RG_avail;
	}

	return false;
}

static int avt_ctrl_send(struct i2c_client *client,
		struct avt_ctrl *vc)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	int ret = 0;
	unsigned int reg = 0;
	int length = 0;

	uint32_t i2c_reg;
	uint32_t i2c_reg_size;
	uint32_t i2c_reg_count;

	char *i2c_reg_buf;

	//void *mipi_csi2_info;
	int r_wn = 0;/* write -> r_wn = 0, read -> r_wn = 1 */
	u64 avail_mipi = 0;
	union bcrm_avail_mipi_reg feature_inquiry_reg;
	union bcrm_bayer_inquiry_reg bayer_inquiry_reg;
	unsigned char bayer_val = 0;
	u64 temp = 0;
	int gencp_mode_local = 0;/* Default BCRM mode */
	__u8 bayer_temp = 0;

	bayer_inquiry_reg.value = 0;
	feature_inquiry_reg.value = 0;

	if (vc->id == V4L2_AV_CSI2_PIXELFORMAT_W) {

		/* read the MIPI format register to check whether the camera
		 * really support the requested pixelformat format
		 */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr +
				IMG_AVAILABLE_MIPI_DATA_FORMATS_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &avail_mipi);

		if (ret < 0)
			dev_err(&client->dev, "i2c read failed (%d)\n", ret);

		feature_inquiry_reg.value = avail_mipi;

		if (avail_mipi == 0) {
			// No pixel formats -> Probablyfallback app runnning
			// Fake it
			dev_dbg(&client->dev, " No pixelformats available. Fallback app running?");
			avail_mipi = 1 << 7; // RGB888
		}

		dev_dbg(&client->dev, "Feature Inquiry Reg value : 0x%llx\n",
				avail_mipi);

		/* read the Bayer Inquiry register to check whether the camera
		 * really support the requested RAW format
		 */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr +
				IMG_BAYER_PATTERN_INQUIRY_8R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
				(char *) &bayer_val);

		if (ret < 0)
			dev_err(&client->dev, "i2c read failed (%d)\n", ret);

		dev_dbg(&client->dev, "Bayer Inquiry Reg value : 0x%x\n",
				bayer_val);

		bayer_inquiry_reg.value = bayer_val;
	}

	switch (vc->id) {
	case V4L2_AV_CSI2_STREAMON_W:
		reg = ACQUISITION_START_8RW;
		length = AV_CAM_DATA_SIZE_8;
		r_wn = 0;
		break;
	case V4L2_AV_CSI2_STREAMOFF_W:
		reg = ACQUISITION_STOP_8RW;
		length = AV_CAM_DATA_SIZE_8;
		r_wn = 0;
		break;
	case V4L2_AV_CSI2_ABORT_W:
		reg = ACQUISITION_ABORT_8RW;
		length = AV_CAM_DATA_SIZE_8;
		r_wn = 0;
		break;
	case V4L2_AV_CSI2_WIDTH_W:
		reg = IMG_WIDTH_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 0;
		break;
	case V4L2_AV_CSI2_HEIGHT_W:
		reg = IMG_HEIGHT_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 0;
		break;
	case V4L2_AV_CSI2_OFFSET_X_W:
		reg = IMG_OFFSET_X_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 0;
		break;
	case V4L2_AV_CSI2_OFFSET_Y_W:
		reg = IMG_OFFSET_Y_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 0;
		break;
	case V4L2_AV_CSI2_HFLIP_W:
		reg = IMG_REVERSE_X_8RW;
		length = AV_CAM_DATA_SIZE_8;
		r_wn = 0;
		break;
	case V4L2_AV_CSI2_VFLIP_W:
		reg = IMG_REVERSE_Y_8RW;
		length = AV_CAM_DATA_SIZE_8;
		r_wn = 0;
		break;
	case V4L2_AV_CSI2_PIXELFORMAT_W:
		reg = IMG_MIPI_DATA_FORMAT_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 0;

		if (!avt_check_fmt_available(client, vc->value0)) {
			dev_err(&client->dev, "format 0x%x not supported\n",
				vc->value0);
			return -EINVAL;
		}

		switch (vc->value0) {
		case MEDIA_BUS_FMT_RGB444_1X12:
			vc->value0 = MIPI_DT_RGB444;
			break;
		case MEDIA_BUS_FMT_RGB565_1X16:
			vc->value0 = MIPI_DT_RGB565;
			break;
		case MEDIA_BUS_FMT_RGB888_1X24:
		case MEDIA_BUS_FMT_BGR888_1X24:
			vc->value0 = MIPI_DT_RGB888;
			break;
		case MEDIA_BUS_FMT_VYUY8_2X8:
			vc->value0 = MIPI_DT_YUV422;
			break;
		/* RAW 8 */
		case MEDIA_BUS_FMT_Y8_1X8:
			vc->value0 = MIPI_DT_RAW8;
			bayer_temp = monochrome;
			break;
		case MEDIA_BUS_FMT_SBGGR8_1X8:
			vc->value0 = MIPI_DT_RAW8;
			bayer_temp = bayer_bg;
			break;
		case MEDIA_BUS_FMT_SGBRG8_1X8:
			vc->value0 = MIPI_DT_RAW8;
			bayer_temp = bayer_gb;
			break;
		case MEDIA_BUS_FMT_SGRBG8_1X8:
			vc->value0 = MIPI_DT_RAW8;
			bayer_temp = bayer_gr;
			break;
		case MEDIA_BUS_FMT_SRGGB8_1X8:
			vc->value0 = MIPI_DT_RAW8;
			bayer_temp = bayer_rg;
			break;
		/* RAW 10 */
		case MEDIA_BUS_FMT_Y10_1X10:
			vc->value0 = MIPI_DT_RAW10;
			bayer_temp = monochrome;
			break;
		case MEDIA_BUS_FMT_SGBRG10_1X10:
			vc->value0 = MIPI_DT_RAW10;
			bayer_temp = bayer_gb;
			break;
		case MEDIA_BUS_FMT_SGRBG10_1X10:
			vc->value0 = MIPI_DT_RAW10;
			bayer_temp = bayer_gr;
			break;
		case MEDIA_BUS_FMT_SRGGB10_1X10:
			vc->value0 = MIPI_DT_RAW10;
			bayer_temp = bayer_rg;
			break;
		/* RAW 12 */
		case MEDIA_BUS_FMT_Y12_1X12:
			vc->value0 = MIPI_DT_RAW12;
			bayer_temp = monochrome;
			break;
		case MEDIA_BUS_FMT_SBGGR12_1X12:
			vc->value0 = MIPI_DT_RAW12;
			bayer_temp = bayer_bg;
			break;
		case MEDIA_BUS_FMT_SGBRG12_1X12:
			vc->value0 = MIPI_DT_RAW12;
			bayer_temp = bayer_gb;
			break;
		case MEDIA_BUS_FMT_SGRBG12_1X12:
			vc->value0 = MIPI_DT_RAW12;
			bayer_temp = bayer_gr;
			break;
		case MEDIA_BUS_FMT_SRGGB12_1X12:
			vc->value0 = MIPI_DT_RAW12;
			bayer_temp = bayer_rg;
			break;

		default:
			dev_err(&client->dev, "%s: format 0x%x not supported by the host\n",
					__func__, vc->value0);
			return -EINVAL;
		}
		break;
	case V4L2_AV_CSI2_WIDTH_R:
		reg = IMG_WIDTH_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_WIDTH_MINVAL_R:
		reg = IMG_WIDTH_MIN_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_WIDTH_MAXVAL_R:
		reg = IMG_WIDTH_MAX_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_WIDTH_INCVAL_R:
		reg = IMG_WIDTH_INCREMENT_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_HEIGHT_R:
		reg = IMG_HEIGHT_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_HEIGHT_MINVAL_R:
		reg = IMG_HEIGHT_MIN_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_HEIGHT_MAXVAL_R:
		reg = IMG_HEIGHT_MAX_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_HEIGHT_INCVAL_R:
		reg = IMG_HEIGHT_INCREMENT_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_OFFSET_X_R:
		reg = IMG_OFFSET_X_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_OFFSET_X_MIN_R:
		reg = IMG_OFFSET_X_MIN_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_OFFSET_X_MAX_R:
		reg = IMG_OFFSET_X_MAX_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_OFFSET_X_INC_R:
		reg = IMG_OFFSET_X_INCREMENT_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_OFFSET_Y_R:
		reg = IMG_OFFSET_Y_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_OFFSET_Y_MIN_R:
		reg = IMG_OFFSET_Y_MIN_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_OFFSET_Y_MAX_R:
		reg = IMG_OFFSET_Y_MAX_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_OFFSET_Y_INC_R:
		reg = IMG_OFFSET_Y_INCREMENT_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_SENSOR_WIDTH_R:
		reg = SENSOR_WIDTH_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_SENSOR_HEIGHT_R:
		reg = SENSOR_HEIGHT_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_MAX_WIDTH_R:
		reg = WIDTH_MAX_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_MAX_HEIGHT_R:
		reg = HEIGHT_MAX_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_PIXELFORMAT_R:
		reg = IMG_MIPI_DATA_FORMAT_32RW;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_PALYLOADSIZE_R:
		reg = BUFFER_SIZE_32R;
		length = AV_CAM_DATA_SIZE_32;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_ACQ_STATUS_R:
		reg = ACQUISITION_STATUS_8R;
		length = AV_CAM_DATA_SIZE_8;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_HFLIP_R:
		reg = IMG_REVERSE_X_8RW;
		length = AV_CAM_DATA_SIZE_8;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_VFLIP_R:
		reg = IMG_REVERSE_Y_8RW;
		length = AV_CAM_DATA_SIZE_8;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_CURRENTMODE_R:
		reg = GENCP_CURRENTMODE_8R;
		length = AV_CAM_DATA_SIZE_8;
		gencp_mode_local = 1;
		r_wn = 1;
		break;
	case V4L2_AV_CSI2_CHANGEMODE_W:
		reg = GENCP_CHANGEMODE_8W;
		length = AV_CAM_DATA_SIZE_8;
		gencp_mode_local = 1;
		if (vc->value0 == 1)
			priv->mode = AVT_GENCP_MODE;
		else
			priv->mode = AVT_BCRM_MODE;
		r_wn = 0;
		break;
	default:
		dev_err(&client->dev, "%s: unknown ctrl 0x%x\n",
				__func__, vc->id);
		return -EINVAL;
	}

	if (r_wn) {/* read (r_wn=1) */

		if (gencp_mode_local) {

			ret = avt_reg_read(client,
					reg, AV_CAM_REG_SIZE, length,
					(char *) &vc->value0);

			if (ret < 0) {
				dev_err(&client->dev, "i2c read failed (%d)\n",
						ret);
				return ret;
			}
			return 0;
		}

		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + reg,
				AV_CAM_REG_SIZE, length,
				(char *) &vc->value0);

		if (ret < 0) {
			dev_err(&client->dev, "i2c read failed (%d)\n", ret);
			return ret;
		}

		if (vc->id == V4L2_AV_CSI2_PIXELFORMAT_R) {
			/* To avoid ambiguity, resulting from
			 * two MBUS formats linked with
			 * one camera image format,
			 * we return value stored in private data
			 */
			vc->value0 = priv->mbus_fmt_code;
		}

		return 0;

	} else {/* write (r_wn=0) */
		dev_dbg(&client->dev, "reg %x, length %d, vc->value0 0x%x\n",
				reg, length, vc->value0);

		if (gencp_mode_local) {

			i2c_reg = reg;
			i2c_reg_size = AV_CAM_REG_SIZE;
			i2c_reg_count = length;

			if (length > AV_CAM_DATA_SIZE_32)
				i2c_reg_buf = (char *) &temp;
			else
				i2c_reg_buf = (char *) &vc->value0;

			ret = ioctl_gencam_i2cwrite_reg(client,
					i2c_reg, i2c_reg_size,
					i2c_reg_count, i2c_reg_buf);

			if (ret < 0) {
				dev_err(&client->dev, "%s:%d i2c write failed\n",
						__func__, __LINE__);
				return ret;
			} else {
				return 0;
			}
		}

		if (vc->id == V4L2_AV_CSI2_PIXELFORMAT_W) {
			/* Set pixelformat then set bayer format, refer OCT-2417
			 *
			 * XXX implement these somehow, below imx6 code:
			 * mipi_csi2_info = mipi_csi2_get_info();
			 * mipi_csi2_set_datatype(mipi_csi2_info, vc->value0);
			 */
		}

		temp = vc->value0;

		if (length > AV_CAM_DATA_SIZE_32)
			swapbytes(&temp, length);
		else
			swapbytes(&vc->value0, length);

		i2c_reg = priv->cci_reg.bcrm_addr + reg;
		i2c_reg_size = AV_CAM_REG_SIZE;
		i2c_reg_count = length;

		if (length > AV_CAM_DATA_SIZE_32)
			i2c_reg_buf = (char *) &temp;
		else
			i2c_reg_buf = (char *) &vc->value0;

		ret = ioctl_gencam_i2cwrite_reg(client, i2c_reg, i2c_reg_size,
						i2c_reg_count, i2c_reg_buf);

		if (ret < 0) {
			dev_err(&client->dev, "%s:%d i2c write failed\n",
					__func__, __LINE__);
			return ret;
		}

		/* Set pixelformat then set bayer format, refer OCT-2417 */
		if (vc->id == V4L2_AV_CSI2_PIXELFORMAT_W) {
			ret = set_bayer_format(client, bayer_temp);
			if (ret < 0) {
				dev_err(&client->dev, "%s:%d i2c write failed, ret %d\n",
						__func__, __LINE__, ret);
				return ret;
			}
		}

		return 0;
	}
}

/* --------------- INIT --------------- */

static int avt_csi2_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
		struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subdev_subscribe(sd, fh, sub);
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(sd, fh, sub);
	default:
		return -EINVAL;
	}
}

/* --------------- CUSTOM IOCTLS --------------- */

long avt_csi2_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = -ENOTTY;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	struct v4l2_i2c *i2c_reg;
	struct v4l2_csi_driver_info *info;
	struct v4l2_csi_config *config;
	uint32_t i2c_reg_address;
	uint32_t i2c_reg_size;
	uint32_t i2c_reg_count;
	uint32_t clk;
	char *i2c_reg_buf;
	int *i2c_clk_freq;
	struct v4l2_gencp_buffer_sizes *gencp_buf_sz;
	uint8_t avt_supported_lane_counts = 0;
	uint32_t avt_min_clk = 0;
	uint32_t avt_max_clk = 0;
	uint32_t common_min_clk = 0;
	uint32_t common_max_clk = 0;
	uint32_t common_inc_clk = 0;

	avt_dbg(2, sd, "()\n");

	switch(cmd) {
	case VIDIOC_R_I2C:
		i2c_reg = (struct v4l2_i2c *)arg;
		i2c_reg_buf = kzalloc(i2c_reg->count, GFP_KERNEL);
		if (!i2c_reg_buf)
			return -ENOMEM;

		ret = i2c_read(client, i2c_reg->reg, i2c_reg->size,
				i2c_reg->count, i2c_reg_buf);

		if (ret < 0)
			avt_err(sd, "i2c read failed (%d), bytes read = %d\n", ret, i2c_reg->count);
		else {
			ret = copy_to_user((char *)i2c_reg->buffer, i2c_reg_buf, i2c_reg->count);
			if (ret == 0)
				avt_dbg(3, sd, "I2C read success. ret=%d\n", ret);
			else
				avt_err(sd, "I2C read failed. copy_to_user failed. ret=%d\n", ret);
		}

		kfree(i2c_reg_buf);
		break;

	case VIDIOC_W_I2C:
		i2c_reg = (struct v4l2_i2c *)arg;

		i2c_reg_buf = kzalloc(i2c_reg->count, GFP_KERNEL);
		if (!i2c_reg_buf)
			return -ENOMEM;

		ret = copy_from_user(i2c_reg_buf, (char *) i2c_reg->buffer, i2c_reg->count);

		ret = ioctl_gencam_i2cwrite_reg(client, i2c_reg->reg, i2c_reg->size, i2c_reg->count, i2c_reg_buf);
		if (ret < 0) {
			avt_err(sd, "i2c write failed (%d), bytes written = %d\n", ret, i2c_reg->count);
		}
		/* Check if mode (BCRM or GenCP) is changed */
		else {
			if (i2c_reg->reg == GENCP_CHANGEMODE_8W) {
				priv->mode = i2c_reg_buf[0] == 0 ? AVT_BCRM_MODE : AVT_GENCP_MODE;
			}
		}

		break;

	case VIDIOC_G_I2C_CLOCK_FREQ:

		i2c_clk_freq = arg;
		*i2c_clk_freq = 100000;
		ret = 0;

		break;

	case VIDIOC_G_GENCP_BUFFER_SIZES:
		gencp_buf_sz = arg;
		gencp_buf_sz->nGenCPInBufferSize = priv->gencp_reg.gencp_in_buffer_size > I2C_IO_LIMIT ? I2C_IO_LIMIT : priv->gencp_reg.gencp_in_buffer_size;
		gencp_buf_sz->nGenCPOutBufferSize = priv->gencp_reg.gencp_out_buffer_size > I2C_IO_LIMIT ? I2C_IO_LIMIT : priv->gencp_reg.gencp_out_buffer_size;
		ret = 0;
		break;

	case VIDIOC_G_DRIVER_INFO:
		info = (struct v4l2_csi_driver_info *)arg;

		info->id.manufacturer_id = MANUFACTURER_ID_NXP;
		info->id.soc_family_id = SOC_FAMILY_ID_IMX8M;
		info->id.driver_id = IMX8M_DRIVER_ID_DEFAULT;

		info->driver_version = (MAJOR_DRV << 16) + (MINOR_DRV << 8) + PATCH_DRV;
		info->driver_interface_version = (MAJOR_DRV_IF << 16) + (MINOR_DRV_IF << 8) + PATCH_DRV_IF;
		info->driver_caps = AVT_DRVCAP_MMAP;
		info->usrptr_alignment = dma_get_cache_alignment();

		ret = 0;
		break;

	case VIDIOC_G_CSI_CONFIG:
		config = (struct v4l2_csi_config *)arg;

		config->lane_count = priv->numlanes;
		config->csi_clock = priv->csi_clk_freq;

		ret = 0;
		break;

	case VIDIOC_S_CSI_CONFIG:
		config = (struct v4l2_csi_config *)arg;

		/* Set number of lanes */
		priv->numlanes = config->lane_count;

		ret = avt_reg_read(priv->client,
				priv->cci_reg.bcrm_addr + SUPPORTED_CSI2_LANE_COUNTS_8R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
				(char *) &avt_supported_lane_counts);
		if (ret < 0) {
			avt_err(sd, "i2c read failed (%d)\n", ret);
			ret = -1;
			break;
		}
		if (!(test_bit(priv->numlanes - 1, (const long *)(&avt_supported_lane_counts)))) {
			avt_err(sd, "requested number of lanes (%u) not supported by this camera!\n",
					priv->numlanes);
			ret = -1;
			break;
		}
		ret = avt_reg_write(priv->client,
				priv->cci_reg.bcrm_addr + CSI2_LANE_COUNT_8RW,
				priv->numlanes);
		if (ret < 0) {
			avt_err(sd, "i2c write failed (%d)\n", ret);
			ret = -1;
			break;
		}
		//priv->numlanes = priv->s_data->numlanes;

		/* Set CSI clock frequency */

		ret = avt_reg_read(priv->client,
				priv->cci_reg.bcrm_addr + CSI2_CLOCK_MIN_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &avt_min_clk);

		if (ret < 0) {
			avt_err(sd, "i2c read failed (%d)\n", ret);
			ret = -1;
			break;
		}

		ret = avt_reg_read(priv->client,
				priv->cci_reg.bcrm_addr + CSI2_CLOCK_MAX_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &avt_max_clk);

		if (ret < 0) {
			avt_err(sd, "i2c read failed (%d)\n", ret);
			ret = -1;
			break;
		}

		if (common_range(avt_min_clk, avt_max_clk, 1,
				config->csi_clock, config->csi_clock, 1,
				&common_min_clk, &common_max_clk, &common_inc_clk)
				== false) {
			avt_err(sd, "clock value does not fit the supported frequency range!\n");
			return -EINVAL;
		}

		CLEAR(i2c_reg_address);
		clk = config->csi_clock;
		swapbytes(&clk, AV_CAM_DATA_SIZE_32);
		i2c_reg_address = priv->cci_reg.bcrm_addr + CSI2_CLOCK_32RW;
		i2c_reg_size = AV_CAM_REG_SIZE;
		i2c_reg_count = AV_CAM_DATA_SIZE_32;
		i2c_reg_buf = (char *) &clk;
		ret = ioctl_gencam_i2cwrite_reg(priv->client, i2c_reg_address, i2c_reg_size,
						i2c_reg_count, i2c_reg_buf);
		if (ret < 0) {
			avt_err(sd, "i2c write failed (%d)\n", ret);
			ret = -1;
			break;
		}

		/* Read back CSI clock frequency */
		ret = avt_reg_read(priv->client,
				priv->cci_reg.bcrm_addr + CSI2_CLOCK_32RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &priv->csi_clk_freq);

		if (ret < 0) {
			avt_err(sd, "i2c read failed (%d)\n", ret);
			ret = -1;
			break;
		}
		ret = 0;
		break;

	default:
		break;
	}

	return ret;
}


/* --------------- VIDEO OPS --------------- */

static int avt_csi2_get_mbus_config(struct v4l2_subdev *sd, unsigned pad,
		struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2_DPHY;

	cfg->flags = V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	cfg->flags |= V4L2_MBUS_CSI2_4_LANE; /* TODO check why Allied put 2 only */

	return 0;
}

static int avt_set_param(struct i2c_client *client,
			uint32_t id, uint32_t value)
{
	struct avt_ctrl ct;

	CLEAR(ct);
	ct.id = id;
	ct.value0 = value;

	return avt_ctrl_send(client, &ct);
}

static int avt_get_param(struct i2c_client *client,
			uint32_t id, uint32_t *value)
{
	struct avt_ctrl ct;
	int ret;

	CLEAR(ct);
	ct.id = id;
	ret = avt_ctrl_send(client, &ct);

	if (ret < 0)
		return ret;

	*value = ct.value0;

	return 0;
}

/* Start/Stop streaming from the device */
static int avt_csi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	int ret = 0;

	if (enable) {
// [mjsob] TODO: numlanes dynamic setting
#if 0
		if (priv->numlanes != priv->s_data->numlanes) {
			ret = avt_init_mode(sd);
			if (ret < 0)
				return ret;
			priv->numlanes = priv->s_data->numlanes;
		}
#endif
		ret = avt_set_param(client, V4L2_AV_CSI2_STREAMON_W, 1);
	} else
		ret = avt_set_param(client, V4L2_AV_CSI2_STREAMOFF_W, 1);

	if (ret < 0)
		return ret;

	priv->stream_on = enable ? true : false;

	return 0;
}

static int avt_csi2_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_format *format)
{
	int ret = 0;
	uint32_t val = 0;

	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (format->pad != 0)
		return -EINVAL;

	ret = avt_get_param(client, V4L2_AV_CSI2_WIDTH_R, &val);
	if (ret < 0)
		return ret;
	format->format.width = val;

	ret = avt_get_param(client, V4L2_AV_CSI2_HEIGHT_R, &val);
	if (ret < 0)
		return ret;
	format->format.height = val;

	ret = avt_get_param(client, V4L2_AV_CSI2_PIXELFORMAT_R, &val);
	if (ret < 0)
		return ret;
	format->format.code = val;

	/* Hardcoded default format */
	format->format.field = V4L2_FIELD_NONE;
	format->format.colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static int avt_frm_supported(int wmin, int wmax, int ws,
				int hmin, int hmax, int hs,
				int w, int h)
{
	if (
		w > wmax || w < wmin ||
		h > hmax || h < hmin ||
		(h - hmin) % hs != 0 ||
		(w - wmin) % ws != 0
	)
		return -EINVAL;

	return 0;
}

static int avt_set_csi_clk(struct v4l2_subdev *sd, uint32_t host_max_clk)
{
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	uint32_t common_min_clk = 0;
	uint32_t common_max_clk = 0;
	uint32_t common_inc_clk = 0;
	uint32_t avt_min_clk = 0;
	uint32_t avt_max_clk = 0;
	int ret = 0;

	uint32_t i2c_reg;
	uint32_t i2c_reg_size;
	uint32_t i2c_reg_count;
	uint32_t clk;

	char *i2c_reg_buf;

	ret = avt_reg_read(priv->client,
			   priv->cci_reg.bcrm_addr + CSI2_CLOCK_MIN_32R,
			   AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
			   (char *) &avt_min_clk);

	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	ret = avt_reg_read(priv->client,
			   priv->cci_reg.bcrm_addr + CSI2_CLOCK_MAX_32R,
			   AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
			   (char *) &avt_max_clk);

	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	avt_dbg(2, sd, "csi clock camera range: %d:%d Hz, host range: %d:%d Hz\n",
		avt_min_clk, avt_max_clk, CSI_HOST_CLK_MIN_FREQ, host_max_clk);

	if (!common_range(avt_min_clk, avt_max_clk, 1,
			 CSI_HOST_CLK_MIN_FREQ, host_max_clk, 1,
			 &common_min_clk, &common_max_clk, &common_inc_clk)) {
		avt_err(sd, "no common clock range for camera and host possible!\n");
		return -EINVAL;
	}

	avt_dbg(2, sd, "camera/host common csi clock range: %d:%d Hz\n",
		common_min_clk, common_max_clk);

	if (common_max_clk == 0) {
		avt_dbg(2, sd, "using csi clock from dts: %u Hz\n", priv->csi_clk_freq);
	} else {
		avt_dbg(2, sd, "using csi clock common max (%d Hz)\n", common_max_clk);
		priv->csi_clk_freq = common_max_clk;
	}

	CLEAR(i2c_reg);
	clk = priv->csi_clk_freq;
	swapbytes(&clk, AV_CAM_DATA_SIZE_32);
	i2c_reg = priv->cci_reg.bcrm_addr + CSI2_CLOCK_32RW;
	i2c_reg_size = AV_CAM_REG_SIZE;
	i2c_reg_count = AV_CAM_DATA_SIZE_32;
	i2c_reg_buf = (char *) &clk;
	ret = ioctl_gencam_i2cwrite_reg(priv->client, i2c_reg, i2c_reg_size,
					i2c_reg_count, i2c_reg_buf);

	ret = avt_reg_read(priv->client,
			   priv->cci_reg.bcrm_addr + CSI2_CLOCK_32RW,
			   AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
			   (char *) &avt_max_clk);

	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	avt_dbg(2, sd, "csi clock read from camera: %d Hz\n", avt_max_clk);

	return 0;
}

static int avt_csi2_try_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_format *format)
{
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	int ret = 0;

	ret = avt_frm_supported(
			priv->frmp.minw, priv->frmp.maxw, priv->frmp.sw,
			priv->frmp.minh, priv->frmp.maxh, priv->frmp.sh,
			format->format.width, format->format.height);

	if (ret < 0) {
		avt_err(sd, "Not supported format!\n");
		return ret;
	}

	return 0;
}

static int avt_csi2_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_format *format)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	struct v4l2_subdev_selection sel;
	uint32_t host_max_clk;
	int ret;

	// changing the resolution is not allowed with VIDIOC_S_FMT
	if (priv->mode == AVT_BCRM_MODE &&
		(format->format.width != priv->frmp.r.width ||
		 format->format.height != priv->frmp.r.height))
	{
		avt_err(sd, "Changing the resolution is not allowed with VIDIOC_S_FMT!\n");
		return -EINVAL;
	}

	format->format.colorspace = V4L2_COLORSPACE_SRGB;
	format->format.field = V4L2_FIELD_NONE;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		return avt_csi2_try_fmt(sd, state, format);

	sel.target = V4L2_SEL_TGT_CROP;
	sel.r = priv->frmp.r;

	avt_set_selection(sd, NULL, &sel);

	ret = avt_set_param(client, V4L2_AV_CSI2_PIXELFORMAT_W,
		format->format.code);
	if (ret < 0)
		return ret;

	/* Save format to private data only if
	 * set_param succeded
	 */
	priv->mbus_fmt_code = format->format.code;

	/* Change the CSI clock frequency */
	if (priv->numlanes == 4) {
		switch (priv->mbus_fmt_code) {
		case MEDIA_BUS_FMT_Y10_1X10:
		case MEDIA_BUS_FMT_SRGGB10_1X10:
		case MEDIA_BUS_FMT_SGRBG10_1X10:
		case MEDIA_BUS_FMT_SGBRG10_1X10:
		case MEDIA_BUS_FMT_SBGGR10_1X10:
			host_max_clk = CSI_HOST_CLK_MAX_FREQ_4L_BA10;
			break;
		case MEDIA_BUS_FMT_Y8_1X8:
		case MEDIA_BUS_FMT_SBGGR8_1X8:
		case MEDIA_BUS_FMT_SGBRG8_1X8:
		case MEDIA_BUS_FMT_SGRBG8_1X8:
		case MEDIA_BUS_FMT_SRGGB8_1X8:
			host_max_clk = CSI_HOST_CLK_MAX_FREQ_4L_RAW8;
			break;
		case MEDIA_BUS_FMT_RGB888_1X24:
		case MEDIA_BUS_FMT_BGR888_1X24:
			host_max_clk = CSI_HOST_CLK_MAX_FREQ_4L_RGB24;
			break;
		default:
			host_max_clk = CSI_HOST_CLK_MAX_FREQ_4L;
			break;
		}
	} else {
		host_max_clk = CSI_HOST_CLK_MAX_FREQ;
	}

	if (priv->csi_clk_freq != host_max_clk) {
		return avt_set_csi_clk(sd, host_max_clk);
	}

	return 0;
}

static uint16_t avt_mbus_formats[] = {
	/* RAW 8 */
	MEDIA_BUS_FMT_Y8_1X8,
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	/* RAW10 */
	MEDIA_BUS_FMT_Y10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	/* RAW12 */
	MEDIA_BUS_FMT_Y12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	/* RGB565 */
	MEDIA_BUS_FMT_RGB565_1X16,
	/* RGB888 */
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_BGR888_1X24,
	/* YUV422 */
	MEDIA_BUS_FMT_VYUY8_2X8,
	/* JPEG */

};

/* These formats are hidden from VIDIOC_ENUM_FMT ioctl */
static uint16_t avt_hidden_mbus_formats[] = {
	/* RAW12 */
	MEDIA_BUS_FMT_Y12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SBGGR12_1X12,
};

static bool avt_mbus_fmt_is_hidden(uint16_t mbus_fmt)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(avt_hidden_mbus_formats); i++)
		if (avt_hidden_mbus_formats[i] == mbus_fmt)
			return true;

	return false;
}

static void avt_init_avail_formats(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = avt_get_priv(sd);

	int fmt_iter = 0;
	int i;
	int32_t *avail_fmts;

	avail_fmts = kmalloc(sizeof(int32_t)*ARRAY_SIZE(avt_mbus_formats), GFP_KERNEL);

	for (i = 0; i < ARRAY_SIZE(avt_mbus_formats); i++) {
		if (avt_check_fmt_available(client, avt_mbus_formats[i])
		    && !avt_mbus_fmt_is_hidden(avt_mbus_formats[i])) {
			avail_fmts[fmt_iter++] = avt_mbus_formats[i];
		}
	}

	avail_fmts[fmt_iter] = -EINVAL;

	priv->available_fmts = avail_fmts;
	priv->available_fmts_cnt = fmt_iter;
}

static int avt_csi2_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_mbus_code_enum *code)
{
	struct avt_csi2_priv *priv = avt_get_priv(sd);

	avt_dbg(2, sd, "()\n");
	if (code->index >= priv->available_fmts_cnt)
		return -EINVAL;

	code->code = priv->available_fmts[code->index];
	if (code->code == -EINVAL)
		return -EINVAL;

	return 0;
}

static int avt_csi2_enum_framesizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	bool format_present = false;
	int i;

	avt_dbg(2, sd, "()\n");

	fse->min_width = fse->max_width = priv->frmp.r.width;
	fse->min_height = fse->max_height = priv->frmp.r.height;

	for (i = 0; i < ARRAY_SIZE(avt_mbus_formats); i++)
		if (avt_mbus_formats[i] == fse->code)
			format_present = true;

	if (fse->index != 0 || format_present == false)
		return -EINVAL;

	return 0;
}

static int avt_csi2_enum_frameintervals(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_frame_interval_enum *fie)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	struct v4l2_fract tpf;
	int ret;
	u64 framerate;

	/* Only one frame rate should be returned
	 * - the current frame rate
	 */
	if (fie->index > 0)
		return -EINVAL;

	ret = avt_reg_read(client,
			   priv->cci_reg.bcrm_addr +
			   ACQUISITION_FRAME_RATE_64RW,
			   AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
			   (char *) &framerate);
	if (ret < 0) {
			dev_err(&client->dev, "read frameinterval failed\n");
			return ret;
	}

	/* Translate frequency to timeperframe
	* by inverting the fraction
	*/
	tpf.numerator = UHZ_TO_HZ;
	tpf.denominator = framerate;

	fie->interval = tpf;

	return 0;
}

static int convert_bcrm_to_v4l2(struct bcrm_to_v4l2 *bcrmv4l2,
		int conversion, bool abs)
{
	int64_t value = 0;
	int32_t min = 0;
	int32_t max = 0;
	int32_t step = 0;
	int32_t result = 0;
	int32_t valuedown = 0;
	int32_t valueup = 0;

	step = 1;
	max = S32_MAX;

	/* 1. convert to double */
	if (conversion == min_enum) {
		value = bcrmv4l2->min_bcrm;
		//min = 1;
	} else if (conversion == max_enum) {
		value = bcrmv4l2->max_bcrm;
		min = bcrmv4l2->min_bcrm;
	} else if (conversion == step_enum) {
		value = bcrmv4l2->step_bcrm;
		min = S32_MIN;
	}

	/* Clamp to limits of int32 representation */
	if (value > S32_MAX)
		value = S32_MAX;
	if (value < S32_MIN)
		value = S32_MIN;

	/* 2. convert the units */
/*	value *= factor; */
	if (abs) {
		if (value != 0)
			do_div(value, 1UL);
	}
	/* V4L2_CID_EXPOSURE_ABSOLUTE */
	else {
		if (value != 0)
			do_div(value, 100000UL);
	}

	/* 3. Round value to next integer */
	if (value < S32_MIN)
		result = S32_MIN;
	else if (value > S32_MAX)
		result = S32_MAX;
	else
		result = value;

	/* 4. Clamp to limits */
	if (result > max)
		result = max;
	else if (result < min)
		result = min;

	/* 5. Use nearest increment */
	valuedown = result - ((result - min) % (step));
	valueup = valuedown + step;

	if (result >= 0) {
		if (((valueup - result) <= (result - valuedown))
		&& (valueup <= bcrmv4l2->max_bcrm))
			result = valueup;
		else
			result = valuedown;
	} else {
		if (((valueup - result) < (result - valuedown))
			&& (valueup <= bcrmv4l2->max_bcrm))
			result = valueup;
		else
			result = valuedown;
	}

	if (conversion == min_enum)
		bcrmv4l2->min_v4l2 = result;
	else if (conversion == max_enum)
		bcrmv4l2->max_v4l2 = result;
	else if (conversion == step_enum)
		bcrmv4l2->step_v4l2 = result;
	return 0;
}

static __s32 convert_s_ctrl(__s32 val, __s32 min, __s32 max, __s32 step)
{
	int32_t valuedown = 0, valueup = 0;

	if (val > max)
		val = max;

	else if (val < min)
		val = min;

	valuedown = val - ((val - min) % step);
	valueup = valuedown + step;

	if (val >= 0) {
		if (((valueup - val) <= (val - valuedown)) && (valueup <= max))
			val = valueup;
		else
			val = valuedown;
	} else {
		if (((valueup - val) < (val - valuedown)) && (valueup <= max))
			val = valueup;
		else
			val = valuedown;
	}

	return val;
}

static int ioctl_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = to_avt_csi2(client);

	u64 value_feature = 0;
	u32 value = 0;
	u64 value64 = 0;
	int ret;
	union bcrm_feature_reg feature_inquiry_reg;
	struct bcrm_to_v4l2 bcrm_v4l2;

	avt_dbg(2, sd, "\n");

	CLEAR(bcrm_v4l2);

	/* reading the Feature inquiry register */
	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + FEATURE_INQUIRY_REG_64R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
			(char *) &value_feature);
	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	feature_inquiry_reg.value = value_feature;

	switch (qctrl->id) {
	/* BLACK LEVEL is deprecated and thus we use Brightness */
	case V4L2_CID_BRIGHTNESS:
		avt_dbg(2, sd, "case V4L2_CID_BRIGHTNESS\n");

		if (!feature_inquiry_reg.feature_inq.black_level_avail) {
			avt_info(sd, "control 'Brightness' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the current Black Level value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + BLACK_LEVEL_32RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "BLACK_LEVEL_32RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = value;

		/* reading the Minimum Black Level */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + BLACK_LEVEL_MIN_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "BLACK_LEVEL_MIN_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->minimum = value;

		/* reading the Maximum Black Level */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + BLACK_LEVEL_MAX_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "BLACK_LEVEL_MAX_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->maximum = value;

		/* reading the Black Level step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + BLACK_LEVEL_INCREMENT_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "BLACK_LEVEL_INCREMENT_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->step = value;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Brightness: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Brightness: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		qctrl->type = V4L2_CTRL_TYPE_INTEGER;
		strcpy(qctrl->name, "Brightness");
		break;

	case V4L2_CID_EXPOSURE:
		avt_dbg(2, sd, "case V4L2_CID_EXPOSURE\n");

		/* reading the Exposure time */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + EXPOSURE_TIME_64RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "EXPOSURE_TIME_64RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = (__s32) value64;

		/* reading the Minimum Exposure time */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + EXPOSURE_TIME_MIN_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "EXPOSURE_TIME_MIN_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.min_bcrm = value64;

		/* reading the Maximum Exposure time */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + EXPOSURE_TIME_MAX_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "EXPOSURE_TIME_MAX_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.max_bcrm = value64;

		/* reading the Exposure time step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + EXPOSURE_TIME_INCREMENT_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "EXPOSURE_TIME_INCREMENT_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.step_bcrm = value64;

		convert_bcrm_to_v4l2(&bcrm_v4l2, min_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, max_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, step_enum, true);

		qctrl->minimum = bcrm_v4l2.min_v4l2;
		qctrl->maximum = bcrm_v4l2.max_v4l2;
		qctrl->step = bcrm_v4l2.step_v4l2;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Exposure: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Exposure: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		qctrl->type = V4L2_CTRL_TYPE_INTEGER;
		strcpy(qctrl->name, "Exposure");
		break;

	case V4L2_CID_EXPOSURE_ABSOLUTE:
		avt_dbg(2, sd, "case V4L2_CID_EXPOSURE_ABSOLUTE\n");

		/* reading the Exposure time */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + EXPOSURE_TIME_64RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "EXPOSURE_TIME_64RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		/* convert unit [ns] to [100*us] */
		value64 = value64 / 100000UL;

		/* clamp to s32 max */
		qctrl->default_value = clamp(value64, (u64)0, (u64)S32_MAX);

		/* reading the Maximum Exposure time */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + EXPOSURE_TIME_MAX_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "EXPOSURE_TIME_MAX_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		/* convert unit [ns] to [100*us] */
		value64 = value64 / 100000UL;

		/* clamp to s32 max */
		qctrl->maximum = clamp(value64, (u64)0, (u64)S32_MAX);

		qctrl->minimum = 1;
		qctrl->step = 1;
		qctrl->type = V4L2_CTRL_TYPE_INTEGER64;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Exposure Absolute: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}

		strcpy(qctrl->name, "Exposure Absolute");
		break;

	case V4L2_CID_EXPOSURE_AUTO:

		avt_dbg(2, sd, "case V4L2_CID_EXPOSURE_AUTO\n");

		if (!feature_inquiry_reg.feature_inq.exposure_auto) {
			avt_info(sd, "control 'Exposure Auto' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the current exposure auto value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + EXPOSURE_AUTO_8RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "EXPOSURE_AUTO_8RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		if (value == 2)
			/* continous mode, Refer BCRM doc */
			qctrl->default_value = V4L2_EXPOSURE_AUTO;
		else
			/* false (OFF) */
			qctrl->default_value = V4L2_EXPOSURE_MANUAL;

		qctrl->minimum = 0;
		qctrl->step = 0;
		qctrl->maximum = 1;
		qctrl->type = V4L2_CTRL_TYPE_MENU;

		strcpy(qctrl->name, "Exposure Auto");
		break;

	case V4L2_CID_GAIN:

		avt_dbg(2, sd, "case V4L2_CID_GAIN\n");

		if (!feature_inquiry_reg.feature_inq.gain_avail) {
			avt_info(sd, "control 'Gain' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Gain value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + GAIN_64RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "GAIN_64RW: i2c read failed (%d)\n", ret);
			return ret;
		}

		qctrl->default_value = (__s32) value64;

		/* reading the Minimum Gain value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + GAIN_MINIMUM_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "GAIN_MINIMUM_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.min_bcrm = value64;

		/* reading the Maximum Gain value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + GAIN_MAXIMUM_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "GAIN_MAXIMUM_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.max_bcrm = value64;

		/* reading the Gain step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + GAIN_INCREMENT_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "GAIN_INCREMENT_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.step_bcrm = value64;

		convert_bcrm_to_v4l2(&bcrm_v4l2, min_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, max_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, step_enum, true);

		qctrl->minimum = bcrm_v4l2.min_v4l2;
		qctrl->maximum = bcrm_v4l2.max_v4l2;
		qctrl->step = bcrm_v4l2.step_v4l2;
		qctrl->type = V4L2_CTRL_TYPE_INTEGER;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Gain: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Gain: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		strcpy(qctrl->name, "Gain");
		break;

	case V4L2_CID_AUTOGAIN:
		avt_dbg(2, sd, "case V4L2_CID_AUTOGAIN\n");

		if (!feature_inquiry_reg.feature_inq.gain_auto) {
			avt_info(sd, "control 'Gain Auto' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Auto Gain value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + GAIN_AUTO_8RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
				(char *) &value);

		if (ret < 0) {
			avt_err(sd, "GAIN_AUTO_8RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		if (value == 2)
			/* true (ON) for continous mode, Refer BCRM doc */
			qctrl->default_value = true;
		else
			/* false (OFF) */
			qctrl->default_value = false;

		qctrl->minimum = 0;
		qctrl->step = 1;
		qctrl->maximum = 1;
		qctrl->type = V4L2_CTRL_TYPE_BOOLEAN;

		strcpy(qctrl->name, "Auto Gain");
		break;

	case V4L2_CID_HFLIP:
		avt_dbg(2, sd, "case V4L2_CID_HFLIP\n");

		if (!feature_inquiry_reg.feature_inq.reverse_x_avail) {
			avt_info(sd, "control 'Reversing X (Horizantal Flip)' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Reverse X value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + IMG_REVERSE_X_8RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "IMG_REVERSE_X_8RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = value;

		qctrl->minimum = 0;
		qctrl->step = qctrl->maximum = 1;
		qctrl->type = V4L2_CTRL_TYPE_BOOLEAN;
		strcpy(qctrl->name, "Reverse X");

		break;

	case V4L2_CID_VFLIP:
		avt_dbg(2, sd, "case V4L2_CID_VFLIP\n");
		if (!feature_inquiry_reg.feature_inq.reverse_y_avail) {
			avt_info(sd, "control 'Reversing Y (Vertical Flip)' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Reverse Y value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + IMG_REVERSE_Y_8RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "IMG_REVERSE_Y_8RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = value;

		qctrl->minimum = 0;
		qctrl->step = qctrl->maximum = 1;
		qctrl->type = V4L2_CTRL_TYPE_BOOLEAN;
		strcpy(qctrl->name, "Reverse Y");

		break;

	case V4L2_CID_GAMMA:
		avt_dbg(2, sd, "case V4L2_CID_GAMMA\n");

		if (!feature_inquiry_reg.feature_inq.gamma_avail) {
			avt_info(sd, "control 'Gamma' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Gamma value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + GAMMA_64RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "GAMMA_64RW: i2c read failed (%d)\n", ret);
			return ret;
		}

		qctrl->default_value = (__s32) value64;

		/* reading the Minimum Gamma */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + GAMMA_GAIN_MINIMUM_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "GAMMA_GAIN_MINIMUM_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.min_bcrm = value64;

		/* reading the Maximum Gamma */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + GAMMA_GAIN_MAXIMUM_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "GAMMA_GAIN_MAXIMUM_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.max_bcrm = value64;

		/* reading the Gamma step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + GAMMA_GAIN_INCREMENT_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "GAMMA_GAIN_INCREMENT_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.step_bcrm = value64;

		convert_bcrm_to_v4l2(&bcrm_v4l2, min_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, max_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, step_enum, true);

		qctrl->minimum = bcrm_v4l2.min_v4l2;
		qctrl->maximum = bcrm_v4l2.max_v4l2;
		qctrl->step = bcrm_v4l2.step_v4l2;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Gamma: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Gamma: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		qctrl->type = V4L2_CTRL_TYPE_INTEGER;
		strcpy(qctrl->name, "Gamma");
		break;

	case V4L2_CID_CONTRAST:
		avt_dbg(2, sd, "case V4L2_CID_CONTRAST\n");

		if (!feature_inquiry_reg.feature_inq.contrast_avail) {
			avt_info(sd, "control 'Contrast' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Contrast value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + CONTRAST_VALUE_32RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "CONTRAST_VALUE_32RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = value;

		/* reading the Minimum Contrast */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + CONTRAST_VALUE_MIN_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "CONTRAST_VALUE_MIN_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->minimum = value;

		/* reading the Maximum Contrast */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + CONTRAST_VALUE_MAX_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "CONTRAST_VALUE_MAX_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->maximum = value;

		/* reading the Contrast step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr +
				CONTRAST_VALUE_INCREMENT_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "CONTRAST_VALUE_INCREMENT_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->step = value;

		qctrl->type = V4L2_CTRL_TYPE_INTEGER;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Contrast: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Contrast: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		strcpy(qctrl->name, "Contrast");
		break;

	case V4L2_CID_AUTO_WHITE_BALANCE:
		avt_dbg(2, sd, "case V4L2_CID_AUTO_WHITE_BALANCE\n");

		if (!feature_inquiry_reg.feature_inq.white_balance_auto_avail) {
			avt_info(sd, "control 'White balance Auto' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the White balance auto value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + WHITE_BALANCE_AUTO_8RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "WHITE_BALANCE_AUTO_8RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		if (value == 2)
			/* true (ON) */
			qctrl->default_value = true;
		else
			/* false (OFF) */
			qctrl->default_value = false;

		qctrl->minimum = 0;
		qctrl->step = 1;
		qctrl->maximum = 1;
		qctrl->type = V4L2_CTRL_TYPE_BOOLEAN;

		strcpy(qctrl->name, "White Balance Auto");
		break;

	case V4L2_CID_DO_WHITE_BALANCE:
		avt_dbg(2, sd, "case V4L2_CID_DO_WHITE_BALANCE\n");

		if (!feature_inquiry_reg.feature_inq.white_balance_avail) {
			avt_info(sd, "control 'White balance' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the White balance auto reg */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + WHITE_BALANCE_AUTO_8RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "WHITE_BALANCE_AUTO_8RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = 0;
		qctrl->minimum = 0;
		qctrl->step = 0;
		qctrl->maximum = 0;
		qctrl->type = V4L2_CTRL_TYPE_BUTTON;

		strcpy(qctrl->name, "White Balance");
		break;

	case V4L2_CID_SATURATION:
		avt_dbg(2, sd, "case V4L2_CID_SATURATION\n");

		if (!feature_inquiry_reg.feature_inq.saturation_avail) {
			avt_info(sd, "control 'Saturation' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Saturation value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + SATURATION_32RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "SATURATION_32RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = value;

		/* reading the Minimum Saturation */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + SATURATION_MIN_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "SATURATION_MIN_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->minimum = value;

		/* reading the Maximum Saturation */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + SATURATION_MAX_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "SATURATION_MAX_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->maximum = value;

		/* reading the Saturation step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + SATURATION_INCREMENT_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "SATURATION_INCREMENT_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->step = value;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Saturation: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Saturation: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		qctrl->type = V4L2_CTRL_TYPE_INTEGER;
		strcpy(qctrl->name, "Saturation");
		break;

	case V4L2_CID_HUE:
		avt_dbg(2, sd, "case V4L2_CID_HUE\n");

		if (!feature_inquiry_reg.feature_inq.hue_avail) {
			avt_info(sd, "control 'Hue' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Hue value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + HUE_32RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "HUE_32RW: i2c read failed (%d)\n", ret);
			return ret;
		}

		qctrl->default_value = value;

		/* reading the Minimum HUE */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + HUE_MIN_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "HUE_MIN_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->minimum = value;

		/* reading the Maximum HUE */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + HUE_MAX_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "HUE_MAX_32R: i2c read failed (%d)\n", ret);
			return ret;
		}

		qctrl->maximum = value;

		/* reading the HUE step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + HUE_INCREMENT_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "HUE_INCREMENT_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->step = value;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Hue: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Hue: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		qctrl->type = V4L2_CTRL_TYPE_INTEGER;
		strcpy(qctrl->name, "Hue");
		break;

	case V4L2_CID_RED_BALANCE:
		avt_dbg(2, sd, "case V4L2_CID_RED_BALANCE\n");

		if (!feature_inquiry_reg.feature_inq.white_balance_avail) {
			avt_info(sd, "control 'Red balance' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Red balance value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + RED_BALANCE_RATIO_64RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "RED_BALANCE_RATIO_64RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = (__s32) value64;

		/* reading the Minimum Red balance */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + RED_BALANCE_RATIO_MIN_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "RED_BALANCE_RATIO_MIN_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.min_bcrm = value64;

		/* reading the Maximum Red balance */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + RED_BALANCE_RATIO_MAX_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "RED_BALANCE_RATIO_MAX_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.max_bcrm = value64;

		/* reading the Red balance step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr +
				RED_BALANCE_RATIO_INCREMENT_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "RED_BALANCE_RATIO_INCREMENT_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.step_bcrm = value64;

		convert_bcrm_to_v4l2(&bcrm_v4l2, min_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, max_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, step_enum, true);

		qctrl->minimum = bcrm_v4l2.min_v4l2;
		qctrl->maximum = bcrm_v4l2.max_v4l2;
		qctrl->step = bcrm_v4l2.step_v4l2;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Red Balance: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Red Balance: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		qctrl->type = V4L2_CTRL_TYPE_INTEGER;
		strcpy(qctrl->name, "Red Balance");
		break;

	case V4L2_CID_BLUE_BALANCE:
		avt_dbg(2, sd, "case V4L2_CID_BLUE_BALANCE\n");

		if (!feature_inquiry_reg.feature_inq.white_balance_avail) {
			avt_info(sd, "control 'Blue balance' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Blue balance value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + BLUE_BALANCE_RATIO_64RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "BLUE_BALANCE_RATIO_64RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = (__s32) value64;

		/* reading the Minimum Blue balance */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + BLUE_BALANCE_RATIO_MIN_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "BLUE_BALANCE_RATIO_MIN_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.min_bcrm = value64;

		/* reading the Maximum Blue balance */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + BLUE_BALANCE_RATIO_MAX_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "BLUE_BALANCE_RATIO_MAX_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.max_bcrm = value64;

		/* reading the Blue balance step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr +
				BLUE_BALANCE_RATIO_INCREMENT_64R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
				(char *) &value64);
		if (ret < 0) {
			avt_err(sd, "BLUE_BALANCE_RATIO_INCREMENT_64R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		bcrm_v4l2.step_bcrm = value64;

		convert_bcrm_to_v4l2(&bcrm_v4l2, min_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, max_enum, true);
		convert_bcrm_to_v4l2(&bcrm_v4l2, step_enum, true);

		qctrl->minimum = bcrm_v4l2.min_v4l2;
		qctrl->maximum = bcrm_v4l2.max_v4l2;
		qctrl->step = bcrm_v4l2.step_v4l2;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Blue Balance: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Blue Balance: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		qctrl->type = V4L2_CTRL_TYPE_INTEGER;
		strcpy(qctrl->name, "Blue Balance");
		break;

	case V4L2_CID_SHARPNESS:
		avt_dbg(2, sd, "case V4L2_CID_SHARPNESS\n");

		if (!feature_inquiry_reg.feature_inq.sharpness_avail) {
			avt_info(sd, "control 'Sharpness' not supported by firmware\n");
			qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
			return 0;
		}

		/* reading the Sharpness value */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + SHARPNESS_32RW,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "SHARPNESS_32RW: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->default_value = value;

		/* reading the Minimum sharpness */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + SHARPNESS_MIN_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "SHARPNESS_MIN_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->minimum = value;

		/* reading the Maximum sharpness */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + SHARPNESS_MAX_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "SHARPNESS_MAX_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->maximum = value;

		/* reading the sharpness step increment */
		ret = avt_reg_read(client,
				priv->cci_reg.bcrm_addr + SHARPNESS_INCREMENT_32R,
				AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
				(char *) &value);
		if (ret < 0) {
			avt_err(sd, "SHARPNESS_INCREMENT_32R: i2c read failed (%d)\n",
					ret);
			return ret;
		}

		qctrl->step = value;

		if (qctrl->minimum > qctrl->maximum) {
			avt_err(sd, "Sharpness: min > max! (%d > %d)\n",
					qctrl->minimum, qctrl->maximum);
			return -EINVAL;
		}
		if (qctrl->step <= 0) {
			avt_err(sd, "Sharpness: non-positive step value (%d)!\n",
					qctrl->step);
			return -EINVAL;
		}

		qctrl->type = V4L2_CTRL_TYPE_INTEGER;
		strcpy(qctrl->name, "Sharpness");
		break;

	default:
		avt_info(sd, "case default or not supported qctrl->id 0x%x\n",
				qctrl->id);
		qctrl->flags = V4L2_CTRL_FLAG_DISABLED;
		return 0;
	}

	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	avt_dbg(2, sd, "ret = %d\n", ret);

	return 0;
}

static int32_t convert_bcrm_to_v4l2_gctrl(struct bcrm_to_v4l2 *bcrmv4l2,
		int64_t val64, bool abs)
{

	int32_t value = 0;
	int32_t min = 0;
	int32_t max = 0;
	int32_t step = 0;
	int32_t result = 0;
	int32_t valuedown = 0;
	int32_t valueup = 0;

/* 1. convert to double */
	step = bcrmv4l2->step_v4l2;
	max = bcrmv4l2->max_v4l2;
	min = bcrmv4l2->min_v4l2;
	value = (int32_t) val64;

	/* 2. convert the units */
/*	value *= factor; */

	/* V4L2_CID_EXPOSURE_ABSOLUTE */
	if (abs) {
		if (value != 0)
			do_div(value, 100000UL);
	}

	/* 3. Round value to next integer */

	if (value < S32_MIN)
		result = S32_MIN;
	else if (value > S32_MAX)
		result = S32_MAX;
	else
		result = value;

	/* 4. Clamp to limits */
	if (result > max)
		result = max;
	else if (result < min)
		result = min;

	/* 5. Use nearest increment */
	valuedown = result - ((result - min) % (step));
	valueup = valuedown + step;

	if (result >= 0) {
		if (((valueup - result) <= (result - valuedown))
		&& (valueup <= bcrmv4l2->max_bcrm))
			result = valueup;
		else
			result = valuedown;
	} else {
		if (((valueup - result) < (result - valuedown))
			&& (valueup <= bcrmv4l2->max_bcrm))
			result = valueup;
		else
			result = valuedown;
	}

	return result;
}

static int avt_ioctl_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *vc)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = to_avt_csi2(client);

	unsigned int reg = 0;
	int length = 0;
	struct bcrm_to_v4l2 bcrm_v4l2;
	struct v4l2_queryctrl qctrl;
	int ret = 0;
	uint64_t val64 = 0;

	avt_dbg(2, sd, "\n");

	vc->value = 0;

	switch (vc->id) {
	/* BLACK LEVEL is deprecated and thus we use Brightness */
	case V4L2_CID_BRIGHTNESS:
		avt_dbg(2, sd, "V4L2_CID_BRIGHTNESS\n");
		reg = BLACK_LEVEL_32RW;
		length = AV_CAM_DATA_SIZE_32;
		break;
	case V4L2_CID_GAMMA:
		avt_dbg(2, sd, "V4L2_CID_GAMMA\n");
		reg = GAMMA_64RW;
		length = AV_CAM_DATA_SIZE_64;
		break;
	case V4L2_CID_CONTRAST:
		avt_dbg(2, sd, "V4L2_CID_CONTRAST\n");
		reg = CONTRAST_VALUE_32RW;
		length = AV_CAM_DATA_SIZE_32;
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		avt_dbg(2, sd, "V4L2_CID_DO_WHITE_BALANCE\n");
		reg = WHITE_BALANCE_AUTO_8RW;
		length = AV_CAM_DATA_SIZE_8;
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		avt_dbg(2, sd, "V4L2_CID_AUTO_WHITE_BALANCE\n");
		reg = WHITE_BALANCE_AUTO_8RW;
		length = AV_CAM_DATA_SIZE_8;
		break;
	case V4L2_CID_SATURATION:
		avt_dbg(2, sd, "V4L2_CID_SATURATION\n");
		reg = SATURATION_32RW;
		length = AV_CAM_DATA_SIZE_32;
		break;
	case V4L2_CID_HUE:
		avt_dbg(2, sd, "V4L2_CID_HUE\n");
		reg = HUE_32RW;
		length = AV_CAM_DATA_SIZE_32;
		break;
	case V4L2_CID_RED_BALANCE:
		avt_dbg(2, sd, "V4L2_CID_RED_BALANCE\n");
		reg = RED_BALANCE_RATIO_64RW;
		length = AV_CAM_DATA_SIZE_64;
		break;
	case V4L2_CID_BLUE_BALANCE:
		avt_dbg(2, sd, "V4L2_CID_BLUE_BALANCE\n");
		reg = BLUE_BALANCE_RATIO_64RW;
		length = AV_CAM_DATA_SIZE_64;
		break;
	case V4L2_CID_EXPOSURE:
		reg = EXPOSURE_TIME_64RW;
		length = AV_CAM_DATA_SIZE_64;
		break;

	case V4L2_CID_GAIN:
		avt_dbg(2, sd, "V4L2_CID_GAIN\n");
		reg = GAIN_64RW;
		length = AV_CAM_DATA_SIZE_64;
		break;

	case V4L2_CID_AUTOGAIN:
		avt_dbg(2, sd, "V4L2_CID_AUTOGAIN\n");
		reg = GAIN_AUTO_8RW;
		length = AV_CAM_DATA_SIZE_8;
		break;

	case V4L2_CID_SHARPNESS:
		avt_dbg(2, sd, "V4L2_CID_SHARPNESS\n");
		reg = SHARPNESS_32RW;
		length = AV_CAM_DATA_SIZE_32;
		break;

	default:
		avt_err(sd, "case default or not supported\n");
		return -EINVAL;
	}

	CLEAR(bcrm_v4l2);
	CLEAR(qctrl);

	qctrl.id = vc->id;
	ret = ioctl_queryctrl(sd, &qctrl);

	if (ret < 0) {
		avt_err(sd, "queryctrl failed: ret %d\n", ret);
		return ret;
	}

	bcrm_v4l2.min_v4l2 = qctrl.minimum;
	bcrm_v4l2.max_v4l2 = qctrl.maximum;
	bcrm_v4l2.step_v4l2 = qctrl.step;

	/* Overwrite the queryctrl maximum value for auto features since value
	 * 2 is 'true' (1)
	 */
	if (vc->id == V4L2_CID_AUTOGAIN ||
			vc->id == V4L2_CID_AUTO_WHITE_BALANCE)
		bcrm_v4l2.max_v4l2 = 2;

	/* Check values from BCRM */
	if ((bcrm_v4l2.min_v4l2 > bcrm_v4l2.max_v4l2) ||
			(bcrm_v4l2.step_v4l2 <= 0)) {
		avt_err(sd, "invalid BCRM values found. vc->id %d\n", vc->id);
		return -EINVAL;
	}

	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + reg,
			AV_CAM_REG_SIZE, length, (char *) &val64);

	if (vc->id == V4L2_CID_EXPOSURE_ABSOLUTE)
		/* Absolute */
		vc->value = convert_bcrm_to_v4l2_gctrl(&bcrm_v4l2,
				val64, true);
	else
		vc->value = convert_bcrm_to_v4l2_gctrl(&bcrm_v4l2,
				val64, false);

	/* BCRM Auto Exposure changes -> Refer to BCRM document */
	if (vc->id == V4L2_CID_EXPOSURE_AUTO) {

		if (vc->value == 2)
			 /* continous mode, Refer BCRM doc */
			vc->value = V4L2_EXPOSURE_AUTO;
		else
			/* OFF for off & once mode, Refer BCRM doc */
			vc->value = V4L2_EXPOSURE_MANUAL;
	}

	/* BCRM Auto Gain/WB changes -> Refer to BCRM document */
	if (vc->id == V4L2_CID_AUTOGAIN ||
			vc->id == V4L2_CID_AUTO_WHITE_BALANCE) {

		if (vc->value == 2)
			/* continous mode, Refer BCRM doc */
			vc->value = true;
		else
			/* OFF for off & once mode, Refer BCRM doc */
			vc->value = false;
	}

	return ret;
}

static int avt_ioctl_s_ctrl(struct v4l2_subdev *sd, struct v4l2_ext_control *vc)
{
	int ret = 0;
	unsigned int reg = 0;
	int length = 0;
	__s32 value_bkp = 0;
	struct v4l2_queryctrl qctrl;
	struct v4l2_ctrl *vctrl_handle;
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch (vc->id) {
	case V4L2_CID_DO_WHITE_BALANCE:
		avt_dbg(2, sd, "V4L2_CID_DO_WHITE_BALANCE vc->value %u\n",
				vc->value);
		reg = WHITE_BALANCE_AUTO_8RW;
		length = AV_CAM_DATA_SIZE_8;
		vc->value = 1; /* Set 'once' in White Balance Auto Register. */
		break;

/* BLACK LEVEL is deprecated and thus we use Brightness */
	case V4L2_CID_BRIGHTNESS:
		avt_dbg(2, sd, "V4L2_CID_BRIGHTNESS vc->value %u\n", vc->value);
		reg = BLACK_LEVEL_32RW;
		length = AV_CAM_DATA_SIZE_32;

		qctrl.id = V4L2_CID_BRIGHTNESS;/* V4L2_CID_BLACK_LEVEL; */

		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}
		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);
		break;

	case V4L2_CID_CONTRAST:
		avt_dbg(2, sd, "V4L2_CID_CONTRAST vc->value %u\n", vc->value);
		reg = CONTRAST_VALUE_32RW;
		length = AV_CAM_DATA_SIZE_32;

		qctrl.id = V4L2_CID_CONTRAST;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);
		break;
	case V4L2_CID_SATURATION:
		avt_dbg(2, sd, "V4L2_CID_SATURATION vc->value %u\n", vc->value);
		reg = SATURATION_32RW;
		length = AV_CAM_DATA_SIZE_32;

		qctrl.id = V4L2_CID_SATURATION;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);
		break;
	case V4L2_CID_HUE:
		avt_dbg(2, sd, "V4L2_CID_HUE vc->value %u\n", vc->value);
		reg = HUE_32RW;
		length = AV_CAM_DATA_SIZE_32;

		qctrl.id = V4L2_CID_HUE;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);

		break;
	case V4L2_CID_RED_BALANCE:
		avt_dbg(2, sd, "V4L2_CID_RED_BALANCE vc->value %u\n", vc->value);
		reg = RED_BALANCE_RATIO_64RW;
		length = AV_CAM_DATA_SIZE_64;

		qctrl.id = V4L2_CID_RED_BALANCE;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);
		break;
	case V4L2_CID_BLUE_BALANCE:
		avt_dbg(2, sd, "V4L2_CID_BLUE_BALANCE vc->value %u\n", vc->value);
		reg = BLUE_BALANCE_RATIO_64RW;
		length = AV_CAM_DATA_SIZE_64;

		qctrl.id = V4L2_CID_BLUE_BALANCE;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);
		break;

	case V4L2_CID_AUTO_WHITE_BALANCE:
		avt_dbg(2, sd, "V4L2_CID_AUTO_WHITE_BALANCE vc->value %u\n",
				vc->value);
		reg = WHITE_BALANCE_AUTO_8RW;
		length = AV_CAM_DATA_SIZE_8;

		/* BCRM Auto White balance changes	*/
		if (vc->value == true)
			vc->value = 2;/* Continouous mode */
		else
			vc->value = 0;/* 1; OFF/once mode */

		break;
	case V4L2_CID_GAMMA:
		avt_dbg(2, sd, "V4L2_CID_GAMMA vc->value %u\n", vc->value);
		reg = GAMMA_64RW;
		length = AV_CAM_DATA_SIZE_64;

		qctrl.id = V4L2_CID_GAMMA;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}
		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);
		break;
	case V4L2_CID_EXPOSURE:
		avt_dbg(2, sd, "V4L2_CID_EXPOSURE, cci_reg.bcrm_addr 0x%x, vc->value %u\n",
				priv->cci_reg.bcrm_addr, vc->value);

		value_bkp = vc->value;/* backup the actual value */

		/*  i) Setting 'Manual' in Exposure Auto reg. Refer to BCRM
		 *  document
		 */
		vc->value = 0;

		avt_dbg(2, sd, "V4L2_CID_EXPOSURE, cci_reg.bcrm_addr 0x%x, vc->value %u\n",
				priv->cci_reg.bcrm_addr, vc->value);

		ret = ioctl_bcrm_i2cwrite_reg(client,
				vc, EXPOSURE_AUTO_8RW + priv->cci_reg.bcrm_addr,
				AV_CAM_DATA_SIZE_8);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		/* ii) Setting value in Exposure reg. */
		vc->value = value_bkp;/* restore the actual value */
		reg = EXPOSURE_TIME_64RW;
		length = AV_CAM_DATA_SIZE_64;

		qctrl.id = V4L2_CID_EXPOSURE;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}
		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);

		/* Setting the absolute exposure control */
		vctrl_handle = avt_get_control(sd, V4L2_CID_EXPOSURE_ABSOLUTE);
		if (vctrl_handle != NULL) {
			if (!priv->cross_update) {
				priv->cross_update = true;
				__v4l2_ctrl_s_ctrl_int64(vctrl_handle, vc->value / EXP_ABS);
			} else {
				priv->cross_update = false;
			}
		}

		break;

	case V4L2_CID_EXPOSURE_ABSOLUTE:
		avt_dbg(2, sd, "V4L2_CID_EXPOSURE_ABSOLUTE, cci_reg.bcrm_addr 0x%x, vc->value %u\n",
				priv->cci_reg.bcrm_addr, vc->value);

		value_bkp = vc->value;/* backup the actual value */

		/* i) Setting 'Manual' in Exposure Auto reg. */
		vc->value = 0;

		avt_dbg(2, sd, "V4L2_CID_EXPOSURE_ABSOLUTE, cci_reg.bcrm_addr 0x%x, vc->value %u\n",
				priv->cci_reg.bcrm_addr, vc->value);

		ret = ioctl_bcrm_i2cwrite_reg(client,
				vc, EXPOSURE_AUTO_8RW + priv->cci_reg.bcrm_addr,
				AV_CAM_DATA_SIZE_8);

		if (ret < 0) {
			avt_err(sd, "i2c write failed\n");
			return ret;
		}

		/* ii) Setting value in Exposure reg. */
		vc->value = value_bkp;/* restore the actual value */
		reg = EXPOSURE_TIME_64RW;
		length = AV_CAM_DATA_SIZE_64;

		qctrl.id = V4L2_CID_EXPOSURE;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value64 = vc->value64 * EXP_ABS;

		vc->value64 = convert_s_ctrl(vc->value64,
				qctrl.minimum, qctrl.maximum, qctrl.step);

		/* Setting the exposure control */
		vctrl_handle = avt_get_control(sd, V4L2_CID_EXPOSURE);
		if (vctrl_handle != NULL) {
			if (!priv->cross_update) {
				priv->cross_update = true;
				__v4l2_ctrl_s_ctrl(vctrl_handle, vc->value64);
			} else {
				priv->cross_update = false;
			}
		}

		break;

	case V4L2_CID_EXPOSURE_AUTO:
		avt_dbg(2, sd, "V4L2_CID_EXPOSURE_AUTO vc->value %u\n", vc->value);
		reg = EXPOSURE_AUTO_8RW;
		length = AV_CAM_DATA_SIZE_8;

		/* BCRM Auto Gain changes */
		if (vc->value == V4L2_EXPOSURE_AUTO) {
			vc->value = 2;/* Continouous mode */
		} else {
			vc->value = 0;/* 1; OFF/once mode */
		}

		break;

	case V4L2_CID_AUTOGAIN:
		avt_dbg(2, sd, "V4L2_CID_AUTOGAIN vc->value %u\n", vc->value);
		reg = GAIN_AUTO_8RW;
		length = AV_CAM_DATA_SIZE_8;

		/* BCRM Auto Gain changes */
		if (vc->value == true)
			vc->value = 2;/* Continouous mode */
		else
			vc->value = 0;/* 1; OFF/once mode */

		break;
	case V4L2_CID_GAIN:
		avt_dbg(2, sd, "V4L2_CID_GAIN, vc->value %u\n", vc->value);
		reg = GAIN_64RW;
		length = AV_CAM_DATA_SIZE_64;

		qctrl.id = V4L2_CID_GAIN;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);

		break;

	case V4L2_CID_HFLIP:
		avt_dbg(2, sd, "V4L2_CID_HFLIP, vc->value %u\n", vc->value);
		reg = IMG_REVERSE_X_8RW;
		length = AV_CAM_DATA_SIZE_8;

		qctrl.id = V4L2_CID_HFLIP;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);
		break;

	case V4L2_CID_VFLIP:
		avt_dbg(2, sd, "V4L2_CID_VFLIP, vc->value %u\n", vc->value);
		reg = IMG_REVERSE_Y_8RW;
		length = AV_CAM_DATA_SIZE_8;

		qctrl.id = V4L2_CID_VFLIP;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);
		break;

	case V4L2_CID_SHARPNESS:
		avt_dbg(2, sd, "V4L2_CID_SHARPNESS, vc->value %u\n", vc->value);
		reg = SHARPNESS_32RW;
		length = AV_CAM_DATA_SIZE_32;

		qctrl.id = V4L2_CID_SHARPNESS;
		ret = ioctl_queryctrl(sd, &qctrl);

		if (ret < 0) {
			avt_err(sd, "queryctrl failed: ret %d\n", ret);
			return ret;
		}

		vc->value = convert_s_ctrl(vc->value,
				qctrl.minimum, qctrl.maximum, qctrl.step);
		break;

	default:
		avt_err(sd, "case default or not supported\n");
		ret = -EPERM;
		return ret;
	}

	ret = ioctl_bcrm_i2cwrite_reg(client,
			vc, reg + priv->cci_reg.bcrm_addr, length);

	avt_dbg(2, sd, "ret %d\n", ret);

	return ret < 0 ? ret : 0;
}

static int avt_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd;
	struct v4l2_ext_control c;

	struct avt_csi2_priv *priv;

	priv = container_of(ctrl->handler, struct avt_csi2_priv, hdl);
	sd = &priv->subdev;

	c.id = ctrl->id;
	c.value = ctrl->val;
	/* For 64-bit extended V4L2 controls,
	 * new value comes in this pointer
	 */
	c.value64 = *(ctrl->p_new.p_s64);

	return avt_ioctl_s_ctrl(sd, &c);
}

static int avt_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd;
	struct v4l2_control c;
	struct avt_csi2_priv *priv;
	int ret;

	priv = container_of(ctrl->handler, struct avt_csi2_priv, hdl);
	sd = &priv->subdev;

	c.id = ctrl->id;
	ret = avt_ioctl_g_ctrl(sd, &c);
	ctrl->val = c.value;

	return ret;
}

static const struct v4l2_ctrl_ops avt_ctrl_ops = {
	.g_volatile_ctrl = avt_g_volatile_ctrl,
	.s_ctrl		= avt_s_ctrl,
};

static int read_max_resolution(struct v4l2_subdev *sd, uint32_t *max_width, uint32_t *max_height)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	ret = avt_get_param(client, V4L2_AV_CSI2_SENSOR_WIDTH_R, max_width);
	if (ret < 0)
		return ret;

	ret = avt_get_param(client, V4L2_AV_CSI2_SENSOR_HEIGHT_R, max_height);
	if (ret < 0)
		return ret;

	return ret;
}

static int avt_get_selection(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *_state,
		struct v4l2_subdev_selection *sel)
{
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	uint32_t max_width = 0, max_height = 0;

	if (read_max_resolution(sd, &max_width, &max_height) < 0)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = priv->frmp.r;
		break;
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_NATIVE_SIZE:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.top = sel->r.left = 0;
		sel->r.width = max_width;
		sel->r.height = max_height;
		break;
	default:
		return -EINVAL;
	}

	sel->flags = V4L2_SEL_FLAG_LE;

	return 0;
}

static int avt_set_selection(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_selection *sel)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = avt_get_priv(sd);

	// update width, height, offset x/y restrictions from camera
	avt_init_frame_param(priv);

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:

#if 0
		/* Tegra doesn't seem to accept offsets that are not divisible
		 * by 8.
		 */
		roundup(priv->frmp.swoff, 8);
		roundup(priv->frmp.shoff, 8);

		/* Tegra doesn't allow image resolutions smaller than 64x32 */
		priv->frmp.minw = max_t(uint32_t, priv->frmp.minw, 64);
		priv->frmp.minh = max_t(uint32_t, priv->frmp.minh, 32);
#endif

/*
*       As per the crop concept document, the following steps should be followed before setting crop to the sensor.
*
* i)    If new width is less or equal than current width, write the width register first then write offset X (left) register,
*       both values should be within the range (min, max and inc).
* ii)   If new width is higher than current width, write the offset X (left) register first then write the width register,
*       both values should be within the range (min, max, and inc)
* iii)  If new height is less or equal than current height, write the height register first then write offset Y (top) register,
*       both values should be within the range (min, max and inc).
* iv)   If new height is higher than current height, write the offset Y (top) register first then write the height register,
*       both values should be within the range (min, max, and inc)
*/

		if (sel->r.width <= priv->frmp.r.width) { /* case i) New width is lesser or equal than current */

			// write width
			sel->r.width = clamp(roundup(sel->r.width, priv->frmp.sw), priv->frmp.minw, priv->frmp.maxw);
			avt_set_param(client, V4L2_AV_CSI2_WIDTH_W, sel->r.width);
			avt_set_param(client, V4L2_AV_CSI2_WIDTH_W, sel->r.width); // BUG: write twice in a row to ensure it works

			// update width, height, offset x/y restrictions from camera
			avt_init_frame_param(priv);

			// write offset x
			sel->r.left = clamp(roundup(sel->r.left, priv->frmp.swoff), priv->frmp.minwoff, priv->frmp.maxwoff);
			avt_set_param(client, V4L2_AV_CSI2_OFFSET_X_W, sel->r.left);
		} else { /* case ii) New width is higher than current */

			// write offset x
			sel->r.left = clamp(roundup(sel->r.left, priv->frmp.swoff), priv->frmp.minwoff, priv->frmp.maxwoff);
			avt_set_param(client, V4L2_AV_CSI2_OFFSET_X_W, sel->r.left);

			// update width, height, offset x/y restrictions from camera
			avt_init_frame_param(priv);

			// write width
			sel->r.width = clamp(roundup(sel->r.width, priv->frmp.sw), priv->frmp.minw, priv->frmp.maxw);
			avt_set_param(client, V4L2_AV_CSI2_WIDTH_W, sel->r.width);
			avt_set_param(client, V4L2_AV_CSI2_WIDTH_W, sel->r.width); // BUG: write twice in a row to ensure it works
		}

		if (sel->r.height <= priv->frmp.r.height) { /* case iii) New height is lesser or equal than current */

			// write height
			sel->r.height = clamp(roundup(sel->r.height, priv->frmp.sh), priv->frmp.minh, priv->frmp.maxh);
			avt_set_param(client, V4L2_AV_CSI2_HEIGHT_W, sel->r.height);

			// update width, height, offset x/y restrictions from camera
			avt_init_frame_param(priv);

			// write offset y
			sel->r.top = clamp(roundup(sel->r.top, priv->frmp.shoff), priv->frmp.minhoff, priv->frmp.maxhoff);
			avt_set_param(client, V4L2_AV_CSI2_OFFSET_Y_W, sel->r.top);
		} else { /* case iv) New height is higher than current */

			// write offset y
			sel->r.top = clamp(roundup(sel->r.top, priv->frmp.shoff), priv->frmp.minhoff, priv->frmp.maxhoff);
			avt_set_param(client, V4L2_AV_CSI2_OFFSET_Y_W, sel->r.top);

			// update width, height, offset x/y restrictions from camera
			avt_init_frame_param(priv);

			// write height
			sel->r.height = clamp(roundup(sel->r.height, priv->frmp.sh), priv->frmp.minh, priv->frmp.maxh);
			avt_set_param(client, V4L2_AV_CSI2_HEIGHT_W, sel->r.height);
		}

		// update width, height, offset x/y restrictions from camera
		avt_init_frame_param(priv);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int avt_s_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *ival)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	struct v4l2_fract *tpf = &(priv->streamcap.timeperframe);
	struct v4l2_ext_control vc;
	int ret;
	u64 value64, min, max, step;
	struct bcrm_to_v4l2 bcrm_v4l2;
	union bcrm_feature_reg feature_inquiry_reg;
	CLEAR(bcrm_v4l2);


	/* reading the Feature inquiry register */
	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + FEATURE_INQUIRY_REG_64R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
			(char *) &value64);
	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	feature_inquiry_reg.value = value64;

	/* Check if setting acquisition frame rate is available */
	if (!feature_inquiry_reg.feature_inq.acquisition_frame_rate) {
			avt_info(sd, "Acquisition frame rate setting not supported by firmware\n");
			return 0;
	}

	/* Copy new settings to internal structure */
	memcpy(&priv->streamcap.timeperframe, &ival->interval, sizeof(struct v4l2_fract));

	/* reading the Minimum Frame Rate Level */
	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + ACQUISITION_FRAME_RATE_MIN_64R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
			(char *) &value64);
	if (ret < 0) {
		avt_err(sd, "ACQUISITION_FRAME_RATE_MIN_64R: i2c read failed (%d)\n",
				ret);
		return ret;
	}

	bcrm_v4l2.min_bcrm = value64;

	/* reading the Maximum Frame Rate Level */
	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + ACQUISITION_FRAME_RATE_MAX_64R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
			(char *) &value64);
	if (ret < 0) {
		avt_err(sd, "ACQUISITION_FRAME_RATE_MAX_64R: i2c read failed (%d)\n",
				ret);
		return ret;
	}

	bcrm_v4l2.max_bcrm = value64;

	/* reading the Frame Rate Level step increment */
	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + ACQUISITION_FRAME_RATE_INCREMENT_64R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
			(char *) &value64);
	if (ret < 0) {
		avt_err(sd, "ACQUISITION_FRAME_RATE_INCREMENT_64R: i2c read failed (%d)\n",
				ret);
		return ret;
	}

	bcrm_v4l2.step_bcrm = value64;

	convert_bcrm_to_v4l2(&bcrm_v4l2, min_enum, true);
	convert_bcrm_to_v4l2(&bcrm_v4l2, max_enum, true);
	convert_bcrm_to_v4l2(&bcrm_v4l2, step_enum, true);

	min = bcrm_v4l2.min_v4l2;
	max = bcrm_v4l2.max_v4l2;
	/* Set step to 1 uHz because zero value came from camera register */
	if (!step)
		step = 1;

	if (min > max) {
		avt_err(sd, "Frame rate: min > max! (%llu > %llu)\n",
				min, max);
		return -EINVAL;
	}
	if (step <= 0) {
		avt_err(sd, "Frame rate: non-positive step value (%llu)!\n",
				step);
		return -EINVAL;
	}

	/* Translate timeperframe to frequency
	 * by inverting the fraction
	 */
	value64 = (tpf->denominator / tpf->numerator) * UHZ_TO_HZ;
	value64 = convert_s_ctrl(value64, min, max, step);
	if (value64 <= 0) {
		avt_err(sd, "Frame rate: non-positive value (%llu)!\n",
				value64);
		return -EINVAL;
	}

	/* Enable manual frame rate */
	vc.value = 1;
	ret = ioctl_bcrm_i2cwrite_reg(client, &vc, priv->cci_reg.bcrm_addr + FRAME_RATE_ENABLE_8RW, AV_CAM_DATA_SIZE_8);
	if (ret < 0) {
		avt_err(sd, "ACQUISITION_FRAME_RATE_64RW: i2c write failed (%d)\n",
				ret);
		return ret;
	}

	/* Save new frame rate to camera register */
	vc.value = value64;
	ret = ioctl_bcrm_i2cwrite_reg(client, &vc, priv->cci_reg.bcrm_addr + ACQUISITION_FRAME_RATE_64RW, AV_CAM_DATA_SIZE_64);
	if (ret < 0) {
		avt_err(sd, "ACQUISITION_FRAME_RATE_64RW: i2c write failed (%d)\n",
				ret);
		return ret;
	}

	tpf->numerator = UHZ_TO_HZ;
	tpf->denominator = value64;

	/* Copy modified settings back */
	memcpy(&ival->interval, &priv->streamcap.timeperframe, sizeof(struct v4l2_fract));

	return 0;
}

static int avt_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *ival)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = to_avt_csi2(client);

	if (ival->pad != 0)
		return -EINVAL;

	memcpy(&ival->interval, &priv->streamcap.timeperframe,
	       sizeof(struct v4l2_fract));

	return 0;
}

static int avt_csi2_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct avt_csi2_priv *priv = avt_get_priv(sd);

	// stop the stream
	int ret = avt_csi2_s_stream(sd, false);

	priv->is_open = false;
	return ret;
}

static int avt_csi2_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	// called when userspace app calls 'open'
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	int ret;
	uint32_t i2c_reg;
	uint32_t i2c_reg_size;
	uint32_t i2c_reg_count;
	uint8_t bcm_mode = 0;
	char *i2c_reg_buf;;

	if (priv->is_open) {
		return -EBUSY;
	}

#if 0
	// set stride align
	if (priv->stride_align_enabled)
		set_channel_stride_align_for_format(sd, priv->mbus_fmt_code);
	else
		set_channel_stride_align(sd, 1);
#endif

	// set BCRM mode
	CLEAR(i2c_reg);
	i2c_reg = GENCP_CHANGEMODE_8W;
	i2c_reg_size = AV_CAM_REG_SIZE;
	i2c_reg_count = AV_CAM_DATA_SIZE_8;
	i2c_reg_buf = (char *) &bcm_mode;

	ret = ioctl_gencam_i2cwrite_reg(priv->client,
			i2c_reg, i2c_reg_size,
			i2c_reg_count, i2c_reg_buf);
	if (ret < 0) {
		avt_err(sd, "Failed to set BCM mode: i2c write failed (%d)\n", ret);
		return ret;
	}
	priv->mode = AVT_BCRM_MODE;
	priv->is_open = true;

	return 0;
}

static int avt_csi2_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	return 0;
}

int avt_csi2_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int avt_csi2_get_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	int ret;
	u8 val;

	if (reg->reg & ~0xffff)
		return -EINVAL;

	ret = avt_reg_read(client, priv->cci_reg.bcrm_addr + reg->reg,
			   AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8, &val);
	if (ret < 0)
		return ret;

	reg->size = 1;
	reg->val = (__u64) val;

	return 0;
}

static int avt_csi2_set_register(struct v4l2_subdev *sd,
				 const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->reg & ~0xffff || reg->val & ~0xff)
		return -EINVAL;

	return avt_reg_write(client, reg->reg, reg->val);
}
#endif

static const struct v4l2_subdev_core_ops avt_csi2_core_ops = {
	.subscribe_event = avt_csi2_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
	.s_power = avt_csi2_s_power,
	.ioctl = avt_csi2_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = avt_csi2_get_register,
	.s_register = avt_csi2_set_register,
#endif
};

static const struct v4l2_subdev_internal_ops avt_csi2_int_ops = {
	.open = avt_csi2_open,
	.close = avt_csi2_close,
};

static const struct v4l2_subdev_video_ops avt_csi2_video_ops = {
	.s_stream = avt_csi2_s_stream,
	.s_frame_interval = avt_s_frame_interval,
	.g_frame_interval = avt_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops avt_csi2_pad_ops = {
	.set_fmt = avt_csi2_set_fmt,
	.get_fmt = avt_csi2_get_fmt,
	.enum_mbus_code = avt_csi2_enum_mbus_code,
	.enum_frame_size = avt_csi2_enum_framesizes,
	.enum_frame_interval = avt_csi2_enum_frameintervals,
	.get_selection = avt_get_selection,
	.set_selection = avt_set_selection,
	.get_mbus_config = avt_csi2_get_mbus_config,
};

static const struct v4l2_subdev_ops avt_csi2_subdev_ops = {
	.core = &avt_csi2_core_ops,
	.video = &avt_csi2_video_ops,
	.pad = &avt_csi2_pad_ops,
};

static const struct media_entity_operations avt_csi2_media_ops = {
	.link_validate = v4l2_subdev_link_validate,
	.link_setup = avt_csi2_link_setup,
};

static int read_cci_registers(struct i2c_client *client)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);

	int ret = 0;
	uint32_t crc = 0;
	uint32_t crc_byte_count = 0;

	uint32_t i2c_reg;
	uint32_t i2c_reg_size;
	uint32_t i2c_reg_count;
	uint32_t i2c_reg_left;

	char *i2c_reg_buf;

	i2c_reg = cci_cmd_tbl[CCI_REGISTER_LAYOUT_VERSION].address;
	i2c_reg_size = AV_CAM_REG_SIZE;
	/*
	 * Avoid last 3 bytes read as its WRITE only register except
	 * CURRENT MODE REG
	 */
	i2c_reg_left = i2c_reg_count = sizeof(priv->cci_reg) - 3;

	i2c_reg_buf = (char *)&priv->cci_reg;
	/* Calculate CRC from each reg up to the CRC reg */
	crc_byte_count =
		(uint32_t)((char *)&priv->cci_reg.checksum - (char *)&priv->cci_reg);

	dev_dbg(&client->dev, "crc_byte_count = %d, i2c_reg.count = %d\n",
			crc_byte_count, i2c_reg_count);

	/* read CCI registers */
	while (i2c_reg_left) {
		ret = i2c_read(client, i2c_reg + (i2c_reg_count - i2c_reg_left), i2c_reg_size,
			       i2c_reg_left > I2C_IO_LIMIT ? I2C_IO_LIMIT : i2c_reg_left,
			       i2c_reg_buf + (i2c_reg_count - i2c_reg_left));
		if (i2c_reg_left > I2C_IO_LIMIT)
			i2c_reg_left -= I2C_IO_LIMIT;
		else
			i2c_reg_left = 0;
	}

	if (ret < 0) {
		dev_err(&client->dev, "i2c read failed (%d)\n", ret);
		return ret;
	}

/* CRC calculation */
	crc = crc32(U32_MAX, &priv->cci_reg, crc_byte_count);

/* Swap bytes if neccessary */
	cpu_to_be32s(&priv->cci_reg.layout_version);
	cpu_to_be64s(&priv->cci_reg.device_capabilities);
	cpu_to_be16s(&priv->cci_reg.gcprm_address);
	cpu_to_be16s(&priv->cci_reg.bcrm_addr);
	cpu_to_be32s(&priv->cci_reg.checksum);

/* Check the checksum of received with calculated. */
	if (crc != priv->cci_reg.checksum) {
		dev_err(&client->dev,
			"wrong CCI CRC value! calculated = 0x%x, received = 0x%x\n",
			crc, priv->cci_reg.checksum);
		return -EINVAL;
	}

	dev_info(&client->dev, "cci layout version: %x\n",
			priv->cci_reg.layout_version);
	dev_info(&client->dev, "cci device capabilities: %llx\n",
			priv->cci_reg.device_capabilities);
	dev_info(&client->dev, "cci device guid: %s\n",
			priv->cci_reg.device_guid);
	dev_info(&client->dev, "cci gcprm_address: 0x%x\n",
			priv->cci_reg.gcprm_address);

	return 0;
}

static int read_gencp_registers(struct i2c_client *client)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);

	int ret = 0;
	uint32_t crc = 0;
	uint32_t crc_byte_count = 0;

	uint32_t i2c_reg;
	uint32_t i2c_reg_size;
	uint32_t i2c_reg_count;

	char *i2c_reg_buf;

	i2c_reg = priv->cci_reg.gcprm_address + 0x0000;
	i2c_reg_size = AV_CAM_REG_SIZE;
	i2c_reg_count = sizeof(priv->gencp_reg);
	i2c_reg_buf = (char *)&priv->gencp_reg;

	/* Calculate CRC from each reg up to the CRC reg */
	crc_byte_count =
		(uint32_t)((char *)&priv->gencp_reg.checksum - (char *)&priv->gencp_reg);

	ret = i2c_read(client, i2c_reg, i2c_reg_size,
			i2c_reg_count, i2c_reg_buf);

	crc = crc32(U32_MAX, &priv->gencp_reg, crc_byte_count);

	if (ret < 0) {
		pr_err("%s : I2C read failed, ret %d\n", __func__, ret);
		return ret;
	}

	be32_to_cpus(&priv->gencp_reg.gcprm_layout_version);
	be16_to_cpus(&priv->gencp_reg.gencp_out_buffer_address);
	be16_to_cpus(&priv->gencp_reg.gencp_in_buffer_address);
	be16_to_cpus(&priv->gencp_reg.gencp_out_buffer_size);
	be16_to_cpus(&priv->gencp_reg.gencp_in_buffer_size);
	be32_to_cpus(&priv->gencp_reg.checksum);

	if (crc != priv->gencp_reg.checksum) {
		dev_warn(&client->dev,
			"wrong GENCP CRC value! calculated = 0x%x, received = 0x%x\n",
			crc, priv->gencp_reg.checksum);
	}

	dev_info(&client->dev, "gcprm layout version: %x\n",
		priv->gencp_reg.gcprm_layout_version);
	dev_info(&client->dev, "gcprm out buf addr: %x\n",
		priv->gencp_reg.gencp_out_buffer_address);
	dev_info(&client->dev, "gcprm out buf size: %x\n",
		priv->gencp_reg.gencp_out_buffer_size);
	dev_info(&client->dev, "gcprm in buf addr: %x\n",
		priv->gencp_reg.gencp_in_buffer_address);
	dev_info(&client->dev, "gcprm in buf size: %x\n",
		priv->gencp_reg.gencp_in_buffer_size);

	return 0;
}

static int cci_version_check(struct i2c_client *client)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	uint32_t cci_minver, cci_majver;

	cci_minver = (priv->cci_reg.layout_version & CCI_REG_LAYOUT_MINVER_MASK)
				>> CCI_REG_LAYOUT_MINVER_SHIFT;

	if (cci_minver == CCI_REG_LAYOUT_MINVER) {
		dev_dbg(&client->dev, "%s: correct cci register minver: %d (0x%x)\n",
				__func__, cci_minver, priv->cci_reg.layout_version);
	} else {
		dev_err(&client->dev, "%s: cci reg minver mismatch! read: %d (0x%x) expected: %d\n",
				__func__, cci_minver, priv->cci_reg.layout_version,
				CCI_REG_LAYOUT_MINVER);
		return -EINVAL;
	}

	cci_majver = (priv->cci_reg.layout_version & CCI_REG_LAYOUT_MAJVER_MASK)
				>> CCI_REG_LAYOUT_MAJVER_SHIFT;

	if (cci_majver == CCI_REG_LAYOUT_MAJVER) {
		dev_dbg(&client->dev, "%s: correct cci register majver: %d (0x%x)\n",
				__func__, cci_majver, priv->cci_reg.layout_version);
	} else {
		dev_err(&client->dev, "%s: cci reg majver mismatch! read: %d (0x%x) expected: %d\n",
				__func__, cci_majver, priv->cci_reg.layout_version,
				CCI_REG_LAYOUT_MAJVER);
		return -EINVAL;
	}

	return 0;
}

static int bcrm_version_check(struct i2c_client *client)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	u32 value = 0;
	int ret;

	/* reading the BCRM version */
	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + BCRM_VERSION_REG_32R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
			(char *) &value);

	if (ret < 0) {
		dev_err(&client->dev, "i2c read failed (%d)\n", ret);
		return ret;
	}

	dev_info(&client->dev, "bcrm version (driver): 0x%x (maj: 0x%x min: 0x%x)\n",
						BCRM_DEVICE_VERSION,
						BCRM_MAJOR_VERSION,
						BCRM_MINOR_VERSION);

	dev_info(&client->dev, "bcrm version (camera): 0x%x (maj: 0x%x min: 0x%x)\n",
						value,
						(value & 0xffff0000) >> 16,
						(value & 0x0000ffff));

	return (value >> 16) == BCRM_MAJOR_VERSION ? 1 : 0;
}

static ssize_t availability_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%d\n", priv->stream_on ? 1 : 0);
}

static int gcprm_version_check(struct i2c_client *client)
{
	struct avt_csi2_priv *priv = to_avt_csi2(client);
	u32 value = priv->gencp_reg.gcprm_layout_version;

	dev_info(&client->dev, "gcprm version (driver): 0x%x (maj: 0x%x min: 0x%x)\n",
						GCPRM_DEVICE_VERSION,
						GCPRM_MAJOR_VERSION,
						GCPRM_MINOR_VERSION);

	dev_info(&client->dev, "gcprm version (camera): 0x%x (maj: 0x%x min: 0x%x)\n",
						value,
						(value & 0xffff0000) >> 16,
						(value & 0x0000ffff));

	return (value & 0xffff0000) >> 16 == GCPRM_MAJOR_VERSION ? 1 : 0;
}

static ssize_t cci_register_layout_version_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%d\n", priv->cci_reg.layout_version);
}

static ssize_t device_capabilities_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%llu\n", priv->cci_reg.device_capabilities);
}

static ssize_t device_guid_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%s\n", priv->cci_reg.device_guid);
}

static ssize_t manufacturer_name_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%s\n", priv->cci_reg.manufacturer_name);
}

static ssize_t model_name_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%s\n", priv->cci_reg.model_name);
}

static ssize_t family_name_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%s\n", priv->cci_reg.family_name);
}

static ssize_t lane_count_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%d\n", priv->numlanes);
}

static ssize_t device_version_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%s\n", priv->cci_reg.device_version);
}

static ssize_t manufacturer_info_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%s\n", priv->cci_reg.manufacturer_info);
}

static ssize_t serial_number_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%s\n", priv->cci_reg.serial_number);
}

static ssize_t user_defined_name_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct avt_csi2_priv *priv = to_avt_csi2(to_i2c_client(dev));

	return sprintf(buf, "%s\n", priv->cci_reg.user_defined_name);
}

static ssize_t driver_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d:%d:%d:%d\n",
			MAJOR_DRV, MINOR_DRV, PATCH_DRV, BUILD_DRV);
}

static ssize_t debug_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", debug);
}

static ssize_t debug_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 10, &debug);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR_RO(availability);
static DEVICE_ATTR_RO(cci_register_layout_version);
static DEVICE_ATTR_RO(device_capabilities);
static DEVICE_ATTR_RO(device_guid);
static DEVICE_ATTR_RO(device_version);
static DEVICE_ATTR_RO(driver_version);
static DEVICE_ATTR_RO(family_name);
static DEVICE_ATTR_RO(lane_count);
static DEVICE_ATTR_RO(manufacturer_info);
static DEVICE_ATTR_RO(manufacturer_name);
static DEVICE_ATTR_RO(model_name);
static DEVICE_ATTR_RO(serial_number);
static DEVICE_ATTR_RO(user_defined_name);
static DEVICE_ATTR_RW(debug_en);

static struct attribute *avt_csi2_attrs[] = {
	&dev_attr_availability.attr,
	&dev_attr_cci_register_layout_version.attr,
	&dev_attr_device_capabilities.attr,
	&dev_attr_device_guid.attr,
	&dev_attr_device_version.attr,
	&dev_attr_driver_version.attr,
	&dev_attr_family_name.attr,
	&dev_attr_lane_count.attr,
	&dev_attr_manufacturer_info.attr,
	&dev_attr_manufacturer_name.attr,
	&dev_attr_model_name.attr,
	&dev_attr_serial_number.attr,
	&dev_attr_user_defined_name.attr,
	&dev_attr_debug_en.attr,
	NULL
};

static struct attribute_group avt_csi2_attr_grp = {
	.attrs = avt_csi2_attrs,
};

static bool common_range(uint32_t nMin1, uint32_t nMax1, uint32_t nInc1,
				uint32_t nMin2, uint32_t nMax2, uint32_t nInc2,
				uint32_t *rMin, uint32_t *rMax, uint32_t *rInc)
{
	bool bResult = false;

	uint32_t nMin = max(nMin1, nMin2);
	uint32_t nMax = min(nMax1, nMax2);

	/* Check if it is overlapping at all */
	if (nMax >= nMin) {
		/* if both minima are equal,
		 * then the computation is a bit simpler
		 */
		if (nMin1 == nMin2) {
			uint32_t nLCM = lcm(nInc1, nInc2);
			*rMin = nMin;
			*rMax = nMax - ((nMax - nMin) % nLCM);

			if (*rMin == *rMax)
				*rInc = 1;
			else
				*rInc = nLCM;

			bResult = true;
		} else if (nMin1 > nMin2) {
			/* Find the first value that is ok for Host and BCRM */
			uint32_t nMin1Shifted = nMin1 - nMin2;
			uint32_t nMaxShifted = nMax - nMin2;
			uint32_t nValue = nMin1Shifted;

			for (; nValue <= nMaxShifted; nValue += nInc1) {
				if ((nValue % nInc2) == 0)
					break;
			}

			/* Compute common increment and maximum */
			if (nValue <= nMaxShifted) {
				uint32_t nLCM = lcm(nInc1, nInc2);
				*rMin = nValue + nMin2;
				*rMax = nMax - ((nMax - *rMin) % nLCM);

				if (*rMin == *rMax)
					*rInc = 1;
				else
					*rInc = nLCM;

				bResult = true;
			}
		} else {
			/* Find the first value that is ok for Host and BCRM */
			uint32_t nMin2Shifted = nMin2 - nMin1;
			uint32_t nMaxShifted = nMax - nMin1;
			uint32_t nValue = nMin2Shifted;

			for (; nValue <= nMaxShifted; nValue += nInc2) {
				if ((nValue % nInc1) == 0)
					break;
			}

			/* Compute common increment and maximum */
			if (nValue <= nMaxShifted) {
				uint32_t nLCM = lcm(nInc2, nInc1);
				*rMin = nValue + nMin1;
				*rMax = nMax - ((nMax - *rMin) % nLCM);
				if (*rMin == *rMax)
					*rInc = 1;
				else
					*rInc = nLCM;

				bResult = true;
			}
		}
	}

	return bResult;
}

static int avt_init_frame_param(struct avt_csi2_priv *priv)
{
	if (avt_get_param(priv->client, V4L2_AV_CSI2_HEIGHT_MINVAL_R,
				&priv->frmp.minh))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_HEIGHT_MAXVAL_R,
				&priv->frmp.maxh))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_HEIGHT_INCVAL_R,
				&priv->frmp.sh))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_WIDTH_MINVAL_R,
				&priv->frmp.minw))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_WIDTH_MAXVAL_R,
				&priv->frmp.maxw))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_WIDTH_INCVAL_R,
				&priv->frmp.sw))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_OFFSET_Y_MIN_R,
				&priv->frmp.minhoff))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_OFFSET_Y_MAX_R,
				&priv->frmp.maxhoff))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_OFFSET_Y_INC_R,
				&priv->frmp.shoff))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_OFFSET_X_MIN_R,
				&priv->frmp.minwoff))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_OFFSET_X_MAX_R,
				&priv->frmp.maxwoff))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_OFFSET_X_INC_R,
				&priv->frmp.swoff))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_WIDTH_R,
				&priv->frmp.r.width))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_HEIGHT_R,
				&priv->frmp.r.height))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_OFFSET_X_R,
				&priv->frmp.r.left))
		return -EINVAL;

	if (avt_get_param(priv->client, V4L2_AV_CSI2_OFFSET_Y_R,
				&priv->frmp.r.top))
		return -EINVAL;

	return 0;
}

/* Read image format from camera,
 * should be only called once, during initialization
 * */
static int avt_read_fmt_from_device(struct v4l2_subdev *sd, uint32_t *fmt)
{
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	struct i2c_client *client = priv->client;
	uint32_t avt_img_fmt = 0;
	uint8_t bayer_pattern;
	int ret = 0;

	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + IMG_BAYER_PATTERN_8RW,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
			(char *) &bayer_pattern);

	if (ret < 0) {
		dev_err(&client->dev, "i2c read failed (%d)\n", ret);
		return ret;
	}

	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + IMG_MIPI_DATA_FORMAT_32RW,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
			(char *) &avt_img_fmt);

	if (ret < 0) {
		dev_err(&client->dev, "i2c read failed (%d)\n", ret);
		return ret;
	}

	switch (avt_img_fmt) {
	case	MIPI_DT_RGB888:
		avt_img_fmt = MEDIA_BUS_FMT_RGB888_1X24;
		break;
	case	MIPI_DT_RGB565:
		avt_img_fmt = MEDIA_BUS_FMT_RGB565_1X16;
		break;
	case	MIPI_DT_YUV422:
		avt_img_fmt = MEDIA_BUS_FMT_VYUY8_2X8;
		break;
	case	MIPI_DT_CUSTOM:
		avt_img_fmt = 0;
		break;
	case	MIPI_DT_RAW8:
		switch (bayer_pattern) {
		case	monochrome:
			avt_img_fmt = MEDIA_BUS_FMT_Y8_1X8;
			break;
		case	bayer_gr:
			avt_img_fmt = MEDIA_BUS_FMT_SGRBG8_1X8;
			break;
		case	bayer_rg:
			avt_img_fmt = MEDIA_BUS_FMT_SRGGB8_1X8;
			break;
		case	bayer_gb:
			avt_img_fmt = MEDIA_BUS_FMT_SGBRG8_1X8;
			break;
		case	bayer_bg:
			avt_img_fmt = MEDIA_BUS_FMT_SBGGR8_1X8;
			break;
		default:
			dev_err(&client->dev, "%s: Unknown RAW8 pixelformat read"
				" bayer_pattern %d\n", __func__, bayer_pattern);
			return -EINVAL;
		}
		break;
	case	MIPI_DT_RAW10:
		switch (bayer_pattern) {
		case	monochrome:
			avt_img_fmt = MEDIA_BUS_FMT_Y10_1X10;
			break;
		case	bayer_gr:
			avt_img_fmt = MEDIA_BUS_FMT_SGRBG10_1X10;
			break;
		case	bayer_rg:
			avt_img_fmt = MEDIA_BUS_FMT_SRGGB10_1X10;
			break;
		case	bayer_gb:
			avt_img_fmt = MEDIA_BUS_FMT_SGBRG10_1X10;
			break;
		case	bayer_bg:
			avt_img_fmt = MEDIA_BUS_FMT_SBGGR10_1X10;
			break;
		default:
			dev_err(&client->dev, "%s:Unknown RAW10 pixelformat read"
				" bayer_pattern %d\n", __func__, bayer_pattern);
			return -EINVAL;
		}
		break;
	case	MIPI_DT_RAW12:
		switch (bayer_pattern) {
		case	monochrome:
			avt_img_fmt = MEDIA_BUS_FMT_Y12_1X12;
			break;
		case	bayer_gr:
			avt_img_fmt = MEDIA_BUS_FMT_SGRBG12_1X12;
			break;
		case	bayer_rg:
			avt_img_fmt = MEDIA_BUS_FMT_SRGGB12_1X12;
			break;
		case	bayer_gb:
			avt_img_fmt = MEDIA_BUS_FMT_SGBRG12_1X12;
			break;
		case	bayer_bg:
			avt_img_fmt = MEDIA_BUS_FMT_SBGGR12_1X12;
			break;
		default:
			dev_err(&client->dev, "%s:Unknown RAW12 pixelformat read"
				" bayer_pattern %d\n", __func__, bayer_pattern);
			return -EINVAL;
		}
		break;

	case	0:
		dev_warn(&client->dev, "Invalid pixelformat detected: "
			 "avt_img_fmt=0x%x. Fallback app running?", avt_img_fmt);
		avt_img_fmt = MEDIA_BUS_FMT_RGB888_1X24;
		break;

	default:
		dev_err(&client->dev, "%s: Unknown pixelformat read, "
			"avt_img_fmt 0x%x\n", __func__, avt_img_fmt);
		return -EINVAL;
	}

	*fmt = avt_img_fmt;

	return 0;
}

static int avt_init_mode(struct v4l2_subdev *sd)
{
	struct avt_csi2_priv *priv = avt_get_priv(sd);
	int ret = 0;
	uint32_t common_min_clk = 0;
	uint32_t common_max_clk = 0;
	uint32_t common_inc_clk = 0;
	uint32_t avt_min_clk = 0;
	uint32_t avt_max_clk = 0;
	uint32_t host_max_clk = 0;
	uint8_t avt_supported_lane_counts = 0;

	uint32_t i2c_reg;
	uint32_t i2c_reg_size;
	uint32_t i2c_reg_count;
	uint32_t clk;
	uint8_t bcm_mode = 0;

	char *i2c_reg_buf;
	struct v4l2_subdev_selection sel;

	/* Check if requested number of lanes is supported */
	ret = avt_reg_read(priv->client,
			priv->cci_reg.bcrm_addr + SUPPORTED_CSI2_LANE_COUNTS_8R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_8,
			(char *) &avt_supported_lane_counts);
	avt_err(sd, " -> supported lane count: %x\n", (char)avt_supported_lane_counts);
	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	// [mjsob] TODO: proper numlanes support
	if (!(test_bit(priv->numlanes - 1, (const long *)(&avt_supported_lane_counts)))) {
		avt_err(sd, "requested number of lanes (%u) not supported by this camera!\n",
				priv->numlanes);
		return -EINVAL;
	}

	/* Set number of lanes */
	ret = avt_reg_write(priv->client,
			priv->cci_reg.bcrm_addr + CSI2_LANE_COUNT_8RW,
			priv->numlanes);
	if (ret < 0) {
		avt_err(sd, "i2c write failed (%d)\n", ret);
		return ret;
	}

	ret = avt_reg_read(priv->client,
			priv->cci_reg.bcrm_addr + CSI2_CLOCK_MIN_32R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
			(char *) &avt_min_clk);

	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	ret = avt_reg_read(priv->client,
			priv->cci_reg.bcrm_addr + CSI2_CLOCK_MAX_32R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
			(char *) &avt_max_clk);

	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	if (priv->numlanes == 4) {
		host_max_clk = CSI_HOST_CLK_MAX_FREQ_4L;
	} else {
		host_max_clk = CSI_HOST_CLK_MAX_FREQ;
	}

	avt_dbg(1, sd, "csi clock camera range: %d:%d Hz, host range: %d:%d Hz\n",
		avt_min_clk, avt_max_clk,
		CSI_HOST_CLK_MIN_FREQ, host_max_clk);

	if (common_range(avt_min_clk, avt_max_clk, 1,
			CSI_HOST_CLK_MIN_FREQ, host_max_clk, 1,
			&common_min_clk, &common_max_clk, &common_inc_clk)
			== false) {
		avt_err(sd, "no common clock range for camera and host possible!\n");
		return -EINVAL;
	}

	avt_dbg(1, sd, "camera/host common csi clock range: %d:%d Hz\n",
			common_min_clk, common_max_clk);

	if (priv->csi_clk_freq == 0) {
		avt_dbg(1, sd, "no csi clock requested, using common max (%d Hz)\n",
				common_max_clk);
		priv->csi_clk_freq = common_max_clk;
	} else {
		avt_dbg(1, sd, "using csi clock from dts: %u Hz\n",
				priv->csi_clk_freq);
	}

	if ((priv->csi_clk_freq < common_min_clk) ||
			(priv->csi_clk_freq > common_max_clk)) {
		avt_err(sd, "unsupported csi clock frequency (%d Hz, range: %d:%d Hz)!\n",
				priv->csi_clk_freq, common_min_clk,
				common_max_clk);
		return -EINVAL;
	}

	CLEAR(i2c_reg);
	clk = priv->csi_clk_freq;
	swapbytes(&clk, AV_CAM_DATA_SIZE_32);
	i2c_reg = priv->cci_reg.bcrm_addr + CSI2_CLOCK_32RW;
	i2c_reg_size = AV_CAM_REG_SIZE;
	i2c_reg_count = AV_CAM_DATA_SIZE_32;
	i2c_reg_buf = (char *) &clk;
	ret = ioctl_gencam_i2cwrite_reg(priv->client, i2c_reg, i2c_reg_size,
					i2c_reg_count, i2c_reg_buf);

	ret = avt_reg_read(priv->client,
			priv->cci_reg.bcrm_addr + CSI2_CLOCK_32RW,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_32,
			(char *) &avt_max_clk);

	if (ret < 0) {
		avt_err(sd, "i2c read failed (%d)\n", ret);
		return ret;
	}

	avt_dbg(1, sd, "csi clock read from camera: %d Hz\n", avt_max_clk);

	ret = avt_read_fmt_from_device(sd, &(priv->mbus_fmt_code));
	if (ret < 0)
		return ret;

	ret = avt_init_frame_param(priv);
	if (ret < 0)
		return ret;

	sel.target = V4L2_SEL_TGT_CROP;
	sel.r = priv->frmp.r;
	ret = avt_set_selection(sd, NULL, &sel);
	if (ret < 0)
		return ret;

	// set BCRM mode
	CLEAR(i2c_reg);
	i2c_reg = GENCP_CHANGEMODE_8W;
	i2c_reg_size = AV_CAM_REG_SIZE;
	i2c_reg_count = AV_CAM_DATA_SIZE_8;
	i2c_reg_buf = (char *) &bcm_mode;

	ret = ioctl_gencam_i2cwrite_reg(priv->client,
			i2c_reg, i2c_reg_size,
			i2c_reg_count, i2c_reg_buf);
	if (ret < 0) {
		avt_err(sd, "Failed to set BCM mode: i2c write failed (%d)\n", ret);
		return ret;
	}
	priv->mode = AVT_BCRM_MODE;

	return 0;
}

static int avt_csi2_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct avt_csi2_priv *priv;
	struct device *dev = &client->dev;
	int ret;

	struct v4l2_fwnode_endpoint endpoint = { .bus_type = V4L2_MBUS_CSI2_DPHY };
	struct fwnode_handle *ep;
	struct fwnode_handle *fwnode = dev_fwnode(dev);
	struct v4l2_queryctrl qctrl;
	struct v4l2_ctrl *ctrl;
	union cci_device_caps_reg device_caps;
	struct v4l2_subdev_format format;
	int i, j;
	u64 value64 = 0;

	v4l_dbg(1, debug, client, "chip found @ 0x%x (%s)\n",
		client->addr << 1, client->adapter->name);

	priv = devm_kzalloc(&client->dev, sizeof(struct avt_csi2_priv),
			GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->subdev.ctrl_handler = &priv->hdl;
	priv->client = client;

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep) {
		dev_err(dev, "missing endpoint node\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(ep, &endpoint);
	fwnode_handle_put(ep);
	if (ret) {
		dev_err(dev, "failed to parse endpoint\n");
		return ret;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &avt_csi2_subdev_ops);
	priv->subdev.internal_ops = &avt_csi2_int_ops;
	priv->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	priv->subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	priv->pads[AVT_CSI2_SENS_PAD_SOURCE_0].flags = MEDIA_PAD_FL_SOURCE;
	priv->pads[AVT_CSI2_SENS_PAD_SOURCE_1].flags = MEDIA_PAD_FL_SOURCE;
	priv->pads[AVT_CSI2_SENS_PAD_SOURCE_2].flags = MEDIA_PAD_FL_SOURCE;
	priv->pads[AVT_CSI2_SENS_PAD_SOURCE_3].flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&priv->subdev.entity,
			AVT_CSI2_SENS_PADS_NUM,
			priv->pads);
	if (ret < 0)
		return ret;

	priv->subdev.entity.ops = &avt_csi2_media_ops;

	priv->subdev.dev = &client->dev;

	priv->streamcap.capability = V4L2_MODE_HIGHQUALITY |
					V4L2_CAP_TIMEPERFRAME;
	priv->streamcap.capturemode = 0;
	/* Set 60/1 Fps at the camera start */
	priv->streamcap.timeperframe.denominator = DEFAULT_FPS;
	priv->streamcap.timeperframe.numerator = 1;

	// [msob] XXX: without delay, cci register read fails
	msleep(2000);

	ret = read_cci_registers(client);

	if (ret < 0) {
		dev_err(dev, "%s: read_cci_registers failed: %d\n",
				__func__, ret);
		return ret;
	}

	ret = cci_version_check(client);
	if (ret < 0) {
		dev_err(&client->dev, "cci version mismatch!\n");
		return ret;
	}

	ret = bcrm_version_check(client);
	if (ret < 0) {
		dev_err(&client->dev, "bcrm version mismatch!\n");
		return ret;
	}

	dev_dbg(&client->dev, "correct bcrm version\n");

	priv->write_handshake_available =
		bcrm_get_write_handshake_availibility(client);

	avt_init_avail_formats(&priv->subdev);

	/* reading the Firmware Version register */
	ret = avt_reg_read(client,
			priv->cci_reg.bcrm_addr + FIRMWARE_VERSION_REG_64R,
			AV_CAM_REG_SIZE, AV_CAM_DATA_SIZE_64,
			(char *) &value64);

	dev_info(&client->dev, "Firmware version: %llu.%llu.%llu.%llu ret = %d\n",
						(value64 & 0x000000ff),
						(value64 & 0x0000ff00) >> 8,
						(value64 & 0x00ff0000) >> 16,
						(value64 & 0xff000000) >> 24,
						ret);

	device_caps.value = priv->cci_reg.device_capabilities;

//TODO: GENCP

	if (device_caps.caps.gencp) {
		ret = read_gencp_registers(client);
		if (ret < 0) {
			dev_err(dev, "%s: read_gencp_registers failed: %d\n",
					__func__, ret);
			return ret;
		}

		ret = gcprm_version_check(client);
		if (ret < 0) {
			dev_err(&client->dev, "gcprm version mismatch!\n");
			return ret;
		}

		dev_dbg(&client->dev, "correct gcprm version\n");
	}


	ret = sysfs_create_group(&dev->kobj, &avt_csi2_attr_grp);
	dev_err(dev, " -> %s: sysfs group created! (%d)\n", __func__, ret);
	if (ret) {
		dev_err(dev, "Failed to create sysfs group (%d)\n", ret);
		return ret;
	}

	v4l2_ctrl_handler_init(&priv->hdl, ARRAY_SIZE(avt_ctrl_mappings));

	for (i = 0, j = 0; j < ARRAY_SIZE(avt_ctrl_mappings); ++j) {
		CLEAR(qctrl);
		qctrl.id = avt_ctrl_mappings[j].id;

		ret = ioctl_queryctrl(&priv->subdev, &qctrl);
		if (ret < 0)
			continue;

		dev_dbg(&client->dev, "Checking caps: %s - Range: %d-%d s: %d d: %d - %sabled\n",
			avt_ctrl_mappings[j].attr.name,
			qctrl.minimum,
			qctrl.maximum,
			qctrl.step,
			qctrl.default_value,
			(qctrl.flags & V4L2_CTRL_FLAG_DISABLED) ?
			"dis" : "en");

		if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
			continue;

		priv->ctrl_cfg[i].type = qctrl.type;

		if (qctrl.type == V4L2_CTRL_TYPE_INTEGER)
			priv->ctrl_cfg[i].flags |= V4L2_CTRL_FLAG_SLIDER;

		priv->ctrl_cfg[i].ops = &avt_ctrl_ops;
		priv->ctrl_cfg[i].name = avt_ctrl_mappings[j].attr.name;
		priv->ctrl_cfg[i].id = avt_ctrl_mappings[j].id;

		priv->ctrl_cfg[i].min = qctrl.minimum;
		priv->ctrl_cfg[i].max = qctrl.maximum;
		priv->ctrl_cfg[i].def = qctrl.default_value;
		priv->ctrl_cfg[i].step = qctrl.step;

		ctrl = v4l2_ctrl_new_custom(&priv->hdl,
			&priv->ctrl_cfg[i], NULL);

		if (ctrl == NULL) {
			dev_err(&client->dev, "Failed to init %s ctrl\n",
				priv->ctrl_cfg[i].name);
			continue;
		}

		priv->ctrls[i] = ctrl;
		i++;
	}
	ret = v4l2_ctrl_handler_setup(priv->subdev.ctrl_handler);

	if (of_property_read_u32(dev->of_node,
				"csi_clk_freq",
				&priv->csi_clk_freq))
		priv->csi_clk_freq = 0;

	// [msob] TODO: proper numlanes support
	priv->numlanes = 4;

	priv->stream_on = false;
	priv->cross_update = false;
	priv->is_open = false;

	ret = avt_init_mode(&priv->subdev);
	if (ret < 0)
		return ret;

	ret = v4l2_async_register_subdev(&priv->subdev);
	if (ret < 0)
		return ret;

	format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = MEDIA_BUS_FMT_VYUY8_2X8;
	avt_csi2_set_fmt(&priv->subdev, NULL, &format);

	dev_info(&client->dev, "sensor %s registered\n",
			priv->subdev.name);

	return 0;
}

static int avt_csi2_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &avt_csi2_attr_grp);

	v4l2_async_unregister_subdev(sd);
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);

	return 0;
}

const struct of_device_id avt_csi2_of_match[] = {
	{ .compatible = "alliedvision,avt_csi2",},
	{ },
};

MODULE_DEVICE_TABLE(of, avt_csi2_of_match);


static struct i2c_device_id avt_csi2_id[] = {
	{"avt_csi2", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, avt_csi2_id);

static struct i2c_driver avt_csi2_driver = {
	.driver = {
		.name = "avt_csi2",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(avt_csi2_of_match),
	},
	.probe = avt_csi2_probe,
	.remove = avt_csi2_remove,
	.id_table = avt_csi2_id,
};

module_i2c_driver(avt_csi2_driver);

MODULE_AUTHOR("Allied Vision Inc.");
MODULE_DESCRIPTION("Allied Vision's MIPI-CSI2 Camera Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("l4t-0.1");
