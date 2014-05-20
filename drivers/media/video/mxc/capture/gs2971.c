/*
 * Copyright 2005-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file gs2971.c
 *
 * @brief SEMTECH SD SDI reciever
 *
 * @ingroup Camera
 */
#define DEBUG 1
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-int-device.h>
#include "mxc_v4l2_capture.h"
#include <linux/proc_fs.h>

#include "gs2971.h"

#define GS2971_SPI_TRANSFER_MAX 1024

#define DRIVER_NAME     "gs2971"

/* Debug functions */
static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-2)");

static struct sensor_data gs2971_data;
static struct gs2971_spidata *spidata;

static int gs2971_probe(struct spi_device *spi);
static int gs2971_remove(struct spi_device *spi);
static int gs2971_initialize(void);
static int set_power(int on);

static struct spi_driver gs2971_spi = {
	.driver = {
		.name =		"gs2971",
		.bus	= &spi_bus_type,
		.owner =	THIS_MODULE,
	},
	.probe =	gs2971_probe,
	.remove =	__devexit_p(gs2971_remove),
};


static inline struct gs2971_channel *to_gs2971(struct v4l2_subdev *sd)
{
	//return container_of(sd, struct gs2971_channel, sd);
	return 0;
}

/*! @brief This mutex is used to provide mutual exclusion.
 *
 *  Create a mutex that can be used to provide mutually exclusive
 *  read/write access to the globally accessible data structures
 *  and variables that were defined above.
 */
static DEFINE_MUTEX(mutex);

int gs2971_read_buffer(struct spi_device *spi, u16 offset, u16 *values, int length )
{
	struct spi_message msg;
	struct spi_transfer spi_xfer;
	int status;
	u16 txbuf[GS2971_SPI_TRANSFER_MAX+1] = {
			0x9000,  // read, auto-increment
	};
	u16 rxbuf[GS2971_SPI_TRANSFER_MAX+1];

	if (!spi)
		return -ENODEV;

	if (length > GS2971_SPI_TRANSFER_MAX)
		return -EINVAL;

	txbuf[0] = txbuf[0] | (offset & 0xfff);

	memset( &spi_xfer, '\0', sizeof(spi_xfer) );
	spi_xfer.tx_buf = txbuf;
	spi_xfer.rx_buf = rxbuf;
	spi_xfer.cs_change = 1;
	spi_xfer.bits_per_word = 16;
	spi_xfer.delay_usecs = 0;
	spi_xfer.speed_hz = 1000000;
	spi_xfer.len = sizeof(*values)*(length+1); // length in bytes

	spi_message_init( &msg );
	spi_message_add_tail( &spi_xfer, &msg );

	status = spi_sync(spi, &msg);

	memcpy( values, &rxbuf[1], sizeof(*values)*length );

	return status;
}

int gs2971_read_register(struct spi_device *spi, u16 offset, u16 *value)
{
	struct spi_message msg;
	struct spi_transfer spi_xfer;
	int status;
	u32 txbuf[1] = {
			0x90000000,  // read, auto-increment
	};
	u32 rxbuf[1];

	if (!spi)
		return -ENODEV;

	txbuf[0] = txbuf[0] | ((offset & 0xfff)<<16);

	memset( &spi_xfer, '\0', sizeof(spi_xfer) );
	spi_xfer.tx_buf = txbuf;
	spi_xfer.rx_buf = rxbuf;
	spi_xfer.cs_change = 0; // ??
	spi_xfer.bits_per_word = 32;
	spi_xfer.delay_usecs = 0;
	spi_xfer.speed_hz = 1000000;
	spi_xfer.len = 4; // length in bytes

	spi_message_init( &msg );
	spi_message_add_tail( &spi_xfer, &msg );

	status = spi_sync(spi, &msg);

	if (status) {
		dev_dbg( &spi->dev, "read_reg failed\n" );
		*value = 0xffff;
	} else {
		*value = (u16)(rxbuf[0] & 0xffff);
	}

	printk(KERN_ERR ">> Read register (%x) with value: %x",offset, *value);
	return status;
}


int gs2971_write_buffer(struct spi_device *spi, u16 offset, u16 *values, int length)
{
	struct spi_message msg;
	struct spi_transfer spi_xfer;
	int status;
	u16 txbuf[GS2971_SPI_TRANSFER_MAX] = {
			0x1000,  // write, auto-increment
	};
	u16 rxbuf[GS2971_SPI_TRANSFER_MAX] = {
	};

	if (!spi)
		return -ENODEV;

	if (length > GS2971_SPI_TRANSFER_MAX-1)
		return -EINVAL;

	txbuf[0] = txbuf[0] | (offset & 0xfff);
	memcpy( &txbuf[1], values, sizeof(*values)*length );

	memset( &spi_xfer, '\0', sizeof(spi_xfer) );
	spi_xfer.tx_buf = txbuf;
	spi_xfer.rx_buf = rxbuf;
	spi_xfer.cs_change = 0; // ??
	spi_xfer.bits_per_word = 16;
	spi_xfer.delay_usecs = 0;
	spi_xfer.speed_hz = 1000000;
	spi_xfer.len = sizeof(*values)*(length+1); // length in bytes

	spi_message_init( &msg );
	spi_message_add_tail( &spi_xfer, &msg );

	status = spi_sync(spi, &msg);
	return status;
}


int gs2971_write_register(struct spi_device *spi, u16 offset, u16 value)
{
	printk(KERN_ERR ">> Writing to address (%x) : %x\n",offset,value);

	return gs2971_write_buffer(spi, offset, &value, 1);
}

static struct spi_device *gs2971_get_spi( struct gs2971_spidata *spidata )
{
	if (!spidata)
		return NULL;
	return spidata->spi;
}

void get_mode()
{
	int S_B, D_A;

	gpio_request(SMPTE_BYPASS, "SMPTE_BYPASS");
	gpio_request(DVB_ASI, "DVB_ASI");

	gpio_direction_input(SMPTE_BYPASS);
	gpio_direction_input(DVB_ASI);

	S_B = gpio_get_value(SMPTE_BYPASS);
	D_A = gpio_get_value(DVB_ASI);

	printk(KERN_ERR "***SMPTE = %d \n ***DVB = %d \n",S_B, D_A);

	if (S_B == 0) {
		if (D_A == 0) printk(KERN_ERR "***Data-through mode\n");
		else printk(KERN_ERR "***DVB_ASI Mode\n");
	} else {
		if (D_A == 0) printk(KERN_ERR "***SMPTE Mode \n");
		else printk(KERN_ERR "***Default \n");
	}
}

void get_std(struct v4l2_int_device *s, v4l2_std_id *id)
{
	struct spi_device     *spi = NULL;
	//struct gs2971_channel *ch  = NULL;
	u16 test;

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

	//ch = to_gs2971( sd );
	spi = gs2971_get_spi( spidata );
	if (!spi )
		return -ENODEV;

	/*testing*/
	/*
	 * status = gs2971_write_register( spi, 0x24, 0 );
	 */
	status = gs2971_read_register(spi, 0x006, &test);
	status = gs2971_read_register(spi, 0x007, &test);
	status = gs2971_read_register(spi, 0x008, &test);
	status = gs2971_read_register( spi, 0x024, &test);
	status = gs2971_read_register( spi, 0x025, &test);

	get_mode();
	/*testing*/

//	*id = V4L2_STD_1080P_60;
//	return 0;

	status = gs2971_read_register(spi, GS2971_REG_STD_LOCK, &std_lock_value);
	actlines_per_frame_value = std_lock_value & GS2971_REG_STD_LOCK_ACT_LINES_MASK;
	interlaced_flag = std_lock_value & GS2971_REG_STD_LOCK_INT_PROG_MASK;
	interlaced_flag = !!interlaced_flag;

	printk(KERN_ERR "***** Status(%d) : %d\n",__LINE__,status);

	if (!status)
          status = gs2971_read_register(spi, GS2971_REG_HVLOCK, &sync_lock_value);

	printk(KERN_ERR "***** Status(%d) : %d, lock_value %x\n",__LINE__,status, sync_lock_value);
	if (!status) {
		status = gs2971_read_register(spi, GS2971_REG_WORDS_PER_ACTLINE, &words_per_actline_value);
		words_per_actline_value &= GS2971_REG_WORDS_PER_ACTLINE_MASK;
	}
	printk(KERN_ERR "***** Status(%d) : %d\n, words_per_actline_value = %d\n",__LINE__,status, words_per_actline_value);
	if (!status) {
		status = gs2971_read_register(spi, GS2971_REG_WORDS_PER_LINE, &words_per_line_value);
		words_per_line_value &= GS2971_REG_WORDS_PER_LINE_MASK;
	}
	printk(KERN_ERR "***** Status(%d) : %d\n words_per_line_value = %d\n",__LINE__,status,words_per_line_value);
	if (!status) {
		status = gs2971_read_register(spi, GS2971_REG_LINES_PER_FRAME, &lines_per_frame_value);
		lines_per_frame_value &= GS2971_REG_LINES_PER_FRAME_MASK;
	}
	printk(KERN_ERR "***** Status(%d) : %d\n lines_per_frame_value = %d\n",__LINE__,status, lines_per_frame_value);
	if (!status) {
		printk(KERN_ERR "Words per line %u/%u Lines per frame %u/%u\n",
				(unsigned int) words_per_actline_value,
				(unsigned int) words_per_line_value,
				(unsigned int) actlines_per_frame_value,
				(unsigned int) lines_per_frame_value );
		printk(KERN_ERR "SyncLock: %s %s StdLock: 0x%04x\n",
				(sync_lock_value & GS2971_REG_HVLOCK_VLOCK_MASK) ? "Vsync" : "NoVsync",
				(sync_lock_value & GS2971_REG_HVLOCK_HLOCK_MASK) ? "Hsync" : "NoHsync",
				(unsigned int)(std_lock_value) );
	}
	printk(KERN_ERR "***** Status(%d) : %d\n",__LINE__,status);
	if (!status) {
		status = gs2971_read_register( spi, GS2971_REG_SD_AUDIO_STATUS, &sd_audio_status_value );
		v4l2_dbg(1, debug, s, "SD audio status 0x%x\n", (unsigned int)sd_audio_status_value );
	}
	printk(KERN_ERR "***** Status(%d) : %d\n",__LINE__,status);
	if (!status) {
		status = gs2971_read_register( spi, GS2971_REG_HD_AUDIO_STATUS, &hd_audio_status_value );
		v4l2_dbg(1, debug, s, "HD audio status 0x%x\n", (unsigned int)hd_audio_status_value );
	}
	printk(KERN_ERR "***** Status(%d) : %d\n",__LINE__,status);
	if ( 0 == lines_per_frame_value ) {
		return -EINVAL;
	}
	printk(KERN_ERR "***** Status(%d) : %d\n",__LINE__,status);
	if ( interlaced_flag != 0
			&& lines_per_frame_value == 525 ) {
		printk(KERN_ERR "****STD (V4L2_STD_525_60)\n");
		// *id = V4L2_STD_525_60;
	}
	else if ( interlaced_flag != 0
			&& lines_per_frame_value == 625 ) {
		printk(KERN_ERR "****STD (V4L2_STD_625_50)\n");
		// *id = V4L2_STD_625_50;
	}
	else if ( interlaced_flag == 0
			&& lines_per_frame_value == 525 ) {
		printk(KERN_ERR "****STD (V4L2_STD_525P_60)\n");
		//*id = V4L2_STD_525P_60;
	}
	else if ( interlaced_flag == 0
			&& lines_per_frame_value == 625 ) {
		printk(KERN_ERR "****STD (V4L2_STD_625P_50)\n");
		//*id = V4L2_STD_625P_50;
	}
	else if ( interlaced_flag == 0
			&& 749 <= lines_per_frame_value
			&& lines_per_frame_value <= 750 ) {

		if ( words_per_line_value > 1650  ) {
			printk(KERN_ERR "****STD (V4L2_STD_720P_50)\n");
			//         *id = V4L2_STD_720P_50;
		} else {
			printk(KERN_ERR "****STD (V4L2_STD_720P_60)\n");
			//        *id = V4L2_STD_720P_60;
		}
	} else if ( interlaced_flag == 0
			&& 1124 <= lines_per_frame_value
			&& lines_per_frame_value <= 1125 ) {
		if ( words_per_line_value >= 2200+550 ) {
			printk(KERN_ERR "****STD (V4L2_STD_1080P_24)\n");
			//      *id = V4L2_STD_1080P_24;
		}
		else if ( words_per_line_value >= 2200+440 ) {
			printk(KERN_ERR "****STD (V4L2_STD_1080P_25)\n");
			//     *id = V4L2_STD_1080P_25;
		} else {
			//               *id = V4L2_STD_1080P_30;
			printk(KERN_ERR "****STD (V4L2_STD_1080P_60)\n");
			//*id = V4L2_STD_1080P_60;
		}
	} else if ( interlaced_flag == 1
			&& 1124 <= lines_per_frame_value
			&& lines_per_frame_value <= 1125 ) {

		if ( words_per_line_value >= 2200+440 ) {
			printk(KERN_ERR "****STD (V4L2_STD_1080I_50)\n");
			//*id = V4L2_STD_1080I_50;
		} else {
			printk(KERN_ERR "****STD (V4L2_STD_1080I_60)\n");
			//*id = V4L2_STD_1080I_60;
		}
	} else {
		dev_err( &spi->dev, "Std detection failed: interlaced_flag: %u words per line %u/%u Lines per frame %u/%u SyncLock: %s %s StdLock: 0x%04x\n",
				(unsigned int) interlaced_flag,
				(unsigned int) words_per_actline_value,
				(unsigned int) words_per_line_value,
				(unsigned int) actlines_per_frame_value,
				(unsigned int) lines_per_frame_value,
				(sync_lock_value & GS2971_REG_HVLOCK_VLOCK_MASK) ? "Vsync" : "NoVsync",
				(sync_lock_value & GS2971_REG_HVLOCK_HLOCK_MASK) ? "Hsync" : "NoHsync",
				(unsigned int)(std_lock_value) );
		return -EINVAL;
	}

	sd_audio_config_value = 0xaaaa; // 16-bit, right-justified

	if (!status)
		status = gs2971_write_register(spi, GS2971_REG_SD_AUDIO_CONFIG, sd_audio_config_value );

	printk(KERN_ERR "***** Status(%d) : %d\n",__LINE__,status);
	if (!status) {
		status = gs2971_read_register(spi, GS2971_REG_SD_AUDIO_CONFIG, &readback_value );
		if (!status) {
			if ( sd_audio_config_value != readback_value ) {
				dev_dbg( &spi->dev, "SD audio readback failed, wanted x%04x, got x%04x\n",
						(unsigned int) sd_audio_config_value,
						(unsigned int) readback_value);
			}
		}
	}
	printk(KERN_ERR "***** Status(%d) : %d\n",__LINE__,status);
	if (!status) {
		hd_audio_config_value = 0x0aa4; // 16-bit, right-justified
		status = gs2971_write_register(spi, GS2971_REG_HD_AUDIO_CONFIG, hd_audio_config_value );
	}
	printk(KERN_ERR "***** Status(%d) : %d\n",__LINE__,status);
	if (!status) {
		status = gs2971_read_register( spi,
				GS2971_REG_HD_AUDIO_CONFIG,
				&readback_value );
		if (!status) {
			if ( hd_audio_config_value != readback_value ) {
				dev_dbg( &spi->dev, "HD audio readback failed, wanted x%04x, got x%04x\n",
						(unsigned int) hd_audio_config_value,
						(unsigned int) readback_value
				);
			}
		}
	}

	printk(KERN_ERR "***** Status(%d) : %d\n",__LINE__,status);
	switch ( *id ) {
	case V4L2_STD_525_60:
	case V4L2_STD_625_50:
		dev_dbg( &spi->dev, "Set SD-SDI input, 8-bits width\n");
//		dec_device->if_type = INTERFACE_TYPE_BT656;
		gs2971_write_register(spi, GS2971_REG_ANC_CONFIG, 2);
		break;

	default:
		dev_dbg( &spi->dev, "Set HD-SDI input, 16-bits width\n");
//		dec_device->if_type = INTERFACE_TYPE_BT1120;
		gs2971_write_register(spi, GS2971_REG_ANC_CONFIG, 0);
		break;
	}

#if 0
	status = gs2971_write_register( spi, GS2971_REG_AUDIO_CONFIG,
			(GS2971_REG_AUDIO_CONFIG_SCLK_INV_MASK | GS2971_REG_AUDIO_CONFIG_MCLK_SEL_128FS));
#endif
	*id = V4L2_STD_NTSC;
	return 0;
}

static void gs2971_workqueue_handler(struct work_struct * data);

DECLARE_WORK(gs2971_worker, gs2971_workqueue_handler);

static void gs2971_workqueue_handler(struct work_struct *ignored)
{
//	struct gs2971_channel *ch;
	struct spi_device *spi;
	u16    bank_reg_base, anc_reg;
	static u16    anc_buf[257];
	int    anc_length;
	int    adf_found = 1;
	int    ch_id;
	int    status;
	u16    did,sdid;

	printk(KERN_ERR "****** In function %s\n",__FUNCTION__);
	for ( ch_id = 0; ch_id < GS2971_NUM_CHANNELS; ch_id++ ) {
		//ch = &gs2971_channel_info[ch_id];

		spi = gs2971_get_spi( spidata );
		if (!spi)
			continue;

		/* Step 2: start writing to other bank */
		gs2971_write_register(spi, GS2971_REG_ANC_CONFIG,
				GS2971_ANC_CONFIG_ANC_DATA_SWITCH);

#if 1
		/* Step 1: read ancillary data */
		bank_reg_base = GS2971_ANC_BANK_REG;
		status = 0;
		for ( anc_reg = bank_reg_base, adf_found = 1; (0 == status) &&
				((anc_reg+6) < bank_reg_base + GS2971_ANC_BANK_SIZE);) {
			status = gs2971_read_buffer(spi, anc_reg, anc_buf, 6);
			anc_reg += 6;

			if (anc_reg >= bank_reg_base + GS2971_ANC_BANK_SIZE)
				break;

			if (!status) {
				if (anc_buf[0] == 0 ||
				    anc_buf[1] == 0xff ||
				    anc_buf[2] == 0xff ) {
					did = anc_buf[3];
					sdid = anc_buf[4];
					anc_length = (anc_buf[5] & 0xff)+1;
					anc_reg += anc_length;

					if ( !( did ==0 && sdid == 0)
							&& ( did < 0xf8 ) ) {
						dev_dbg( &spi->dev, "anc[%x] %02x %02x %02x\n",
								anc_reg, did, sdid, anc_length);
					}
				} else {
					break;
				}
			}
		}
		dev_dbg( &spi->dev, "anc_end[%x] %02x %02x %02x\n",
				anc_reg, anc_buf[0], anc_buf[1], anc_buf[2]);
#endif

		/* Step 3: switch reads to other bank */
		gs2971_write_register(spi, GS2971_REG_ANC_CONFIG, 0);
	}
}


#if defined(GS2971_ENABLE_ANCILLARY_DATA)
static int gs2971_init_ancillary(struct spi_device *spi)
{
	u16 value;
	int offset;

	int status = 0;

	/* Set up ancillary data filtering */
	if (!status)
		status = gs2971_write_register( spi, GS2971_REG_ANC_TYPE1, 0x4100 );
	/*  SMPTE-352M Payload ID (0x4101) */
	if (!status)
		status = gs2971_write_register( spi, GS2971_REG_ANC_TYPE2, 0x5f00 ); /* Placeholder */

	if (!status)
		status = gs2971_write_register( spi, GS2971_REG_ANC_TYPE3, 0x6000 ); /* Placeholder */

	if (!status)
		status = gs2971_write_register( spi, GS2971_REG_ANC_TYPE4, 0x6100 );
	/* SMPTE 334 - SDID 01=EIA-708, 02=EIA-608 */

	if (!status)
		status = gs2971_write_register( spi, GS2971_REG_ANC_TYPE5, 0x6200 );
	/* SMPTE 334 - SDID 01=Program description, 02=Data broadcast, 03=VBI data */

	if ( 0 == gs2971_read_register( spi, GS2971_REG_ANC_TYPE1, &value ) ) {
		dev_dbg( &spi->dev, "REG_ANC_TYPE1 value x%04x\n",
				(unsigned int)value );
	}
	if ( 0 == gs2971_read_register( spi, GS2971_REG_ANC_TYPE2, &value ) ) {
		dev_dbg( &spi->dev, "REG_ANC_TYPE2 value x%04x\n",
				(unsigned int)value );
	}

	/* Clear old ancillary data */
	if (!status)
		status = gs2971_write_register(spi, GS2971_REG_ANC_CONFIG, GS2971_ANC_CONFIG_ANC_DATA_SWITCH);

	/* Step 2: start writing to other bank */
	if (!status)
		status = gs2971_write_register(spi, GS2971_REG_ANC_CONFIG, 0);
	return status;
}
#endif


/* gs2971_initialize :
 * This function will set the video format standard
 */
static int gs2971_initialize(void)
{
	// struct gs2971_channel *ch;
	struct spi_device *spi;
	int status = 0;

	// ch = to_gs2971(sd);
	try_module_get(THIS_MODULE);

	spi = gs2971_get_spi( spidata );

#if 0
	gs2971_write_register( spi, GS2971_REG_VIDEO_CONFIG,
			(GS2971_REG_VIDEO_CONFIG_861_PIN_DISABLE_MASK|
					GS2971_REG_VIDEO_CONFIG_TIMING_861_MASK));
#endif

#if defined(GS2971_ENABLE_ANCILLARY_DATA)
	gs2971_init_ancillary(spi);
#endif


#if 0
	if ( gs2971_timer.function == NULL ) {
		init_timer(&gs2971_timer);
		gs2971_timer.function = gs2971_timer_function;
	}
	mod_timer( &gs2971_timer, jiffies + msecs_to_jiffies( gs2971_timeout_ms ) );
#endif
	return status;
}

static int set_power(int on)
{
	struct fsl_mxc_camera_platform_data *gs2971_plat;
	struct spi_device  *spi;

	printk(KERN_ERR "***** In function %s\n",__FUNCTION__);
	spi = gs2971_get_spi(spidata);
	if (!spi)
		return -ENODEV;

	gs2971_plat = spi->dev.platform_data;
	if (on != gs2971_data.on ) {
		if (on) {
			if (gs2971_plat->pwdn)
				gs2971_plat->pwdn(0);
			msleep(400);
			//get_std();
		} else {
			if (gs2971_plat->pwdn)
				gs2971_plat->pwdn(1);
		}
		gs2971_data.on = on;
	}
	return 0;
}

/*!
 * Return attributes of current video standard.
 * Since this device autodetects the current standard, this function also
 * sets the values that need to be changed if the standard changes.
 * There is no set std equivalent function.
 *
 *  @return		None.
 */
static void gs2971_get_std(struct v4l2_int_device *s, v4l2_std_id *std)
{
	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
}

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
	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);

	if (!s) {
		pr_err("   ERROR!! no slave device set!\n");
		return -1;
	}

	/* Initialize structure to 0s then set any non-0 values. */
	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = gs2971_data.mclk;
	p->if_type = V4L2_IF_TYPE_BT656; /* This is the only possibility. */
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	//p->u.bt656.clock_min = 6000000;
	//p->u.bt656.clock_max = 24000000;
	//p->u.bt656.bt_sync_correct = 1;
	return 0;
}

static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	printk(KERN_ERR "********In function: %s with on in %d\n",__FUNCTION__, on);
	return set_power(on);
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
	struct sensor_data *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;

	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
	switch (a->type) {
	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		pr_debug("   type is V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = gs2971_data.streamcap.capability;
		cparm->timeperframe = gs2971_data.streamcap.timeperframe;
		cparm->capturemode = gs2971_data.streamcap.capturemode;
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
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	int ret = 0;

	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
	/* Make sure power on */
	set_power(1);

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = 60;
			timeperframe->numerator = 1;
		}
		gs2971_data.streamcap.timeperframe = *timeperframe;
		gs2971_data.streamcap.capturemode =
				(u32)a->parm.capture.capturemode;
		break;

		/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n", a->type);
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
	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
	struct sensor_data *sensor = s->priv;
	int std;

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		//pr_debug("   Returning size of %dx%d\n",
		//adv->sen.pix.width, adv->sen.pix.height);
		printk(KERN_ERR "********In V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
		get_std(s,&std);
		f->fmt.pix = gs2971_data.pix;
		break;

	case V4L2_BUF_TYPE_PRIVATE:
		f->fmt.pix.pixelformat = (u32)std;
		printk(KERN_ERR "********In V4L2_BUF_TYPE_PRIVATE\n");
		break;

	default:
		printk(KERN_ERR "********In DEFAULT\n");
		f->fmt.pix = gs2971_data.pix;
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
static int ioctl_queryctrl(struct v4l2_int_device *s,
			   struct v4l2_queryctrl *qc)
{
	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
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
	int ret = 0;

	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = gs2971_data.brightness;
		break;
	case V4L2_CID_HUE:
		vc->value = gs2971_data.hue;
		break;
	case V4L2_CID_CONTRAST:
		vc->value = gs2971_data.contrast;
		break;
	case V4L2_CID_SATURATION:
		vc->value = gs2971_data.saturation;
		break;
	case V4L2_CID_RED_BALANCE:
		vc->value = gs2971_data.red;
		break;
	case V4L2_CID_BLUE_BALANCE:
		vc->value = gs2971_data.blue;
		break;
	case V4L2_CID_EXPOSURE:
		vc->value = gs2971_data.ae_mode;
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
	int retval = 0;

	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
	///* Make sure power on */
	set_power(1);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		dev_dbg(spidata->spi, "   V4L2_CID_BRIGHTNESS\n");
		break;
	case V4L2_CID_CONTRAST:
		dev_dbg(spidata->spi, "   V4L2_CID_CONTRAST\n");
		break;
	case V4L2_CID_SATURATION:
		dev_dbg(spidata->spi, "   V4L2_CID_SATURATION\n");
		break;
	case V4L2_CID_HUE:
		dev_dbg(spidata->spi, "   V4L2_CID_HUE\n");
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		dev_dbg(spidata->spi, "   V4L2_CID_AUTO_WHITE_BALANCE\n");
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		dev_dbg(spidata->spi, "   V4L2_CID_DO_WHITE_BALANCE\n");
		break;
	case V4L2_CID_RED_BALANCE:
		dev_dbg(spidata->spi, "   V4L2_CID_RED_BALANCE\n");
		break;
	case V4L2_CID_BLUE_BALANCE:
		dev_dbg(spidata->spi, "   V4L2_CID_BLUE_BALANCE\n");
		break;
	case V4L2_CID_GAMMA:
		dev_dbg(spidata->spi, "   V4L2_CID_GAMMA\n");
		break;
	case V4L2_CID_EXPOSURE:
		dev_dbg(spidata->spi, "   V4L2_CID_EXPOSURE\n");
		break;
	case V4L2_CID_AUTOGAIN:
		dev_dbg(spidata->spi, "   V4L2_CID_AUTOGAIN\n");
		break;
	case V4L2_CID_GAIN:
		dev_dbg(spidata->spi, "   V4L2_CID_GAIN\n");
		break;
	case V4L2_CID_HFLIP:
		dev_dbg(spidata->spi, "   V4L2_CID_HFLIP\n");
		break;
	case V4L2_CID_VFLIP:
		dev_dbg(spidata->spi, "   V4L2_CID_VFLIP\n");
		break;
	default:
		dev_dbg(spidata->spi, "   Default case\n");
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
	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
	if (fsize->index > 2)
		return -EINVAL;

	fsize->pixel_format = gs2971_data.pix.pixelformat;
	fsize->discrete.width = 1280;//video_fmts[fsize->index].active_width;
	fsize->discrete.height = 720; //video_fmts[fsize->index].active_height;
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
	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
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
	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
	struct sensor_data *gs = s->priv;

	if (fmt->index > 0)
		return -EINVAL;
	fmt->pixelformat = gs2971_data.pix.pixelformat;//gs->pix.pixelformat;
	return 0;
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	printk(KERN_ERR "********In function: %s\n",__FUNCTION__);
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
	return 0;
}

/*!
 * This structure defines all the ioctls for this module.
 */
static struct v4l2_int_ioctl_desc gs2971_ioctl_desc[] = {

	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func*)ioctl_dev_init},

	/*!
	 * Delinitialise the dev. at slave detach.
	 * The complement of ioctl_dev_init.
	 */
/*	{vidioc_int_dev_exit_num, (v4l2_int_ioctl_func *)ioctl_dev_exit}, */

	{vidioc_int_s_power_num, (v4l2_int_ioctl_func*)ioctl_s_power},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func*)ioctl_g_ifparm},
/*	{vidioc_int_g_needs_reset_num,
				(v4l2_int_ioctl_func *)ioctl_g_needs_reset}, */
/*	{vidioc_int_reset_num, (v4l2_int_ioctl_func *)ioctl_reset}, */
	{vidioc_int_init_num, (v4l2_int_ioctl_func*)ioctl_init},

	/*!
	 * VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
	 */
	{vidioc_int_enum_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_enum_fmt_cap},

	/*!
	 * VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.
	 * This ioctl is used to negotiate the image capture size and
	 * pixel format without actually making it take effect.
	 */
/*	{vidioc_int_try_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_try_fmt_cap}, */

	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func*)ioctl_g_fmt_cap},

	/*!
	 * If the requested format is supported, configures the HW to use that
	 * format, returns error code if format not supported or HW can't be
	 * correctly configured.
	 */
/*	{vidioc_int_s_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_s_fmt_cap}, */

	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func*)ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func*)ioctl_s_parm},
	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func*)ioctl_queryctrl},
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func*)ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func*)ioctl_s_ctrl},
	{vidioc_int_enum_framesizes_num,
				(v4l2_int_ioctl_func *)ioctl_enum_framesizes},
	{vidioc_int_g_chip_ident_num,
				(v4l2_int_ioctl_func *)ioctl_g_chip_ident},
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

/*! ADV7180 I2C attach function.
 *
 *  @param *adapter	struct i2c_adapter *.
 *
 *  @return		Error code indicating success or failure.
 */

/*!
 * ADV7180 I2C probe function.
 * Function set in i2c_driver struct.
 * Called by insmod.
 *
 *  @param *adapter	I2C adapter descriptor.
 *
 *  @return		Error code indicating success or failure.
 */
static int gs2971_probe(struct spi_device *spi)
{
	struct fsl_mxc_camera_platform_data *gs2971_plat = spi->dev.platform_data;
	struct proc_dir_entry *pde ;
	//struct gs2971_spidata	*spidata;
	int			status = 0;
	//struct gs2971_channel      *channel;

	printk(KERN_ERR "In function %s\n",__FUNCTION__);
	dev_dbg( &spi->dev, "Enter gs2971_probe\n" );

	/* Allocate driver data */
	spidata = kzalloc(sizeof(*spidata), GFP_KERNEL);
	if (!spidata)
	  return -ENOMEM;

	/* Initialize the driver data */
	spidata->spi = spi;
	mutex_init(&spidata->buf_lock);

	/* Set initial values for the sensor struct. */
	memset(&gs2971_data, 0, sizeof(gs2971_data));
	gs2971_data.mclk = 24000000; /* 6 - 54 MHz, typical 24MHz */
	gs2971_data.mclk = gs2971_plat->mclk;
	gs2971_data.mclk_source = gs2971_plat->mclk_source;
	gs2971_data.csi = gs2971_plat->csi;
	gs2971_data.ipu = gs2971_plat->ipu;
	gs2971_data.io_init = gs2971_plat->io_init;

	//ov5642_data.i2c_client = client;
	gs2971_data.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	gs2971_data.pix.width = 1280;
	gs2971_data.pix.height = 720;
	gs2971_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
					   V4L2_CAP_TIMEPERFRAME;
	gs2971_data.streamcap.capturemode = 0;
	gs2971_data.streamcap.timeperframe.denominator = 60;//DEFAULT_FPS;
	gs2971_data.streamcap.timeperframe.numerator = 1;
	//gs2971_data.pix.priv = 1;  /* 1 is used to indicate TV in */
	gs2971_data.on = true;

	if (!gs2971_plat) {
		pr_err("%s: Platform data needed\n", __func__);
		return -ENOMEM;
	}

	if (gs2971_plat->io_init)
		gs2971_plat->io_init();

	/*if (gs2971_plat->reset)
		gs2971_plat->reset();*/

	if (gs2971_plat->pwdn)
		gs2971_plat->pwdn(0);

	msleep(1);

	gs2971_int_device.priv = &gs2971_data;

	pde = create_proc_entry("driver/gs2971", 0, 0);
	if( pde ) {
		//pde->write_proc = write_proc ;
		pde->data = &gs2971_int_device ;
	} else {
		printk( KERN_ERR "Error creating gs2971 proc entry\n" );
	}
	status = v4l2_int_device_register(&gs2971_int_device);

	if (!status) {
             status = gs2971_initialize();
     }
     printk (KERN_ERR "gs2971_probe returns %d\n",status );
     return status;
     //return 0;
}

static int gs2971_remove(struct spi_device *spi)
{
	printk(KERN_ERR "In function %s\n",__FUNCTION__);
	remove_proc_entry("driver/gs2971", NULL);
	kfree( spidata );
	v4l2_int_device_unregister(&gs2971_int_device);
	return 0;
}

/*!
 * gs2971 init function.
 * Called on insmod.
 *
 * @return    Error code indicating success or failure.
 */
static int __init gs2971_init(void)
{
	int status;

	printk(KERN_ERR "In function %s\n",__FUNCTION__);
	status = spi_register_driver(&gs2971_spi);
	printk(KERN_ERR "Status: %d\n",status);
	return status;
}

/*!
 * gs2971 cleanup function.
 * Called on rmmod.
 *
 * @return   Error code indicating success or failure.
 */
static void __exit gs2971_exit(void)
{
	printk(KERN_ERR "In function %s\n",__FUNCTION__);
	spi_unregister_driver(&gs2971_spi);
}


module_init(gs2971_init);
module_exit(gs2971_exit);
MODULE_LICENSE("GPL");
