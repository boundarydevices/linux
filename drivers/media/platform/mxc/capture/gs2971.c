/* SEMTECH SD SDI reciever
 *
 * Copyright (C) 2014 RidgeRun
 * Author: Edison Fernández <edison.fernández@ridgerun.com>
 *
 * Currently working with 1080p30 and 720p60 with no audio support.
 * In order to support more standars the VD_STD value should be
 * used in order to identify the incomming resolution and framerate.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*!
 * @file gs2971.c
 *
 * @brief SEMTECH SD SDI reciever
 *
 * @ingroup Camera
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/videodev2.h>
#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>
#include <media/v4l2-chip-ident.h>
#include "mxc_v4l2_capture.h"
#include <linux/proc_fs.h>

#include "gs2971.h"

#define DRIVER_NAME     "gs2971"

/* Debug functions */
static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-2)");

static struct gs2971_spidata *spidata;

#define YUV_FORMAT(gs) ((gs->cea861) ? V4L2_PIX_FMT_UYVY : V4L2_PIX_FMT_YUYV)

/*! @brief This mutex is used to provide mutual exclusion.
 *
 *  Create a mutex that can be used to provide mutually exclusive
 *  read/write access to the globally accessible data structures
 *  and variables that were defined above.
 */
static DEFINE_MUTEX(mutex);

int gs2971_read_buffer(struct spi_device *spi, u16 offset, u16 * values,
		       int length)
{
	struct spi_message msg;
	struct spi_transfer spi_xfer;
	int status;
	struct gs2971_spidata *sp = spidata;

	if (!spi)
		return -ENODEV;

	if (length > GS2971_SPI_TRANSFER_MAX)
		return -EINVAL;

	sp->txbuf[0] = 0x8000 | (offset & 0xfff);	/* read */
	if (length > 1)
		sp->txbuf[0] |= 0x1000;	/* autoinc */

	memset(&spi_xfer, '\0', sizeof(spi_xfer));
	spi_xfer.tx_buf = sp->txbuf;
	spi_xfer.rx_buf = sp->rxbuf;
	spi_xfer.bits_per_word = 16;
	spi_xfer.delay_usecs = 0;
	spi_xfer.speed_hz = 1000000;
	spi_xfer.len = sizeof(*values) * (length + 1);	// length in bytes

	spi_message_init(&msg);
	spi_message_add_tail(&spi_xfer, &msg);

	status = spi_sync(spi, &msg);

	memcpy(values, &sp->rxbuf[1], sizeof(*values) * length);
	if (status)
		pr_err(">> Read buffer(%d) (%x): %x %x, %x %x\n", status,
		       offset, sp->txbuf[0], sp->txbuf[1], sp->rxbuf[0],
		       sp->rxbuf[1]);

	return status;
}

int gs2971_read_register(struct spi_device *spi, u16 offset, u16 * value)
{
	struct spi_message msg;
	struct spi_transfer spi_xfer;
	int status;
	u32 txbuf[1];
	u32 rxbuf[1];

	if (!spi)
		return -ENODEV;

	txbuf[0] = 0x80000000 | ((offset & 0xfff) << 16);	/* read */

	memset(&spi_xfer, '\0', sizeof(spi_xfer));
	spi_xfer.tx_buf = txbuf;
	spi_xfer.rx_buf = rxbuf;
	spi_xfer.bits_per_word = 32;
	spi_xfer.delay_usecs = 0;
	spi_xfer.speed_hz = 1000000;
	spi_xfer.len = 4;	// length in bytes

	spi_message_init(&msg);
	spi_message_add_tail(&spi_xfer, &msg);

	status = spi_sync(spi, &msg);

	if (status) {
		dev_dbg(&spi->dev, "read_reg failed\n");
		*value = 0xffff;
	} else {
		*value = (u16) rxbuf[0];
	}

	if (status)
		pr_err(">> Read register(%d) (%x): %x, %x\n", status, offset,
		       txbuf[0], rxbuf[0]);

	pr_debug("--> Read register (%x) with value: %x", offset, *value);
	return status;
}

int gs2971_write_buffer(struct spi_device *spi, u16 offset, u16 * values,
			int length)
{
	struct spi_message msg;
	struct spi_transfer spi_xfer;
	int status;
	struct gs2971_spidata *sp = spidata;

	if (!spi)
		return -ENODEV;

	if (length > GS2971_SPI_TRANSFER_MAX - 1)
		return -EINVAL;

	sp->txbuf[0] = (offset & 0xfff);	/* write  */
	if (length > 1)
		sp->txbuf[0] |= 0x1000;	/* autoinc */
	memcpy(&sp->txbuf[1], values, sizeof(*values) * length);

	memset(&spi_xfer, '\0', sizeof(spi_xfer));
	spi_xfer.tx_buf = sp->txbuf;
	spi_xfer.rx_buf = sp->rxbuf;
	spi_xfer.bits_per_word = 16;
	spi_xfer.delay_usecs = 0;
	spi_xfer.speed_hz = 1000000;
	spi_xfer.len = sizeof(*values) * (length + 1);	// length in bytes

	spi_message_init(&msg);
	spi_message_add_tail(&spi_xfer, &msg);

	status = spi_sync(spi, &msg);
	if (status)
		pr_err(">> Write register(%d) (%x): %x %x, %x %x\n", status,
		       offset, sp->txbuf[0], sp->txbuf[1], sp->rxbuf[0],
		       sp->rxbuf[1]);
	return status;
}

int gs2971_write_register(struct spi_device *spi, u16 offset, u16 value)
{
	pr_debug("--> Writing to address (%x) : %x\n", offset, value);
	return gs2971_write_buffer(spi, offset, &value, 1);
}

static const char *gpio_names[] = {
[GPIO_STANDBY] = "standby",
[GPIO_RESET] = "rst",
[GPIO_TIM_861] = "tim_861",
[GPIO_IOPROC_EN] = "ioproc_en",
[GPIO_SW_EN] = "sw_en",
[GPIO_RC_BYPASS] = "rc_bypass",
[GPIO_AUDIO_EN] = "audio_en",
[GPIO_DVB_ASI] = "dvb_asi",
[GPIO_SMPTE_BYPASS] = "smpte_bypass",
[GPIO_DVI_LOCK] = "dvi_lock",
[GPIO_DATA_ERR] = "data_err",
[GPIO_LB_CONT] = "lb_cont",
[GPIO_Y_1ANC] = "y_1anc",
};

void gs2971_gpio_state(struct gs2971_priv *gs, int index, int active)
{
	if (gs->gpios[index]) {
		gpiod_set_value(gs->gpios[index], active);
		pr_debug("%s: active=%d, %s\n", __func__, active, gpio_names[index]);
	}
}

static void gs2971_change_gpio_state(struct gs2971_priv *gs, int on)
{
	gs2971_gpio_state(gs, GPIO_STANDBY, on ? 0 : 1);
	if (on) {
		pr_debug("%s: power up delay\n", __func__);
		msleep(1000);
	}
}

static int gs2971_get_gpios(struct gs2971_priv *gs, struct device *dev)
{
	int i;

	for (i = 0; i < GPIO_CNT; i++) {
		struct gpio_desc *gd = devm_gpiod_get(dev, gpio_names[i], GPIOD_IN);

		if (IS_ERR(gd)) {
			pr_info("%s: gpio %s not available\n", __func__, gpio_names[i]);
		} else {
			int active = ((i == GPIO_STANDBY) || (i == GPIO_RESET)) ? 1 : 0;

			gs->gpios[i] = gd;
			if (i >= GPIO_DVB_ASI) {
				pr_info("%s: gpio %s input\n", __func__, gpio_names[i]);
			} else {
				if (gpiod_is_active_low(gd))
					active ^= 1;
				gpiod_direction_output(gd, active);
				pr_info("%s: gpio %s output, state=%d, active_low=%d\n",
						__func__, gpio_names[i], active, gpiod_is_active_low(gd) ? 1 : 0);
			}
		}
	}
	gs2971_gpio_state(gs, GPIO_IOPROC_EN, 1);	//active ioproc_en
	msleep(2);
	gs2971_gpio_state(gs, GPIO_RESET, 0);	//remove reset
	msleep(400);
	return 0;
}

static s32 power_control(struct gs2971_priv *gs, int on)
{
	struct sensor_data *sensor = &gs->sensor;
	int i;
	int ret = 0;

	pr_debug("%s: %d\n", __func__, on);
	if (sensor->on != on) {
		if (on) {
			for (i = 0; i < REGULATOR_CNT; i++) {
				if (gs->regulator[i]) {
					ret = regulator_enable(gs->regulator[i]);
					if (ret) {
						pr_err("%s:regulator_enable failed(%d)\n",
							__func__, ret);
						on = 0;	/* power all off */
						break;
					}
				}
			}
		}
		gs2971_change_gpio_state(gs, on);
		sensor->on = on;
		if (!on) {
			for (i = REGULATOR_CNT - 1; i >= 0; i--) {
				if (gs->regulator[i])
					regulator_disable(gs->regulator[i]);
			}
		}
	}
	return ret;
}

static void temp_power_on(struct gs2971_priv *gs)
{
	/* Make sure power is on */
	cancel_delayed_work_sync(&gs->power_work);
	power_control(gs, 1);
	schedule_delayed_work(&gs->power_work, msecs_to_jiffies(1000));
}

static struct spi_device *gs2971_get_spi(struct gs2971_spidata *spidata)
{
	if (!spidata)
		return NULL;
	return spidata->spi;
}

static int get_std(struct v4l2_int_device *s, v4l2_std_id * id)
{
	struct gs2971_priv *gs = s->priv;
	struct sensor_data *sensor = &gs->sensor;
	struct spi_device *spi = NULL;
	u16 std_lock_value;
	u16 sync_lock_value;
	u16 words_per_actline_value;
	u16 words_per_line_value;
	u16 lines_per_frame_value;
	u16 actlines_per_frame_value;
	u16 interlaced_flag;
	int status;
	u16 sd_audio_status_value;
	u16 hd_audio_status_value;
	u16 sd_audio_config_value;
	u16 hd_audio_config_value;
	u16 readback_value;
	u16 ds1, ds2;

	pr_debug("-> In function %s\n", __func__);

	gs->mode = gs2971_mode_not_supported;
	gs->framerate = gs2971_default_fps;

	spi = gs2971_get_spi(spidata);
	if (!spi)
		return -ENODEV;

	*id = V4L2_STD_UNKNOWN;

	status =
	    gs2971_read_register(spi, GS2971_RASTER_STRUCT4, &std_lock_value);
	actlines_per_frame_value = std_lock_value & GS_RS4_ACTLINES_PER_FIELD;
	interlaced_flag = std_lock_value & GS_RS4_INT_nPROG;

	status =
	    gs2971_read_register(spi, GS2971_FLYWHEEL_STATUS, &sync_lock_value);
	if (status)
		return status;
	pr_debug("--> lock_value %x\n", sync_lock_value);
	if (!sync_lock_value) {
		pr_err("%s: no lock, gs2971\n", __func__);
		return -EBUSY;
	}

	status = gs2971_read_register(spi, GS2971_DATA_FORMAT_DS1, &ds1);
	if (status)
		return status;
	status = gs2971_read_register(spi, GS2971_DATA_FORMAT_DS2, &ds2);
	if (status)
		return status;
	pr_debug("--> ds1=%x\n--> ds2=%x\n", ds1, ds2);

	status =
	    gs2971_read_register(spi, GS2971_RASTER_STRUCT1,
				 &words_per_actline_value);
	if (status)
		return status;
	words_per_actline_value &= GS_RS1_WORDS_PER_ACTLINE;

	status =
	    gs2971_read_register(spi, GS2971_RASTER_STRUCT2,
				 &words_per_line_value);
	if (status)
		return status;
	words_per_line_value &= GS_RS2_WORDS_PER_LINE;

	status =
	    gs2971_read_register(spi, GS2971_RASTER_STRUCT3,
				 &lines_per_frame_value);
	if (status)
		return status;
	lines_per_frame_value &= GS_RS3_LINES_PER_FRAME;

	pr_debug("--> Words per line %u/%u Lines per frame %u/%u\n",
		 (unsigned int)words_per_actline_value,
		 (unsigned int)words_per_line_value,
		 (unsigned int)actlines_per_frame_value,
		 (unsigned int)lines_per_frame_value);
	pr_debug("--> SyncLock: %s %s StdLock: 0x%04x\n",
		 (sync_lock_value & GS_FLY_V_LOCK_DS1) ? "Vsync" : "NoVsync",
		 (sync_lock_value & GS_FLY_H_LOCK_DS1) ? "Hsync" : "NoHsync",
		 (unsigned int)(std_lock_value));

	status =
	    gs2971_read_register(spi, GS2971_CH_VALID, &sd_audio_status_value);
	if (status)
		return status;

	status =
	    gs2971_read_register(spi, GS2971_HD_CH_VALID,
				 &hd_audio_status_value);
	if (status)
		return status;

	if (!lines_per_frame_value) {
		pr_err("%s: 0 frame size\n", __func__);
		return -EBUSY;
	}

	if (gs->cea861) {
		sensor->spix.swidth = words_per_line_value - 1;
		sensor->spix.sheight = lines_per_frame_value;
	}
	if (interlaced_flag && lines_per_frame_value == 525) {
		pr_debug("--> V4L2_STD_525_60\n");
		*id = V4L2_STD_525_60;
	} else if (interlaced_flag && lines_per_frame_value == 625) {
		pr_debug("--> V4L2_STD_625_50\n");
		*id = V4L2_STD_625_50;
	} else if (interlaced_flag && lines_per_frame_value == 525) {
		pr_debug("--> V4L2_STD_525P_60\n");
		*id = YUV_FORMAT(gs);
	} else if (interlaced_flag && lines_per_frame_value == 625) {
		pr_debug("--> V4L2_STD_625P_50\n");
		*id = YUV_FORMAT(gs);
	} else if (!interlaced_flag && 749 <= lines_per_frame_value
		   && lines_per_frame_value <= 750) {
		if (gs->cea861) {
			sensor->spix.left = 220;
			sensor->spix.top = 25;
		}
		sensor->pix.width = 1280;
		sensor->pix.height = 720;
		if (words_per_line_value > 1650) {
			pr_debug("--> V4L2_STD_720P_50\n");
			*id = YUV_FORMAT(gs);
		} else {
			pr_debug("--> V4L2_STD_720P_60\n");
			gs->mode = gs2971_mode_720p;
			gs->framerate = gs2971_60_fps;
			*id = YUV_FORMAT(gs);
		}
	} else if (!interlaced_flag && 1124 <= lines_per_frame_value
		   && lines_per_frame_value <= 1125) {
		sensor->pix.width = 1920;
		sensor->pix.height = 1080;
		gs->mode = gs2971_mode_1080p;
		gs->framerate = gs2971_30_fps;	// Currently only 1080p30 is supported

		if (words_per_line_value >= 2200 + 550) {
			pr_debug("--> V4L2_STD_1080P_24\n");
			*id = YUV_FORMAT(gs);
		} else if (words_per_line_value >= 2200 + 440) {
			pr_debug("--> V4L2_STD_1080P_25\n");
			*id = YUV_FORMAT(gs);
		} else {
			pr_debug("--> V4L2_STD_1080P_60\n");
			*id = YUV_FORMAT(gs);
		}
	} else if (interlaced_flag && 1124 <= lines_per_frame_value
		   && lines_per_frame_value <= 1125) {

		sensor->pix.width = 1920;
		sensor->pix.height = 1080;
		if (words_per_line_value >= 2200 + 440) {
			pr_debug("--> V4L2_STD_1080I_50\n");
			*id = YUV_FORMAT(gs);
		} else {
			pr_debug("--> V4L2_STD_1080I_60\n");
			*id = YUV_FORMAT(gs);
		}
	} else {
		dev_err(&spi->dev,
			"Std detection failed: interlaced_flag: %u words per line %u/%u Lines per frame %u/%u SyncLock: %s %s StdLock: 0x%04x\n",
			(unsigned int)interlaced_flag,
			(unsigned int)words_per_actline_value,
			(unsigned int)words_per_line_value,
			(unsigned int)actlines_per_frame_value,
			(unsigned int)lines_per_frame_value,
			(sync_lock_value & GS_FLY_V_LOCK_DS1) ? "Vsync" :
			"NoVsync",
			(sync_lock_value & GS_FLY_H_LOCK_DS1) ? "Hsync" :
			"NoHsync", (unsigned int)(std_lock_value));
		return -1;
	}

	sd_audio_config_value = 0xaaaa;	// 16-bit, right-justified

	status =
	    gs2971_write_register(spi, GS2971_CFG_AUD, sd_audio_config_value);
	if (!status) {
		status =
		    gs2971_read_register(spi, GS2971_CFG_AUD, &readback_value);
		if (!status) {
			if (sd_audio_config_value != readback_value) {
				dev_dbg(&spi->dev,
					"SD audio readback failed, wanted x%04x, got x%04x\n",
					(unsigned int)sd_audio_config_value,
					(unsigned int)readback_value);
			}
		}
	}

	hd_audio_config_value = 0x0aa4;	// 16-bit, right-justified
	status =
	    gs2971_write_register(spi, GS2971_HD_CFG_AUD,
				  hd_audio_config_value);
	if (!status) {
		status =
		    gs2971_read_register(spi, GS2971_HD_CFG_AUD,
					 &readback_value);
		if (!status) {
			if (hd_audio_config_value != readback_value) {
				dev_dbg(&spi->dev,
					"HD audio readback failed, wanted x%04x, got x%04x\n",
					(unsigned int)hd_audio_config_value,
					(unsigned int)readback_value);
			}
		}
	}

	status =
	    gs2971_write_register(spi, GS2971_ANC_CONTROL, ANCCTL_ANC_DATA_DEL);
	if (status)
		return status;
	pr_debug("--> remove anc data\n");

#if 0
	status = gs2971_write_register(spi, GS2971_ACGEN_CTRL,
				       (GS2971_ACGEN_CTRL_SCLK_INV_MASK |
					GS2971_ACGEN_CTRL_MCLK_SEL_128FS));
#endif
	return 0;
}

static void gs2971_workqueue_handler(struct work_struct *data);

DECLARE_WORK(gs2971_worker, gs2971_workqueue_handler);

static void gs2971_workqueue_handler(struct work_struct *ignored)
{
	struct spi_device *spi;
	u16 bank_reg_base, anc_reg;
	static u16 anc_buf[257];
	int anc_length;
	int adf_found = 1;
	int ch_id;
	int status;
	u16 did, sdid;

	pr_debug("-> In function %s\n", __func__);

	for (ch_id = 0; ch_id < GS2971_NUM_CHANNELS; ch_id++) {

		spi = gs2971_get_spi(spidata);
		if (!spi)
			continue;

		/* Step 2: start writing to other bank */
		gs2971_write_register(spi, GS2971_ANC_CONTROL,
				      ANCCTL_ANC_DATA_DEL |
				      ANCCTL_ANC_DATA_SWITCH);

#if 1
		/* Step 1: read ancillary data */
		bank_reg_base = GS2971_ANC_BANK_REG;
		status = 0;
		for (anc_reg = bank_reg_base, adf_found = 1; (0 == status) &&
		     ((anc_reg + 6) < bank_reg_base + GS2971_ANC_BANK_SIZE);) {
			status = gs2971_read_buffer(spi, anc_reg, anc_buf, 6);
			anc_reg += 6;

			if (anc_reg >= bank_reg_base + GS2971_ANC_BANK_SIZE)
				break;

			if (!status) {
				if (anc_buf[0] == 0 ||
				    anc_buf[1] == 0xff || anc_buf[2] == 0xff) {
					did = anc_buf[3];
					sdid = anc_buf[4];
					anc_length = (anc_buf[5] & 0xff) + 1;
					anc_reg += anc_length;

					if (!(did == 0 && sdid == 0)
					    && (did < 0xf8)) {
						dev_dbg(&spi->dev,
							"anc[%x] %02x %02x %02x\n",
							anc_reg, did, sdid,
							anc_length);
					}
				} else {
					break;
				}
			}
		}
		dev_dbg(&spi->dev, "anc_end[%x] %02x %02x %02x\n",
			anc_reg, anc_buf[0], anc_buf[1], anc_buf[2]);
#endif

		/* Step 3: switch reads to other bank */
		gs2971_write_register(spi, GS2971_ANC_CONTROL,
				      ANCCTL_ANC_DATA_DEL);
	}
}

#if defined(GS2971_ENABLE_ANCILLARY_DATA)
static int gs2971_init_ancillary(struct spi_device *spi)
{
	pr_debug("-> In function %s\n", __func__);

	u16 value;
	int offset;

	int status = 0;

	/* Set up ancillary data filtering */
	if (!status)
		status =
		    gs2971_write_register(spi, GS2971_REG_ANC_TYPE1, 0x4100);
	/*  SMPTE-352M Payload ID (0x4101) */
	if (!status)
		status = gs2971_write_register(spi, GS2971_REG_ANC_TYPE2, 0x5f00);	/* Placeholder */

	if (!status)
		status = gs2971_write_register(spi, GS2971_REG_ANC_TYPE3, 0x6000);	/* Placeholder */

	if (!status)
		status =
		    gs2971_write_register(spi, GS2971_REG_ANC_TYPE4, 0x6100);
	/* SMPTE 334 - SDID 01=EIA-708, 02=EIA-608 */

	if (!status)
		status =
		    gs2971_write_register(spi, GS2971_REG_ANC_TYPE5, 0x6200);
	/* SMPTE 334 - SDID 01=Program description, 02=Data broadcast, 03=VBI data */

	if (0 == gs2971_read_register(spi, GS2971_REG_ANC_TYPE1, &value)) {
		dev_dbg(&spi->dev, "REG_ANC_TYPE1 value x%04x\n",
			(unsigned int)value);
	}
	if (0 == gs2971_read_register(spi, GS2971_REG_ANC_TYPE2, &value)) {
		dev_dbg(&spi->dev, "REG_ANC_TYPE2 value x%04x\n",
			(unsigned int)value);
	}

	/* Clear old ancillary data */
	if (!status)
		status = gs2971_write_register(spi, GS2971_ANC_CONTROL,
					       ANCCTL_ANC_DATA_DEL |
					       ANCCTL_ANC_DATA_SWITCH);

	/* Step 2: start writing to other bank */
	if (!status)
		status =
		    gs2971_write_register(spi, GS2971_ANC_CONTROL,
					  ANCCTL_ANC_DATA_DEL);
	return status;
}
#endif

/* gs2971_initialize :
 * This function will set the video format standard
 */
static int gs2971_initialize(struct gs2971_priv *gs, struct spi_device *spi)
{
	int status = 0;
	int retry = 0;
	u16 value;

	u16 cfg = GS_VCFG1_861_PIN_DISABLE_MASK;

	if (gs->cea861)
		cfg |= GS_VCFG1_TIMING_861_MASK;

	pr_debug("-> In function %s\n", __func__);

	for (;;) {
		status = gs2971_write_register(spi, GS2971_VCFG1, cfg);
		if (status)
			return status;

		status = gs2971_read_register(spi, GS2971_VCFG1, &value);
		if (status)
			return status;
		if (value == cfg)
			break;
		dev_err(&spi->dev, "status=%x, read value of 0x%04x, expected 0x%04x\n", status,
			(unsigned int)value, cfg);
		if (retry++ >= 20)
			return -ENODEV;
		msleep(50);
	}
//      status = gs2971_write_register(spi, GS2971_VCFG2, GS_VCFG2_DS_SWAP_3G);
	status = gs2971_write_register(spi, GS2971_VCFG2, 0);
	if (status)
		return status;

	status = gs2971_write_register(spi, GS2971_IO_CONFIG,
				       (GS_IOCFG_HSYNC << 0) | (GS_IOCFG_VSYNC
								<< 5) |
				       (GS_IOCFG_DE << 10));
	if (status)
		return status;

	status = gs2971_write_register(spi, GS2971_IO_CONFIG2,
				       (GS_IOCFG_LOCKED << 0) | (GS_IOCFG_Y_ANC
								 << 5) |
				       (GS_IOCFG_DATA_ERROR << 10));
	if (status)
		return status;

	status =
	    gs2971_write_register(spi, GS2971_TIM_861_CFG, GS_TIMCFG_TRS_861);
	if (status)
		return status;

#if defined(GS2971_ENABLE_ANCILLARY_DATA)
	gs2971_init_ancillary(spi);
#endif

#if 0
	if (gs2971_timer.function == NULL) {
		init_timer(&gs2971_timer);
		gs2971_timer.function = gs2971_timer_function;
	}
	mod_timer(&gs2971_timer, jiffies + msecs_to_jiffies(gs2971_timeout_ms));
#endif
	return status;
}

#if 0
/*!
 * Return attributes of current video standard.
 * Since this device autodetects the current standard, this function also
 * sets the values that need to be changed if the standard changes.
 * There is no set std equivalent function.
 *
 *  @return		None.
 */
static void gs2971_get_std(struct v4l2_int_device *s, v4l2_std_id * std)
{
	pr_debug("-> In function %s\n", __func__);
}
#endif

/***********************************************************************
 * IOCTL Functions from v4l2_int_ioctl_desc.
 ***********************************************************************/

/*!
 * ioctl_g_ifparm - V4L2 sensor interface handler for vidioc_int_g_ifparm_num
 * s: pointer to standard V4L2 device structure
 * p: pointer to standard V4L2 vidioc_int_g_ifparm_num ioctl structure
 *
 * Gets slave interface parameters.
 * Calculates the required xclk value to support the requested
 * clock parameters in p.  This value is returned in the p
 * parameter.
 *
 * vidioc_int_g_ifparm returns platform-specific information about the
 * interface settings used by the sensor.
 *
 * Called on open.
 */
static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	struct gs2971_priv *gs = s->priv;
	struct sensor_data *sensor = &gs->sensor;

	pr_debug("-> In function %s\n", __func__);

	/* Initialize structure to 0s then set any non-0 values. */
	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = sensor->mclk;
	if (gs->cea861) {
		p->if_type = V4L2_IF_TYPE_BT656;	/* This is the only possibility. */
		p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_10BIT;
		p->u.bt656.bt_sync_correct = 1;
//      	p->u.bt656.nobt_vs_inv = 1;
		p->u.bt656.nobt_hs_inv = 1;
	} else {
		/*
		 * p->if_type = V4L2_IF_TYPE_BT656_PROGRESSIVE;
		 * BT.656 - 20/10bit pin low doesn't work because then EAV of
		 * DS1/2 are also interleaved and imx only recognizes 4 word
		 * EAV/SAV codes, not 8
		 */
		p->if_type = V4L2_IF_TYPE_BT1120_PROGRESSIVE_SDR;
		p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_BT_10BIT;
		p->u.bt656.bt_sync_correct = 0;	// Use embedded sync
		p->u.bt656.nobt_vs_inv = 1;
//		p->u.bt656.nobt_hs_inv = 1;
	}
	p->u.bt656.latch_clk_inv = 0;	/* pixel clk polarity */
	p->u.bt656.clock_min = 6000000;
	p->u.bt656.clock_max = 180000000;
	return 0;
}

static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	struct gs2971_priv *gs = s->priv;
	struct sensor_data *sensor = &gs->sensor;

	pr_debug("-> In function %s\n", __func__);

	gs->ioctl_power_on = on;
	if (on && !sensor->on) {
		power_control(gs, 1);
	} else if (!on && sensor->on) {
		power_control(gs, 0);
	}
	return 0;
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct gs2971_priv *gs = s->priv;
	struct sensor_data *sensor = &gs->sensor;
	struct v4l2_captureparm *cparm = &a->parm.capture;

	pr_debug("-> In function %s\n", __func__);

	switch (a->type) {
		/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		pr_debug("   type is V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		break;

	default:
		pr_debug("ioctl_g_parm:type is unknown %d\n", a->type);
		break;
	}
	return 0;
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 *
 * This driver cannot change these settings.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct gs2971_priv *gs = s->priv;
	struct sensor_data *sensor = &gs->sensor;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	int ret = 0;

	pr_debug("-> In function %s\n", __func__);

	/* Make sure power on */
	temp_power_on(gs);

	switch (a->type) {
		/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator =
			    gs2971_framerates[gs2971_default_fps];
			timeperframe->numerator = 1;
			pr_warning("   Setting framerate to default (%dfps)!\n",
				   gs2971_framerates[gs2971_default_fps]);
		} else if (timeperframe->denominator !=
			   gs2971_framerates[gs->framerate]) {
			pr_warning
			    ("   Input framerate is %dfps and you are trying to set %dfps!\n",
			     gs2971_framerates[gs->framerate],
			     timeperframe->denominator);
		}
		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode =
		    (u32) a->parm.capture.capturemode;
		break;

		/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			 a->type);
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}
	return ret;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct gs2971_priv *gs = s->priv;
	struct sensor_data *sensor = &gs->sensor;
	v4l2_std_id std = V4L2_STD_UNKNOWN;

	pr_debug("-> In function %s\n", __func__);

	temp_power_on(gs);
	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		get_std(s, &std);
		f->fmt.pix = sensor->pix;
		break;

	case V4L2_BUF_TYPE_SENSOR:
		pr_debug("%s: left=%d, top=%d, %dx%d\n", __func__,
			sensor->spix.left, sensor->spix.top,
			sensor->spix.swidth, sensor->spix.sheight);
		f->fmt.spix = sensor->spix;
		break;

	case V4L2_BUF_TYPE_PRIVATE:
		get_std(s, &std);
//              f->fmt.pix.pixelformat = (u32)std;
		break;

	default:
		f->fmt.pix = sensor->pix;
		break;
	}
	return 0;
}

/*!
 * ioctl_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the video_control[] array.  Otherwise, returns -EINVAL if the
 * control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s, struct v4l2_queryctrl *qc)
{
	pr_debug("-> In function %s\n", __func__);
	return -EINVAL;
}

/*!
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	struct gs2971_priv *gs = s->priv;
	struct sensor_data *sensor = &gs->sensor;
	int ret = 0;

	pr_debug("-> In function %s\n", __func__);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = sensor->brightness;
		break;
	case V4L2_CID_HUE:
		vc->value = sensor->hue;
		break;
	case V4L2_CID_CONTRAST:
		vc->value = sensor->contrast;
		break;
	case V4L2_CID_SATURATION:
		vc->value = sensor->saturation;
		break;
	case V4L2_CID_RED_BALANCE:
		vc->value = sensor->red;
		break;
	case V4L2_CID_BLUE_BALANCE:
		vc->value = sensor->blue;
		break;
	case V4L2_CID_EXPOSURE:
		vc->value = sensor->ae_mode;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	struct gs2971_priv *gs = s->priv;
//	struct sensor_data *sensor = &gs->sensor;
	int retval = 0;

	pr_debug("-> In function %s\n", __func__);

	///* Make sure power on */
	temp_power_on(gs);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_BRIGHTNESS\n");
		break;
	case V4L2_CID_CONTRAST:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_CONTRAST\n");
		break;
	case V4L2_CID_SATURATION:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_SATURATION\n");
		break;
	case V4L2_CID_HUE:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_HUE\n");
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_AUTO_WHITE_BALANCE\n");
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_DO_WHITE_BALANCE\n");
		break;
	case V4L2_CID_RED_BALANCE:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_RED_BALANCE\n");
		break;
	case V4L2_CID_BLUE_BALANCE:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_BLUE_BALANCE\n");
		break;
	case V4L2_CID_GAMMA:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_GAMMA\n");
		break;
	case V4L2_CID_EXPOSURE:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_EXPOSURE\n");
		break;
	case V4L2_CID_AUTOGAIN:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_AUTOGAIN\n");
		break;
	case V4L2_CID_GAIN:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_GAIN\n");
		break;
	case V4L2_CID_HFLIP:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_HFLIP\n");
		break;
	case V4L2_CID_VFLIP:
		dev_dbg(&spidata->spi->dev, "   V4L2_CID_VFLIP\n");
		break;
	default:
		dev_dbg(&spidata->spi->dev, "   Default case\n");
		retval = -EPERM;
		break;
	}
	return retval;
}

/*!
 * ioctl_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fsize)
{
	struct gs2971_priv *gs = s->priv;
	struct sensor_data *sensor = &gs->sensor;

	pr_debug("-> In function %s\n", __func__);

	if (fsize->index > gs2971_mode_MAX || gs->mode > gs2971_mode_MAX)
		return -EINVAL;

	fsize->pixel_format = sensor->pix.pixelformat;
	fsize->discrete.width = gs2971_res[gs->mode].width;
	fsize->discrete.height = gs2971_res[gs->mode].height;
	return 0;
}

/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	pr_debug("-> In function %s\n", __func__);

	((struct v4l2_dbg_chip_ident *)id)->match.type =
	    V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name, "gs2971_video");

	return 0;
}

/*!
 * ioctl_enum_fmt_cap - V4L2 sensor interface handler for VIDIOC_ENUM_FMT
 * @s: pointer to standard V4L2 device structure
 * @fmt: pointer to standard V4L2 fmt description structure
 *
 * Return 0.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	struct gs2971_priv *gs = s->priv;
	struct sensor_data *sensor = &gs->sensor;

	pr_debug("-> In function %s\n", __func__);

	if (fmt->index > 0)
		return -EINVAL;
	fmt->pixelformat = sensor->pix.pixelformat;	//gs->pix.pixelformat;
	return 0;
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	pr_debug("-> In function %s\n", __func__);
	return 0;
}

/*!
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	pr_debug("-> In function %s\n", __func__);
	return 0;
}

/*!
 * This structure defines all the ioctls for this module.
 */
static struct v4l2_int_ioctl_desc gs2971_ioctl_desc[] = {

	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func *) ioctl_dev_init},
	{vidioc_int_s_power_num, (v4l2_int_ioctl_func *) ioctl_s_power},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func *) ioctl_g_ifparm},
	{vidioc_int_init_num, (v4l2_int_ioctl_func *) ioctl_init},

	/*!
	 * VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
	 */
	{vidioc_int_enum_fmt_cap_num,
	 (v4l2_int_ioctl_func *) ioctl_enum_fmt_cap},
	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *) ioctl_g_fmt_cap},
	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func *) ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func *) ioctl_s_parm},
	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func *) ioctl_queryctrl},
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *) ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *) ioctl_s_ctrl},
	{vidioc_int_enum_framesizes_num,
	 (v4l2_int_ioctl_func *) ioctl_enum_framesizes},
	{vidioc_int_g_chip_ident_num,
	 (v4l2_int_ioctl_func *) ioctl_g_chip_ident},
};

static struct v4l2_int_slave gs2971_slave = {
	.ioctls = gs2971_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(gs2971_ioctl_desc),
};

static struct v4l2_int_device gs2971_int_device = {
	.module = THIS_MODULE,
	.name = "gs2971",
	.type = v4l2_int_type_slave,
	.u = {
	      .slave = &gs2971_slave,
	      },
};

static void gs2971_power_worker(struct work_struct *work)
{
	struct gs2971_priv *gs = container_of(work, struct gs2971_priv, power_work.work);

	if (!gs->ioctl_power_on)
		power_control(gs, 0);
}

static ssize_t gs2971_show_video_lock(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gs2971_priv *gs = gs2971_int_device.priv;
	int len = 0;

	if (!gs)
		return -ENODEV;
	temp_power_on(gs);
	len += sprintf(buf+len, "%d\n", gs->video_lock);
	return len;
}

static DEVICE_ATTR(video_lock, S_IRUGO, gs2971_show_video_lock, NULL);

static irqreturn_t lock_detect_handler(int irq, void *data)
{
	struct gs2971_priv *gs = data;
	int level = gpiod_get_value(gs->gpios[GPIO_DVI_LOCK]);

	gs->video_lock = level;
	return IRQ_HANDLED;
}

/*!
 * GS2971 SPI probe function.
 * Function set in spi_driver struct.
 * Called by insmod.
 *
 *  @param *spi	SPI device descriptor.
 *
 *  @return		Error code indicating success or failure.
 */
static int gs2971_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct gs2971_priv *gs;
	struct sensor_data *sensor;
	struct pinctrl_state *pins;
	int irq = 0;

	int ret = 0;

	pr_debug("-> In function %s\n", __func__);

	gs = kzalloc(sizeof(*gs), GFP_KERNEL);
	if (!gs)
		return -ENOMEM;

	INIT_DELAYED_WORK(&gs->power_work, gs2971_power_worker);

	/* Allocate driver data */
	spidata = kzalloc(sizeof(*spidata), GFP_KERNEL);
	if (!spidata) {
		ret = -ENOMEM;
		goto exit;
	}
	/* Initialize the driver data */
	spidata->spi = spi;
	mutex_init(&spidata->buf_lock);
	sensor = &gs->sensor;

	ret = of_property_read_u32(dev->of_node, "mclk", &sensor->mclk);
	if (ret) {
		dev_err(dev, "mclk missing or invalid\n");
		goto exit;
	}

	ret = of_property_read_u32(dev->of_node, "ipu", &sensor->ipu_id);
	if (ret) {
		dev_err(dev, "ipu missing or invalid\n");
		goto exit;
	}

	ret = of_property_read_u32(dev->of_node, "csi", &sensor->csi);
	if (ret) {
		dev_err(dev, "csi missing or invalid\n");
		goto exit;
	}

	ret = of_property_read_u32(dev->of_node, "cea861", &gs->cea861);
	if (ret) {
		dev_err(dev, "csi missing or invalid\n");
		goto exit;
	}
	pr_info("%s:cea861=%d\n", __func__, gs->cea861);

	gs->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(gs->pinctrl)) {
		ret = PTR_ERR(gs->pinctrl);
		goto exit;
	}


	pins = pinctrl_lookup_state(gs->pinctrl, gs->cea861 ? "cea861" : "no_cea861");
	if (IS_ERR(pins)) {
		ret = PTR_ERR(pins);
		goto exit;
	}

	ret = pinctrl_select_state(gs->pinctrl, pins);
	if (ret)
		goto exit;

	sensor->pix.pixelformat = YUV_FORMAT(gs);
	sensor->pix.width = gs2971_res[gs2971_mode_default].width;
	sensor->pix.height = gs2971_res[gs2971_mode_default].height;
	sensor->streamcap.capability = V4L2_MODE_HIGHQUALITY |
	    V4L2_CAP_TIMEPERFRAME;
	sensor->streamcap.capturemode = 0;
	sensor->streamcap.timeperframe.denominator = DEFAULT_FPS;
	sensor->streamcap.timeperframe.numerator = 1;

	//sensor->pix.priv = 1;  /* 1 is used to indicate TV in */
	ret = gs2971_get_gpios(gs, dev);
	if (ret)
		goto exit;

	temp_power_on(gs);
	gs2971_int_device.priv = gs;

	ret = gs2971_initialize(gs, spi);
	if (ret)
		goto exit;

	irq = gpiod_to_irq(gs->gpios[GPIO_DVI_LOCK]);
	ret = request_irq(irq,
			lock_detect_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"lock_det", gs);
	if (ret < 0)
		dev_warn(dev, "could not request irq %d ret=%d\n", irq, ret);

	ret = device_create_file(dev, &dev_attr_video_lock);
	if (ret < 0)
		dev_warn(dev, "could not create lock file ret=%d\n", ret);

	ret = v4l2_int_device_register(&gs2971_int_device);
 exit:
	if (ret) {
		if (irq)
			free_irq(irq,  gs);
		kfree(spidata);
		kfree(gs);
		pr_err("gs2971_probe returns %d\n", ret);
	}
	return ret;

}

static int gs2971_remove(struct spi_device *spi)
{
	struct gs2971_priv *gs = dev_get_drvdata(&spi->dev);
	int irq = gpiod_to_irq(gs->gpios[GPIO_DVI_LOCK]);

	pr_debug("-> In function %s\n", __func__);

	v4l2_int_device_unregister(&gs2971_int_device);
	if (irq)
		free_irq(irq,  gs);
	kfree(gs2971_int_device.priv);
	kfree(spidata);
	return 0;
}

static const struct spi_device_id gs2971_spi_ids[] = {
	{ "gs2971", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, gs2971_spi_ids);

static struct spi_driver gs2971_spi_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .bus = &spi_bus_type,
		   .owner = THIS_MODULE,
		   },
	.id_table = gs2971_spi_ids,
	.probe = gs2971_probe,
	.remove = gs2971_remove,
};

module_spi_driver(gs2971_spi_driver);

MODULE_DESCRIPTION("gs2971 video input Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
