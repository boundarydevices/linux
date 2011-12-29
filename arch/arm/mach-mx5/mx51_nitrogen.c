/*
 * Copyright 2009-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/mtd/mtd.h>
#if defined(CONFIG_MTD)
#include <linux/mtd/map.h>
#endif
#include <linux/mtd/partitions.h>
#include <linux/regulator/consumer.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/pwm_backlight.h>
#include <linux/powerkey.h>
#include <linux/gpio.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/keypad.h>
#include <asm/mach/flash.h>
#include <mach/gpio.h>
#include <mach/mmc.h>
#include <mach/mxc_dvfs.h>
#include <mach/mxc_edid.h>
#include <mach/iomux-mx51.h>
#include <mach/i2c.h>
#include <mach/mxc_iim.h>
#include <linux/android_pmem.h>
#include <linux/usb/android_composite.h>
#ifdef CONFIG_KEYBOARD_GPIO
#include <linux/gpio_keys.h>
#endif
#ifdef CONFIG_MAGSTRIPE_MODULE
#include <linux/magstripe.h>
#endif

#include "devices.h"
#include "crm_regs.h"
#include "usb.h"

#define MX51_UART1_PAD_CTRL	(PAD_CTL_HYS | PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_DSE_HIGH)
#define MX51_PAD_CTRL_12	(PAD_CTL_PKE | PAD_CTL_DSE_LOW | PAD_CTL_SRE_SLOW)
#define MX51_PAD_CTRL_13	(PAD_CTL_PKE | PAD_CTL_PUE)


#define MX51_PAD_AUD3_BB_CK__AUD3_BB_CK	IOMUX_PAD(0x5F8, 0x208, 0, 0x0, 0, MX51_PAD_CTRL_10)
#define MX51_PAD_AUD3_BB_FS__AUD3_BB_FS	IOMUX_PAD(0x5FC, 0x20C, 0, 0x0, 0, MX51_PAD_CTRL_10)
#define MX51_PAD_AUD3_BB_RXD__AUD3_BB_RXD	IOMUX_PAD(0x5F4, 0x204, 0, 0x0, 0, MX51_PAD_CTRL_10)
#define MX51_PAD_AUD3_BB_TXD__AUD3_BB_TXD	IOMUX_PAD(0x5F0, 0x200, 0, 0x0, 0, MX51_PAD_CTRL_10)
#define MX51_PAD_CSI2_D12__GPIO_4_9		IOMUX_PAD(0x5BC, 0x1CC, 3, 0x0, 0, MX51_PAD_CTRL_7)
#define MX51_PAD_CSI2_D13__GPIO_4_10		IOMUX_PAD(0x5C0, 0x1D0, 3, 0x0, 0, MX51_PAD_CTRL_7)
#define MX51_PAD_CSI2_D19__GPIO_4_12		IOMUX_PAD(0x5D8, 0x1E8, 3, 0x0, 0, MX51_PAD_CTRL_12)
#define MX51_PAD_CSI2_HSYNC__GPIO_4_14	IOMUX_PAD(0x5E0, 0x1F0, 3, 0x0, 0, MX51_GPIO_PAD_CTRL)
#define MX51_PAD_CSPI1_RDY__GPIO_4_26	IOMUX_PAD(0x610, 0x220, 3, 0x0, 0, MX51_PAD_CTRL_10 | MX51_PAD_CTRL_13)
#define MX51_PAD_CSPI1_SS0__CSPI1_SS0		IOMUX_PAD(0x608, 0x218, 0, 0x0, 0, (PAD_CTL_HYS | PAD_CTL_DSE_HIGH | PAD_CTL_SRE_FAST))
#define MX51_PAD_CSPI1_SS0__GPIO_4_24	IOMUX_PAD(0x608, 0x218, 3, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_CSPI1_SS0__GPIO_4_24	IOMUX_PAD(0x608, 0x218, 3, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_CSPI1_SS1__CSPI1_SS1		IOMUX_PAD(0x60C, 0x21C, 0, 0x0, 0, (PAD_CTL_HYS | PAD_CTL_DSE_HIGH | PAD_CTL_SRE_FAST))
#define MX51_PAD_CSPI1_SS1__GPIO_4_25	IOMUX_PAD(0x60c, 0x21c, 3, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_DI1_D0_CS__GPIO_3_3		IOMUX_PAD(0x6B4, 0x2B4, 4, 0x0, 0, MX51_PAD_CTRL_7)
#define MX51_PAD_DI1_PIN11__GPIO_3_0		IOMUX_PAD(0x6A8, 0x2A8, 4, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_DISPB2_SER_CLK__GPIO_3_7	IOMUX_PAD(0x6C4, 0x2C4, 4, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_DISPB2_SER_DIN__GPIO_3_5	IOMUX_PAD(0x6BC, 0x2BC, 4, 0x988, 1, NO_PAD_CTRL)
#define MX51_PAD_DISPB2_SER_DIO__GPIO_3_6	IOMUX_PAD(0x6C0, 0x2C0, 4, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A16__GPIO_2_10	IOMUX_PAD(0x430, 0x09c, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A17__GPIO_2_11	IOMUX_PAD(0x434, 0x0a0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A17__GPIO_2_11	IOMUX_PAD(0x434, 0x0a0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A18__GPIO_2_12	IOMUX_PAD(0x438, 0x0a4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A18__GPIO_2_12	IOMUX_PAD(0x438, 0x0a4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A19__GPIO_2_13	IOMUX_PAD(0x43c, 0x0a8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A19__GPIO_2_13	IOMUX_PAD(0x43c, 0x0a8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A20__GPIO_2_14	IOMUX_PAD(0x440, 0x0ac, 1, 0x0,   0, PAD_CTL_PKE)
#define MX51_PAD_EIM_A20__GPIO_2_14	IOMUX_PAD(0x440, 0x0ac, 1, 0x0,   0, PAD_CTL_PKE)
#define MX51_PAD_EIM_A21__GPIO_2_15	IOMUX_PAD(0x444, 0x0b0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A21__GPIO_2_15	IOMUX_PAD(0x444, 0x0b0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A22__GPIO_2_16	IOMUX_PAD(0x448, 0x0b4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A22__GPIO_2_16	IOMUX_PAD(0x448, 0x0b4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A23__GPIO_2_17	IOMUX_PAD(0x44c, 0x0b8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A23__GPIO_2_17	IOMUX_PAD(0x44c, 0x0b8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A24__GPIO_2_18	IOMUX_PAD(0x450, 0x0bc, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A27__GPIO_2_21	IOMUX_PAD(0x45c, 0x0c8, 1, 0x0,   0, MX51_PAD_CTRL_10)
#define MX51_PAD_EIM_CRE__GPIO_3_2		IOMUX_PAD(0x4A0, 0x100, 1, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_EIM_CS2__FEC_RDAT2	IOMUX_PAD(0x47c, 0x0e8, 3, 0x0,   0, MX51_PAD_CTRL_2)
#define MX51_PAD_EIM_CS2__FEC_RDAT2	IOMUX_PAD(0x47c, 0x0e8, 3, 0x0,   0, MX51_PAD_CTRL_2)
#define MX51_PAD_EIM_CS3__FEC_RDAT3	IOMUX_PAD(0x480, 0x0ec, 3, 0x0,   0, MX51_PAD_CTRL_2)
#define MX51_PAD_EIM_CS3__FEC_RDAT3	IOMUX_PAD(0x480, 0x0ec, 3, 0x0,   0, MX51_PAD_CTRL_2)
#define MX51_PAD_EIM_D17__GPIO_2_1	IOMUX_PAD(0x3f4, 0x060, 1, 0x0, 0, MX51_GPIO_PAD_CTRL)
#define MX51_PAD_EIM_D18__GPIO_2_2	IOMUX_PAD(0x3f8, 0x064, 1, 0x0, 0, MX51_PAD_CTRL_10 | MX51_PAD_CTRL_13)
#define MX51_PAD_EIM_D18__GPIO_2_2	IOMUX_PAD(0x3f8, 0x064, 1, 0x0, 0, MX51_PAD_CTRL_10 | MX51_PAD_CTRL_13)
#define MX51_PAD_EIM_D19__GPIO_2_3	IOMUX_PAD(0x3fc, 0x068, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D21__GPIO_2_5	IOMUX_PAD(0x404, 0x070, 1, 0x0, 0, MX51_PAD_CTRL_10 | MX51_PAD_CTRL_13)
#define MX51_PAD_EIM_D22__GPIO_2_6	IOMUX_PAD(0x408, 0x074, 1, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D23__GPIO_2_7	IOMUX_PAD(0x40c, 0x078, 1, 0x0, 0, MX51_PAD_CTRL_7)
#define MX51_PAD_EIM_D29__IDDIG		IOMUX_PAD(0x424, 0x090, 7, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DTACK__GPIO_2_31	IOMUX_PAD(0x48c, 0x0f8, 1, 0x0,   0, MX51_PAD_CTRL_3)
#define MX51_PAD_EIM_EB2__GPIO_2_22	IOMUX_PAD(0x468, 0x0d4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_EB3__FEC_RDAT1	IOMUX_PAD(0x46c, 0x0d8, 3, 0x0,   0, MX51_PAD_CTRL_2)
#define MX51_PAD_EIM_LBA__GPIO_3_1		IOMUX_PAD(0x494, 0xFC, 1, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_0__GPIO_1_0	IOMUX_PAD(0x7B4, 0x3AC, 1 | IOMUX_CONFIG_SION, 0x0, 0, MX51_PAD_CTRL_9)
#define MX51_PAD_GPIO_1_1__GPIO_1_1	IOMUX_PAD(0x7B8, 0x3B0, 1, 0x0, 0, MX51_PAD_CTRL_9)
#define MX51_PAD_GPIO_1_1__GPIO_1_1	IOMUX_PAD(0x7B8, 0x3B0, 1, 0x0, 0, MX51_PAD_CTRL_9)
#define MX51_PAD_GPIO_1_2__PWM_PWMO	IOMUX_PAD(0x7D4, 0x3CC, 1, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_3__GPIO_1_3		IOMUX_PAD(0x7D8, 0x3D0, 0, 0x0, 0, MX51_UART3_PAD_CTRL)
#define MX51_PAD_GPIO_1_3__I2C2_SDA		IOMUX_PAD(0x7D8, 0x3D0, 2 | IOMUX_CONFIG_SION, 0x9bc, 3, MX51_I2C_PAD_CTRL)
#define MX51_PAD_GPIO_1_4__GPIO_1_4	IOMUX_PAD(0x804, 0x3D8, IOMUX_CONFIG_SION, 0x0, 0, MX51_PAD_CTRL_9)
#define MX51_PAD_GPIO_1_5__GPIO_1_5	IOMUX_PAD(0x808, 0x3DC, IOMUX_CONFIG_SION, 0x0, 0, MX51_PAD_CTRL_9)
#define MX51_PAD_GPIO_1_5__GPIO_1_5	IOMUX_PAD(0x808, 0x3DC, IOMUX_CONFIG_SION, 0x0, 0, MX51_PAD_CTRL_9)
#define MX51_PAD_GPIO_1_6__GPIO_1_6	IOMUX_PAD(0x80C, 0x3E0, IOMUX_CONFIG_SION, 0x0, 0, MX51_PAD_CTRL_9)
#define MX51_PAD_GPIO_1_6__GPIO_1_6	IOMUX_PAD(0x80C, 0x3E0, IOMUX_CONFIG_SION, 0x0, 0, MX51_PAD_CTRL_9)
#define MX51_PAD_GPIO_1_7__GPIO_1_7	IOMUX_PAD(0x810, 0x3E4, 0, 0x0, 0, MX51_GPIO_PAD_CTRL)
#define MX51_PAD_GPIO_1_7__GPIO_1_7	IOMUX_PAD(0x810, 0x3E4, 0, 0x0, 0, MX51_GPIO_PAD_CTRL)
#define MX51_PAD_GPIO_1_8__GPIO_1_8	IOMUX_PAD(0x814, 0x3E8, IOMUX_CONFIG_SION, 0x0 , 1, MX51_PAD_CTRL_6)
#define MX51_PAD_GPIO_1_8__GPIO_1_8	IOMUX_PAD(0x814, 0x3E8, IOMUX_CONFIG_SION, 0x0 , 1, MX51_PAD_CTRL_6)
#define MX51_PAD_GPIO_1_9__GPIO_1_9	IOMUX_PAD(0x818, 0x3EC, 0, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_9__GPIO_1_9	IOMUX_PAD(0x818, 0x3EC, 0, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_I2C1_CLK__GPIO_4_16		IOMUX_PAD(0x5E8, 0x1F8, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_I2C1_CLK__HSI2C_CLK		IOMUX_PAD(0x5E8, 0x1F8, IOMUX_CONFIG_SION, 0x0, 0, MX51_PAD_CTRL_8)
#define MX51_PAD_I2C1_DAT__GPIO_4_17		IOMUX_PAD(0x5EC, 0x1FC, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_I2C1_DAT__GPIO_4_17		IOMUX_PAD(0x5EC, 0x1FC, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_I2C1_DAT__HSI2C_DAT		IOMUX_PAD(0x5EC, 0x1FC, IOMUX_CONFIG_SION, 0x0, 0, MX51_PAD_CTRL_8)
#define MX51_PAD_NANDF_CS0__GPIO_3_16	IOMUX_PAD(0x518, 0x130, 3, 0x0, 0, PAD_CTL_PUS_100K_UP)
#define MX51_PAD_NANDF_CS1__GPIO_3_17	IOMUX_PAD(0x51C, 0x134, 3, 0x0, 0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CS4__FEC_TDAT1	IOMUX_PAD(0x528, 0x140, 2, 0x0, 0, MX51_PAD_CTRL_5)
#define MX51_PAD_NANDF_CS5__FEC_TDAT2	IOMUX_PAD(0x52C, 0x144, 2, 0x0, 0, MX51_PAD_CTRL_5)
#define MX51_PAD_NANDF_CS6__FEC_TDAT3	IOMUX_PAD(0x530, 0x148, 2, 0x0, 0, MX51_PAD_CTRL_5)
#define MX51_PAD_NANDF_D12__GPIO_3_28	IOMUX_PAD(0x548, 0x160, 3, 0x0, 0, PAD_CTL_PUS_100K_UP | PAD_CTL_DVS)
#define MX51_PAD_NANDF_D14__GPIO_3_26	IOMUX_PAD(0x540, 0x158, 3, 0x0, 0, PAD_CTL_PUS_100K_UP)
#define MX51_PAD_NANDF_RB3__FEC_RXCLK	IOMUX_PAD(0x504, 0x128, 1, 0x0, 0, MX51_PAD_CTRL_2)
#define MX51_PAD_NANDF_RB6__FEC_RDAT0	IOMUX_PAD(0x5DC, 0x134, 1, 0x0, 0, MX51_PAD_CTRL_4)
#define MX51_PAD_NANDF_RB7__FEC_TDAT0	IOMUX_PAD(0x5E0, 0x138, 1, 0x0, 0, MX51_PAD_CTRL_5)
#define MX51_PAD_OWIRE_LINE__SPDIF_OUT1	IOMUX_PAD(0x638, 0x248, 6, 0x0, 0, MX51_PAD_CTRL_10 | MX51_PAD_CTRL_13)
#define MX51_PAD_UART1_CTS__GPIO_4_31	IOMUX_PAD(0x624, 0x234, 3, 0x0, 0, MX51_UART1_PAD_CTRL)
#define MX51_PAD_UART1_RTS__GPIO_4_30	IOMUX_PAD(0x620, 0x230, 3, 0x9e0, 0, MX51_UART1_PAD_CTRL)
#define MX51_PAD_UART3_RXD__GPIO_1_22	IOMUX_PAD(0x630, 0x240, 3, 0x0, 0, MX51_PAD_CTRL_1)

#define MAKE_GP(port, bit) ((port - 1) * 32 + bit)
//This file defaults to Nitrogen_E settings
//The other boards will override values

/*!
 * @file mach-mx51/mx51_nitrogen.c
 *
 * @brief This file contains the board specific initialization routines.
 *
 * @ingroup MSL_MX51
 */


/*
 * differences between Nitrogen variants:
 *
 * Pad		GP		Nitrogen-P (handheld)		Nitrogen-E		Nitrogen-VM (vertical mount)
 * ----------	--------	-----------------------		--------------------	----------------------------
 * CSPI1_RDY	GP4_26		GPIO				I2C(Pic) interrupt	N/C
 * EIM_D17	GP2_1		USB/Audio clock enable		N/C			I2C(Pic) interrupt
 */

#define GP_SD1_CD			MAKE_GP(1, 0)
#define GP_SD1_WP			MAKE_GP(1, 1)
#define GP_POWER_KEY			MAKE_GP(2, 21)
#define CAMERA_STROBE			MAKE_GP(4, 9)
#define CAMERA_POWERDOWN		MAKE_GP(4, 10)
#define CAMERA_RESET			MAKE_GP(4, 14)

struct gpio nitrogen_gpios[] __initdata = {
	{.label="I2C_connector_int",	.gpio = MAKE_GP(4, 26),		.flags = GPIOF_DIR_IN},	/* overriden by i2c_generic_data.gp, DON'T REORDER  */
	{.label="sdhc1-detect",		.gpio = GP_SD1_CD,		.flags = GPIOF_DIR_IN},		/* MAKE_GP(1, 0) */
	{.label="sdhc1-wp",		.gpio = GP_SD1_WP,		.flags = GPIOF_DIR_IN},		/* MAKE_GP(1, 1) */
	{.label="pmic-int",		.gpio = MAKE_GP(1, 8),		.flags = GPIOF_DIR_IN},
	{.label="usb-overcurrent",	.gpio = MAKE_GP(1, 9),		.flags = GPIOF_DIR_IN},		/* current rev */
	{.label="power-key",		.gpio = GP_POWER_KEY,		.flags = GPIOF_DIR_IN},		/* MAKE_GP(2, 21) */
	{.label="usb-overcurrent_r1",	.gpio = MAKE_GP(3, 0),		.flags = GPIOF_DIR_IN},		/* 1st rev */
	{.label="tfp410int",		.gpio = MAKE_GP(3, 28),		.flags = GPIOF_DIR_IN},		/* same as i2c_tfp410_data.gp */
	{.label="hs_i2c_clk",		.gpio = MAKE_GP(4, 16),		.flags = GPIOF_DIR_IN},
	{.label="hs_i2c_data",		.gpio = MAKE_GP(4, 17),		.flags = GPIOF_DIR_IN},
	{.label="gp_4_30",		.gpio = MAKE_GP(4, 30),		.flags = GPIOF_DIR_IN},		/* release immediately */
	{.label="gp_4_31",		.gpio = MAKE_GP(4, 31),		.flags = GPIOF_DIR_IN},		/* release immediately */
#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) \
 || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
	{.label="camera_strobe",	.gpio = CAMERA_STROBE,		.flags = GPIOF_DIR_OUT},
	{.label="camera_powerdown",	.gpio = CAMERA_POWERDOWN,	.flags = GPIOF_DIR_OUT},
	{.label="camera_reset",		.gpio = CAMERA_RESET,		.flags = GPIOF_DIR_OUT},
#endif


#define GP_USBH1_HUB_RST		MAKE_GP(1, 7)		//Normally, gpio1[7] is esdhc2 - wp
								//Normally, gpio1[8] is esdhc2 - cd
#define GP_FEC_PHY_RESET		MAKE_GP(2, 14)
#define GP_FM_RESET			MAKE_GP(2, 15)
#define GP_AUDAMP_STBY			MAKE_GP(2, 17)

#define GP_26M_OSC_EN			MAKE_GP(3, 1)
#define GP_LVDS_POWER_DOWN		MAKE_GP(3, 3)
#define GP_DISP_BRIGHTNESS_CTL		MAKE_GP(3, 4)

#define GP_LCD_3V3_ON			MAKE_GP(4, 9)
#define GP_LCD_5V_ON			MAKE_GP(4, 10)
#define GP_CSP1_SS0			MAKE_GP(4, 24)
#define GP_CSP1_SS1			MAKE_GP(4, 25)

	{.label = "gp_1_5",		.gpio = MAKE_GP(1, 5),		.flags = GPIOF_INIT_HIGH},
	{.label = "gp_1_6",		.gpio = MAKE_GP(1, 6),		.flags = GPIOF_INIT_HIGH},
	{.label = "hub-rst",		.gpio = GP_USBH1_HUB_RST,	.flags = 0},			/* MAKE_GP(1, 7) */
	{.label = "osc-en",		.gpio = MAKE_GP(2, 2),		.flags = GPIOF_INIT_HIGH},
	{.label = "usb-phy-reset",	.gpio = MAKE_GP(2, 5),		.flags = GPIOF_INIT_HIGH},
	{.label = "cam-reset",		.gpio = MAKE_GP(2, 7),		.flags = GPIOF_INIT_HIGH},
	{.label = "OTG_power_enable",	.gpio = MAKE_GP(2, 10),		.flags = GPIOF_INIT_HIGH},
	{.label = "on_off_to_pmic",	.gpio = MAKE_GP(2, 11),		.flags = GPIOF_INIT_HIGH},
	{.label = "fec-phy-reset",	.gpio = GP_FEC_PHY_RESET,	.flags = 0},			/* MAKE_GP(2, 14) */
	{.label = "fm-reset",		.gpio = GP_FM_RESET,		.flags = 0},			/* MAKE_GP(2, 15) */
	{.label = "audioamp-stdby",	.gpio = GP_AUDAMP_STBY,		.flags = 0},			/* MAKE_GP(2, 17) */
	{.label = "26m-osc-en",		.gpio = GP_26M_OSC_EN,		.flags = GPIOF_INIT_HIGH},	/* MAKE_GP(3, 1) */
	{.label = "lvds-power-down",	.gpio = GP_LVDS_POWER_DOWN,	.flags = 0},			/* MAKE_GP(3, 3) */
	{.label = "disp-brightness",	.gpio = GP_DISP_BRIGHTNESS_CTL, .flags = 0},			/* MAKE_GP(3, 4) */
	{.label = "tfp410_i2cmode",	.gpio = MAKE_GP(3, 5),		.flags = 0},			/* same as i2c_tfp410_data.gp_i2c_sel */
	{.label = "lcd-3v3-on",		.gpio = GP_LCD_3V3_ON,		.flags = 0},			/* MAKE_GP(4, 9) */
	{.label = "lcd-5v-on",		.gpio = GP_LCD_5V_ON,		.flags = 0},			/* MAKE_GP(4, 10) */
	{.label = "cam-low-power",	.gpio = MAKE_GP(4, 12),		.flags = 0},
	{.label = "cspi1-ss0",		.gpio = GP_CSP1_SS0,		.flags = 0},			/* MAKE_GP(4, 24) */
	{.label = "cspi1-ss1",		.gpio = GP_CSP1_SS1,		.flags = GPIOF_INIT_HIGH},	/* MAKE_GP(4, 25) */
};

void mx51_reboot_setup(void)
{
	/* workaround for ENGcm09397 - Fix SPI NOR reset issue*/
	/* de-select SS0 of instance: eCSPI1 */
	iomux_v3_cfg_t cspi1_ss0_gpio = MX51_PAD_CSPI1_SS0__GPIO_4_24;
	iomux_v3_cfg_t cspi1_ss1_gpio = MX51_PAD_CSPI1_SS1__GPIO_4_25;

	gpio_set_value(GP_CSP1_SS0, 0);	/* SS0 high active */
	gpio_set_value(GP_CSP1_SS1, 1); /* SS1 low active */
	mxc_iomux_v3_setup_pad(cspi1_ss0_gpio);
	mxc_iomux_v3_setup_pad(cspi1_ss1_gpio);
}


extern int __init mx51_nitrogen_init_mc13892(void);
extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void (*set_num_cpu_wp)(int num);
static int num_cpu_wp = 3;

static iomux_v3_cfg_t mx51nitrogen_pads[] __initdata = {
	/* UART1 */
	MX51_PAD_UART1_RXD__UART1_RXD,
	MX51_PAD_UART1_TXD__UART1_TXD,
	MX51_PAD_UART1_RTS__GPIO_4_30,
	MX51_PAD_UART1_CTS__GPIO_4_31,
	MX51_PAD_UART2_RXD__UART2_RXD,
	MX51_PAD_UART2_TXD__UART2_TXD,

	/* USB HOST1 */
	MX51_PAD_USBH1_STP__USBH1_STP,
	MX51_PAD_USBH1_CLK__USBH1_CLK,
	MX51_PAD_USBH1_DIR__USBH1_DIR,
	MX51_PAD_USBH1_NXT__USBH1_NXT,
	MX51_PAD_USBH1_DATA0__USBH1_DATA0,
	MX51_PAD_USBH1_DATA1__USBH1_DATA1,
	MX51_PAD_USBH1_DATA2__USBH1_DATA2,
	MX51_PAD_USBH1_DATA3__USBH1_DATA3,
	MX51_PAD_USBH1_DATA4__USBH1_DATA4,
	MX51_PAD_USBH1_DATA5__USBH1_DATA5,
	MX51_PAD_USBH1_DATA6__USBH1_DATA6,
	MX51_PAD_USBH1_DATA7__USBH1_DATA7,

	MX51_PAD_GPIO_1_0__GPIO_1_0,
	MX51_PAD_GPIO_1_1__GPIO_1_1,
        MX51_PAD_GPIO_1_3__GPIO_1_3,
	MX51_PAD_GPIO_1_4__GPIO_1_4,
	MX51_PAD_GPIO_1_5__GPIO_1_5,
	MX51_PAD_GPIO_1_6__GPIO_1_6,
	MX51_PAD_GPIO_1_7__GPIO_1_7,
	MX51_PAD_GPIO_1_8__GPIO_1_8,
	MX51_PAD_GPIO_1_9__GPIO_1_9,
	MX51_PAD_UART3_RXD__UART3_RXD,
        MX51_PAD_UART3_TXD__UART3_TXD,

	MX51_PAD_EIM_D17__GPIO_2_1,
	MX51_PAD_EIM_D18__GPIO_2_2,
	MX51_PAD_EIM_D21__GPIO_2_5,
	MX51_PAD_EIM_D23__GPIO_2_7,
	MX51_PAD_EIM_D24__I2C2_SDA,
	MX51_PAD_EIM_D27__I2C2_SCL,
	MX51_PAD_EIM_D29__IDDIG,
	MX51_PAD_EIM_A16__GPIO_2_10,
	MX51_PAD_EIM_A17__GPIO_2_11,
	MX51_PAD_EIM_A18__GPIO_2_12,
	MX51_PAD_EIM_A19__GPIO_2_13,
	MX51_PAD_EIM_A20__GPIO_2_14,
	MX51_PAD_EIM_A21__GPIO_2_15,
	MX51_PAD_EIM_A22__GPIO_2_16,
	MX51_PAD_EIM_A23__GPIO_2_17,
	MX51_PAD_EIM_A27__GPIO_2_21,
	MX51_PAD_EIM_DTACK__GPIO_2_31,

	MX51_PAD_DI1_PIN11__GPIO_3_0,
	MX51_PAD_EIM_LBA__GPIO_3_1,
	MX51_PAD_DI1_D0_CS__GPIO_3_3,
	MX51_PAD_DISPB2_SER_DIN__GPIO_3_5,
	MX51_PAD_DISPB2_SER_DIO__GPIO_3_6,
	MX51_PAD_NANDF_CS0__GPIO_3_16,
	MX51_PAD_NANDF_CS1__GPIO_3_17,
	MX51_PAD_NANDF_D14__GPIO_3_26,
	MX51_PAD_NANDF_D12__GPIO_3_28,

	MX51_PAD_CSI2_D12__GPIO_4_9,
	MX51_PAD_CSI2_D13__GPIO_4_10,
	MX51_PAD_CSI2_D19__GPIO_4_12,
	MX51_PAD_CSI2_HSYNC__GPIO_4_14,
	MX51_PAD_CSPI1_RDY__GPIO_4_26,

	MX51_PAD_EIM_EB2__FEC_MDIO,
	MX51_PAD_EIM_EB3__FEC_RDAT1,
	MX51_PAD_EIM_CS2__FEC_RDAT2,
	MX51_PAD_EIM_CS3__FEC_RDAT3,
	MX51_PAD_EIM_CS4__FEC_RX_ER,
	MX51_PAD_EIM_CS5__FEC_CRS,
	MX51_PAD_NANDF_RB2__FEC_COL,
	MX51_PAD_NANDF_RB3__FEC_RXCLK,
	MX51_PAD_NANDF_RB6__FEC_RDAT0,
	MX51_PAD_NANDF_RB7__FEC_TDAT0,
	MX51_PAD_NANDF_CS2__FEC_TX_ER,
	MX51_PAD_NANDF_CS3__FEC_MDC,
	MX51_PAD_NANDF_CS4__FEC_TDAT1,
	MX51_PAD_NANDF_CS5__FEC_TDAT2,
	MX51_PAD_NANDF_CS6__FEC_TDAT3,
	MX51_PAD_NANDF_CS7__FEC_TX_EN,
	MX51_PAD_NANDF_RDY_INT__FEC_TX_CLK,

	MX51_PAD_GPIO_NAND__PATA_INTRQ,

	MX51_PAD_DI_GP4__DI2_PIN15,
	MX51_PAD_I2C1_CLK__HSI2C_CLK,
	MX51_PAD_I2C1_DAT__HSI2C_DAT,
	MX51_PAD_EIM_D16__I2C1_SDA,
	MX51_PAD_EIM_D19__I2C1_SCL,

	MX51_PAD_GPIO_1_2__PWM_PWMO,

	MX51_PAD_SD1_CMD__SD1_CMD,
	MX51_PAD_SD1_CLK__SD1_CLK,
	MX51_PAD_SD1_DATA0__SD1_DATA0,
	MX51_PAD_SD1_DATA1__SD1_DATA1,
	MX51_PAD_SD1_DATA2__SD1_DATA2,
	MX51_PAD_SD1_DATA3__SD1_DATA3,

	MX51_PAD_SD2_CMD__SD2_CMD,
	MX51_PAD_SD2_CLK__SD2_CLK,
	MX51_PAD_SD2_DATA0__SD2_DATA0,
	MX51_PAD_SD2_DATA1__SD2_DATA1,
	MX51_PAD_SD2_DATA2__SD2_DATA2,
	MX51_PAD_SD2_DATA3__SD2_DATA3,

	MX51_PAD_AUD3_BB_TXD__AUD3_BB_TXD,
	MX51_PAD_AUD3_BB_RXD__AUD3_BB_RXD,
	MX51_PAD_AUD3_BB_CK__AUD3_BB_CK,
	MX51_PAD_AUD3_BB_FS__AUD3_BB_FS,

	MX51_PAD_CSPI1_SS1__CSPI1_SS1,

	MX51_PAD_DI_GP3__CSI1_DATA_EN,
	MX51_PAD_CSI1_D10__CSI1_D10,
	MX51_PAD_CSI1_D11__CSI1_D11,
	MX51_PAD_CSI1_D12__CSI1_D12,
	MX51_PAD_CSI1_D13__CSI1_D13,
	MX51_PAD_CSI1_D14__CSI1_D14,
	MX51_PAD_CSI1_D15__CSI1_D15,
	MX51_PAD_CSI1_D16__CSI1_D16,
	MX51_PAD_CSI1_D17__CSI1_D17,
	MX51_PAD_CSI1_D18__CSI1_D18,
	MX51_PAD_CSI1_D19__CSI1_D19,
	MX51_PAD_CSI1_VSYNC__CSI1_VSYNC,
	MX51_PAD_CSI1_HSYNC__CSI1_HSYNC,

	MX51_PAD_OWIRE_LINE__SPDIF_OUT1,
};

/* working point(wp): 0 - 800MHz; 1 - 166.25MHz; */
static struct cpu_wp cpu_wp_auto[] = {
	{
	 .pll_rate = 1000000000,
	 .cpu_rate = 1000000000,
	 .pdf = 0,
	 .mfi = 10,
	 .mfd = 11,
	 .mfn = 5,
	 .cpu_podf = 0,
	 .cpu_voltage = 1175000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 800000000,
	 .pdf = 0,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 0,
	 .cpu_voltage = 1100000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 166250000,
	 .pdf = 4,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 4,
	 .cpu_voltage = 850000,},
};

static struct fb_videomode video_modes[] = {
	{
	 /* NTSC TV output */
	 "TV-NTSC", 60, 720, 480, 74074,
	 122, 15,
	 18, 26,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED,
	 0,},
	{
	 /* PAL TV output */
	 "TV-PAL", 50, 720, 576, 74074,
	 132, 11,
	 22, 26,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED | FB_VMODE_ODD_FLD_FIRST,
	 0,},
	{
	 /* 720p60 TV output */
	 "720P60", 60, 1280, 720, 13468,
	 260, 109,
	 25, 4,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 /*MITSUBISHI LVDS panel */
	 "XGA", 60, 1024, 768, 15385,
	 220, 40,
	 21, 7,
	 60, 10,
	 0,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 /* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	 "CLAA-WVGA", 57, 800, 480, 37037, 40, 60, 10, 10, 20, 10,
	 FB_SYNC_CLK_LAT_FALL,
	 FB_VMODE_NONINTERLACED,
	 0,},
};

struct cpu_wp *mx51_nitrogen_get_cpu_wp(int *wp)
{
	*wp = num_cpu_wp;
	return cpu_wp_auto;
}

void mx51_nitrogen_set_num_cpu_wp(int num)
{
	num_cpu_wp = num;
	return;
}

static struct mxc_w1_config mxc_w1_data = {
	.search_rom_accelerator = 1,
};

static u16 keymapping[16] = {
	KEY_UP, KEY_DOWN, KEY_MENU, KEY_BACK,
	KEY_RIGHT, KEY_LEFT, KEY_SELECT, KEY_ENTER,
	KEY_F1, KEY_F3, KEY_1, KEY_3,
	KEY_F2, KEY_F4, KEY_2, KEY_4,
};

static struct keypad_data keypad_plat_data = {
	.rowmax = 4,
	.colmax = 4,
	.learning = 0,
	.delay = 2,
	.matrix = keymapping,
};

#if defined(CONFIG_MXC_PWM) && defined(CONFIG_BACKLIGHT_PWM)
static struct platform_pwm_backlight_data mxc_pwm_backlight_data = {
	.pwm_id = 0,
	.max_brightness = 255,
	.dft_brightness = 128,
	.pwm_period_ns = 78770,
};
#endif

extern void mx5_ipu_reset(void);
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 2,
	.reset = mx5_ipu_reset,
};

extern void mx5_vpu_reset(void);
static struct mxc_vpu_platform_data mxc_vpu_data = {
	.reset = mx5_vpu_reset,
};

/* workaround for ecspi chipselect pin may not keep correct level when idle */
static void mx51_nitrogen_gpio_spi_chipselect_active(int cspi_mode, int status,
					     int chipselect)
{
	switch (cspi_mode) {
	case 1:
		switch (chipselect) {
		case 0x1:
			{
			iomux_v3_cfg_t cspi1_ss0 = MX51_PAD_CSPI1_SS0__CSPI1_SS0;

			mxc_iomux_v3_setup_pad(cspi1_ss0);
			break;
			}
		case 0x2:
			{
			iomux_v3_cfg_t cspi1_ss0_gpio = MX51_PAD_CSPI1_SS0__GPIO_4_24;

			mxc_iomux_v3_setup_pad(cspi1_ss0_gpio);
			gpio_set_value(GP_CSP1_SS0, 1 & (~status));
			break;
			}
		default:
			break;
		}
		break;
	case 2:
		break;
	case 3:
		break;
	default:
		break;
	}
}

static void mx51_nitrogen_gpio_spi_chipselect_inactive(int cspi_mode, int status,
					       int chipselect)
{
	switch (cspi_mode) {
	case 1:
		switch (chipselect) {
		case 0x1:
			break;
		case 0x2:
			break;

		default:
			break;
		}
		break;
	case 2:
		break;
	case 3:
		break;
	default:
		break;
	}
}

static struct mxc_spi_master mxcspi1_data = {
	.maxchipselect = 4,
	.spi_version = 23,
	.chipselect_active = mx51_nitrogen_gpio_spi_chipselect_active,
	.chipselect_inactive = mx51_nitrogen_gpio_spi_chipselect_inactive,
};

static struct imxi2c_platform_data mxci2c_data[] = {
	{
	.bitrate = 100000,
	}
,	{
	.bitrate = 100000,
	}
};

static struct resource mxci2c1_resources[] = {
	{
		.start = I2C1_BASE_ADDR,
		.end = I2C1_BASE_ADDR + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = MXC_INT_I2C1,
		.end = MXC_INT_I2C1,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource mxci2c2_resources[] = {
	{
		.start = I2C2_BASE_ADDR,
		.end = I2C2_BASE_ADDR + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = MXC_INT_I2C2,
		.end = MXC_INT_I2C2,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device i2c_devices[] = {
	{
		.name = "imx-i2c",
		.id = 0,
		.num_resources = ARRAY_SIZE(mxci2c1_resources),
		.resource = mxci2c1_resources,
	},
	{
		.name = "imx-i2c",
		.id = 1,
		.num_resources = ARRAY_SIZE(mxci2c2_resources),
		.resource = mxci2c2_resources,
	},
};

static iomux_v3_cfg_t hs_i2c_clk_pad_gp = MX51_PAD_I2C1_CLK__GPIO_4_16 ;
static iomux_v3_cfg_t hs_i2c_dat_pad_gp = MX51_PAD_I2C1_DAT__GPIO_4_17 ;
static iomux_v3_cfg_t hs_i2c_clk_pad_clk = MX51_PAD_I2C1_CLK__HSI2C_CLK ;
static iomux_v3_cfg_t hs_i2c_dat_pad_dat = MX51_PAD_I2C1_DAT__HSI2C_DAT ;

#define PRINT_SDA
/* Generate a pulse on the i2c clock pin. */
static void hs_i2c_clock_toggle(void)
{
	unsigned i;
	unsigned gp_clk = MAKE_GP(4, 16);
	unsigned gp_dat = MAKE_GP(4, 17);
	printk(KERN_INFO "%s\n", __FUNCTION__);
	gpio_direction_input(gp_clk);
	mxc_iomux_v3_setup_pad(hs_i2c_clk_pad_gp);

#ifdef PRINT_SDA
	gpio_direction_input(gp_dat);
	mxc_iomux_v3_setup_pad(hs_i2c_dat_pad_gp);
	printk(KERN_INFO "%s dat = %i\n", __FUNCTION__, gpio_get_value(gp_dat));
#endif
	/* Send high and low on the SCL line */
	for (i = 0; i < 9; i++) {
		gpio_direction_output(gp_clk,0);
		udelay(20);
		gpio_direction_input(gp_clk);
#ifdef PRINT_SDA
		printk(KERN_INFO "%s dat = %i\n", __FUNCTION__, gpio_get_value(gp_dat));
#endif
		udelay(20);
	}

        mxc_iomux_v3_setup_pad(hs_i2c_clk_pad_clk);
#ifdef PRINT_SDA
	mxc_iomux_v3_setup_pad(hs_i2c_dat_pad_dat);
#endif
}

static struct imxi2c_platform_data mxci2c_hs_data = {
	.bitrate = 100000,
	.i2c_clock_toggle = hs_i2c_clock_toggle,
};

static struct tve_platform_data tve_data = {
	.dac_reg = "VVIDEO",
};

static struct mxc_bus_freq_platform_data bus_freq_data = {
	.gp_reg_id = "SW1",
	.lp_reg_id = "SW2",
};

static struct mxc_dvfs_platform_data dvfs_core_data = {
	.reg_id = "SW1",
	.clk1_id = "cpu_clk",
	.clk2_id = "gpc_dvfs_clk",
	.gpc_cntr_offset = MXC_GPC_CNTR_OFFSET,
	.gpc_vcr_offset = MXC_GPC_VCR_OFFSET,
	.ccm_cdcr_offset = MXC_CCM_CDCR_OFFSET,
	.ccm_cacrr_offset = MXC_CCM_CACRR_OFFSET,
	.ccm_cdhipr_offset = MXC_CCM_CDHIPR_OFFSET,
	.prediv_mask = 0x1F800,
	.prediv_offset = 11,
	.prediv_val = 3,
	.div3ck_mask = 0xE0000000,
	.div3ck_offset = 29,
	.div3ck_val = 2,
	.emac_val = 0x08,
	.upthr_val = 25,
	.dnthr_val = 9,
	.pncthr_val = 33,
	.upcnt_val = 10,
	.dncnt_val = 10,
	.delay_time = 30,
};

static struct mxc_dvfsper_data dvfs_per_data = {
	.reg_id = "SW2",
	.clk_id = "gpc_dvfs_clk",
	.gpc_cntr_reg_addr = MXC_GPC_CNTR,
	.gpc_vcr_reg_addr = MXC_GPC_VCR,
	.gpc_adu = 0x0,
	.vai_mask = MXC_DVFSPMCR0_FSVAI_MASK,
	.vai_offset = MXC_DVFSPMCR0_FSVAI_OFFSET,
	.dvfs_enable_bit = MXC_DVFSPMCR0_DVFEN,
	.irq_mask = MXC_DVFSPMCR0_FSVAIM,
	.div3_offset = 0,
	.div3_mask = 0x7,
	.div3_div = 2,
	.lp_high = 1250000,
	.lp_low = 1250000,
};

static struct mxc_spdif_platform_data mxc_spdif_data = {
	.spdif_tx = 1,
	.spdif_rx = 0,
	.spdif_clk_44100 = 0,	/* spdif_ext_clk source for 44.1KHz */
	.spdif_clk_48000 = 7,	/* audio osc source */
	.spdif_clkid = 0,
	.spdif_clk = NULL,	/* spdif bus clk */
};

static struct resource mxcfb_resources[] = {
	[0] = {
	       .flags = IORESOURCE_MEM,
	       },
};

static struct mxc_fb_platform_data fb_data[] = {
	{
	 .interface_pix_fmt = IPU_PIX_FMT_RGB24,
	 .mode_str = "1024x768M-16@60",
	 .mode = video_modes,
	 .num_modes = ARRAY_SIZE(video_modes),
	 },
	{
	 .interface_pix_fmt = IPU_PIX_FMT_RGB565,
	 .mode_str = "CLAA-WVGA",
	 .mode = video_modes,
	 .num_modes = ARRAY_SIZE(video_modes),
	 },
};

static void mxc_iim_enable_fuse(void)
{
	u32 reg;

	if (!ccm_base)
		return;
	/* Enable fuse blown */
	reg = readl(ccm_base + 0x64);
	reg |= 0x10;
	writel(reg, ccm_base + 0x64);
}

static void mxc_iim_disable_fuse(void)
{
	u32 reg;

	/* Disable fuse blown */
	if (!ccm_base)
		return;

	reg = readl(ccm_base + 0x64);
	reg &= ~0x10;
	writel(reg, ccm_base + 0x64);
}

static struct mxc_iim_data iim_data = {
	.bank_start = MXC_IIM_MX51_BANK_START_ADDR,
	.bank_end   = MXC_IIM_MX51_BANK_END_ADDR,
	.enable_fuse = mxc_iim_enable_fuse,
	.disable_fuse = mxc_iim_disable_fuse,
};

extern int primary_di;
static int __init mxc_init_fb(void)
{
	/* DI0-LVDS */
	gpio_set_value(GP_LVDS_POWER_DOWN, 0);
	msleep(1);
	gpio_set_value(GP_LVDS_POWER_DOWN, 1);
	gpio_set_value(GP_LCD_3V3_ON, 1);
	gpio_set_value(GP_LCD_5V_ON, 1);

	/* WVGA Reset */
	gpio_set_value(GP_DISP_BRIGHTNESS_CTL, 1);

	if (primary_di) {
		printk(KERN_INFO "DI1 is primary\n");

		/* DI1 -> DP-BG channel: */
		mxc_fb_devices[1].num_resources = ARRAY_SIZE(mxcfb_resources);
		mxc_fb_devices[1].resource = mxcfb_resources;
		mxc_register_device(&mxc_fb_devices[1], &fb_data[1]);

		/* DI0 -> DC channel: */
		mxc_register_device(&mxc_fb_devices[0], &fb_data[0]);
	} else {
		printk(KERN_INFO "DI0 is primary\n");

		/* DI0 -> DP-BG channel: */
		mxc_fb_devices[0].num_resources = ARRAY_SIZE(mxcfb_resources);
		mxc_fb_devices[0].resource = mxcfb_resources;
		mxc_register_device(&mxc_fb_devices[0], &fb_data[0]);

		/* DI1 -> DC channel: */
		mxc_register_device(&mxc_fb_devices[1], &fb_data[1]);
	}

	/*
	 * DI0/1 DP-FG channel:
	 */
	mxc_register_device(&mxc_fb_devices[2], NULL);

	return 0;
}
device_initcall(mxc_init_fb);

#if 0
static int handle_edid(int *pixclk)
{
	int err = 0;
	int dvi = 0;
	int fb0 = 0;
	int fb1 = 1;
	struct fb_var_screeninfo screeninfo;
	struct i2c_adapter *adp;

	memset(&screeninfo, 0, sizeof(screeninfo));

	adp = i2c_get_adapter(1);

	if (cpu_rev(mx51, CHIP_REV_3_0) > 0) {
		gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSI2_HSYNC), 1);
		msleep(1);
	}
	err = read_edid(adp, &screeninfo, &dvi);
	if (cpu_rev(mx51, CHIP_REV_3_0) > 0)
		gpio_set_value(IOMUX_TO_GPIO(MX51_PIN_CSI2_HSYNC), 0);

	if (!err) {
		printk(KERN_INFO " EDID read\n");
		if (!dvi) {
			enable_vga = 1;
			fb0 = 1; /* fb0 will be VGA */
			fb1 = 0; /* fb1 will be DVI or TV */
		}

		/* Handle TV modes */
		/* This logic is fairly complex yet still doesn't handle all
		   possibilities.  Once a customer knows the platform
		   configuration, this should be simplified to what is desired.
		 */
		if (screeninfo.xres == 1920 && screeninfo.yres != 1200) {
			/* MX51 can't handle clock speeds for anything larger.*/
			if (!enable_tv)
				enable_tv = 1;
			if (enable_vga || enable_wvga || enable_tv == 2)
				enable_tv = 2;
			fb_data[0].mode = &(video_modes[0]);
			if (!enable_wvga)
				fb_data[1].mode_str = "800x600M-16@60";
		} else if (screeninfo.xres > 1280 && screeninfo.yres > 1024) {
			if (!enable_wvga) {
				fb_data[fb0].mode_str = "1280x1024M-16@60";
				fb_data[fb1].mode_str = NULL;
			} else {
				/* WVGA is preset so the DVI can't be > this. */
				fb_data[0].mode_str = "1024x768M-16@60";
			}
		} else if (screeninfo.xres > 0 && screeninfo.yres > 0) {
			if (!enable_wvga) {
				fb_data[fb0].mode =
					kzalloc(sizeof(struct fb_videomode),
							GFP_KERNEL);
				fb_var_to_videomode(fb_data[fb0].mode,
						    &screeninfo);
				fb_data[fb0].mode_str = NULL;
				if (screeninfo.xres >= 1280 &&
						screeninfo.yres > 720)
					fb_data[fb1].mode_str = NULL;
				else if (screeninfo.xres > 1024 &&
						screeninfo.yres > 768)
					fb_data[fb1].mode_str =
						"800x600M-16@60";
				else if (screeninfo.xres > 800 &&
						screeninfo.yres > 600)
					fb_data[fb1].mode_str =
						"1024x768M-16@60";
			} else {
				/* A WVGA panel was specified and an EDID was
				   read thus there is a DVI monitor attached. */
				if (screeninfo.xres >= 1024)
					fb_data[0].mode_str = "1024x768M-16@60";
				else if (screeninfo.xres >= 800)
					fb_data[0].mode_str = "800x600M-16@60";
				else
					fb_data[0].mode_str = "640x480M-16@60";
			}
		}
	}
	return 0;
}
#endif

#ifdef CONFIG_KEYBOARD_GPIO
static struct gpio_keys_button nitrogen_gpio_keys[] = {
	{
		.type	= EV_KEY,
		.code	= KEY_HOME,
		.gpio	= MAKE_GP(1, 3),
		.desc	= "Home Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.code	= KEY_BACK,
		.gpio	= MAKE_GP(1, 4),
		.desc	= "Back Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.code	= KEY_MENU,
		.gpio	= MAKE_GP(4, 30),
		.desc	= "Menu Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.code	= KEY_SEARCH,
		.gpio	= MAKE_GP(4, 31),
		.desc	= "Search Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
};

static struct gpio_keys_platform_data nitrogen_gpio_keys_platform_data = {
       .buttons        = nitrogen_gpio_keys,
       .nbuttons       = ARRAY_SIZE(nitrogen_gpio_keys),
};

static struct platform_device nitrogen_gpio_keys_device = {
       .name   = "gpio-keys",
       .id     = -1,
       .dev    = {
               .platform_data  = &nitrogen_gpio_keys_platform_data,
       },
};
#endif

#ifdef CONFIG_MAGSTRIPE_MODULE

static struct mag_platform_data nitrogen_mag_platform_data = {
	.front_pin = CONFIG_MAG_FRONT,
	.rear_pin = CONFIG_MAG_REAR,
	.clock_pin = CONFIG_MAG_CLOCK,
	.data_pin = CONFIG_MAG_DATA,
	.edge = CONFIG_MAG_RISING_EDGE,
	.timeout = CONFIG_MAG_TIMEOUT,
};

static struct platform_device magstripe_device = {
	.name	= "magstripe",
	.dev	= {
		.platform_data = &nitrogen_mag_platform_data
	},
};
#endif


struct plat_i2c_generic_data {
	unsigned irq;
	unsigned gp;
};

static struct plat_i2c_generic_data i2c_generic_data = {
	.irq = IOMUX_TO_IRQ_V3(MAKE_GP(4, 26)), .gp = MAKE_GP(4, 26) /* CSPI1_RDY Nitrogen-E */
};

struct plat_i2c_tfp410_data {
	int irq;
	int gp;
	int gp_i2c_sel;
};

static struct plat_i2c_tfp410_data i2c_tfp410_data = {
	.irq = IOMUX_TO_IRQ_V3(MAKE_GP(3, 28)), .gp = MAKE_GP(3, 28),
	.gp_i2c_sel = MAKE_GP(3, 5)
};

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	 },
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
};

static struct i2c_board_info mxc_i2c_hs_board_info[] __initdata = {
	{
	 .type = "Pic16F616-ts",
	 .addr = 0x22,
	 .platform_data  = &i2c_generic_data,
	},
	{
	 .type = "mma7660",
	 .addr = 0x4c,
	 .platform_data  = &i2c_generic_data,
	},
	{
	 .type = "tfp410",
	 .addr = 0x38,
	 .platform_data  = &i2c_tfp410_data,
	}
};

static struct mtd_partition mxc_spi_nor_partitions[] = {
	{
	 .name = "bootloader",
	 .offset = 0,
	 .size = 0x00040000,},
	{
	 .name = "kernel",
	 .offset = MTDPART_OFS_APPEND,
	 .size = MTDPART_SIZ_FULL,},

};

static struct mtd_partition mxc_dataflash_partitions[] = {
	{
	 .name = "bootloader",
	 .offset = 0,
	 .size = 1024 * 528,},
	{
	 .name = "kernel",
	 .offset = MTDPART_OFS_APPEND,
	 .size = MTDPART_SIZ_FULL,},
};

static struct flash_platform_data mxc_spi_flash_data[] = {
	{
	 .name = "mxc_spi_nor",
	 .parts = mxc_spi_nor_partitions,
	 .nr_parts = ARRAY_SIZE(mxc_spi_nor_partitions),
	 .type = "sst25vf016b",},
	{
	 .name = "mxc_dataflash",
	 .parts = mxc_dataflash_partitions,
	 .nr_parts = ARRAY_SIZE(mxc_dataflash_partitions),
	 .type = "at45db321d",}
};

static struct spi_board_info mxc_spi_nor_device[] __initdata = {
	{
	 .modalias = "mxc_spi_nor",
	 .max_speed_hz = 25000000,	/* max spi clock (SCK) speed in HZ */
	 .bus_num = 1,
	 .chip_select = 1,
	 .platform_data = &mxc_spi_flash_data[0],
	},
};

static struct spi_board_info mxc_dataflash_device[] __initdata = {
	{
	 .modalias = "mxc_dataflash",
	 .max_speed_hz = 25000000,	/* max spi clock (SCK) speed in HZ */
	 .bus_num = 1,
	 .chip_select = 1,
	 .platform_data = &mxc_spi_flash_data[1],},
};

/*
 * Out: 0 - card not write protected, 1 - card write protected
 */
static int sdhc_write_protect(struct device *dev)
{
	int ret;

	if (to_platform_device(dev)->id == 0)
		ret = gpio_get_value(GP_SD1_WP);
	else {
//		rc = gpio_get_value(GP_SD2_WP);
		ret = 0;
	}
	return ret;
}

/*
 * Out: 0 - card present, 1 - card not present
 */
static unsigned int sdhc_get_card_det_status(struct device *dev)
{
	int ret;

	if (to_platform_device(dev)->id == 0) {
		ret = gpio_get_value(GP_SD1_CD);
	} else {		/* config the det pin for SDHC2 */
		/* BB2.5 */
//		ret = gpio_get_value(GP_SD2_CD);
		ret = 0;
	}
	return ret;
}

static struct mxc_mmc_platform_data mmc1_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30 |
	    MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 150000,
	.max_clk = 52000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
	.power_mmc = NULL,
};

static struct mxc_mmc_platform_data mmc2_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30 |
	    MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 150000,
	.max_clk = 50000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
};

static int mxc_sgtl5000_amp_enable(int enable)
{
	gpio_set_value(GP_AUDAMP_STBY, enable ? 1 : 0);
	return 0;
}

static int headphone_det_status(void)
{
	return 0 ;
}

static struct mxc_audio_platform_data sgtl5000_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.hp_status = headphone_det_status,
	.amp_enable = mxc_sgtl5000_amp_enable,
	.sysclk = 26000000,	//12288000,
};

static struct platform_device mxc_sgtl5000_device = {
	.name = "imx-3stack-sgtl5000",
};

static int __initdata enable_w1 = { 0 };
static int __init w1_setup(char *__unused)
{
	enable_w1 = 1;
	return cpu_is_mx51();
}

__setup("w1", w1_setup);

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data android_pmem_data = {
	.name = "pmem_adsp",
	.size = SZ_32M,
};

static struct android_pmem_platform_data android_pmem_gpu_data = {
	.name = "pmem_gpu",
	.size = SZ_32M,
	.cached = 1,
};
#endif

static char *usb_functions_ums[] = {
	"usb_mass_storage",
};

static char *usb_functions_ums_adb[] = {
	"usb_mass_storage",
	"adb",
};

static char *usb_functions_rndis[] = {
	"rndis",
};

static char *usb_functions_rndis_adb[] = {
	"rndis",
	"adb",
};

static char *usb_functions_all[] = {
	"rndis",
	"usb_mass_storage",
	"adb"
};

static struct android_usb_product usb_products[] = {
	{
		.product_id	= 0x0c01,
		.num_functions	= ARRAY_SIZE(usb_functions_ums),
		.functions	= usb_functions_ums,
	},
	{
		.product_id	= 0x0c02,
		.num_functions	= ARRAY_SIZE(usb_functions_ums_adb),
		.functions	= usb_functions_ums_adb,
	},
	{
		.product_id	= 0x0c03,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis),
		.functions	= usb_functions_rndis,
	},
	{
		.product_id	= 0x0c04,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis_adb),
		.functions	= usb_functions_rndis_adb,
	},
};

static struct usb_mass_storage_platform_data mass_storage_data = {
	.nluns		= 3,
	.vendor		= "Freescale",
	.product	= "Android Phone",
	.release	= 0x0100,
};

static struct usb_ether_platform_data rndis_data = {
	.vendorID	= 0x0bb4,
	.vendorDescr	= "Freescale",
};

static struct android_usb_platform_data android_usb_data = {
	.vendor_id      = 0x0bb4,
	.product_id     = 0x0c01,
	.version        = 0x0100,
	.product_name   = "Android Phone",
	.manufacturer_name = "Freescale",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
};

#define MAX_CAMERA_FRAME_SIZE (((2592*1944*2+PAGE_SIZE-1)/PAGE_SIZE)*PAGE_SIZE)
#define MAX_CAMERA_FRAME_COUNT 4
#define MAX_CAMERA_MEM SZ_64M
//#define MAX_CAMERA_MEM ((MAX_CAMERA_FRAME_SIZE)*(MAX_CAMERA_FRAME_COUNT))

#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)

static unsigned long camera_buf_phys = 0UL ;
unsigned long get_camera_phys(unsigned maxsize) {
	if (maxsize <= MAX_CAMERA_MEM)
		return camera_buf_phys ;
	else
		return 0UL ;
}
EXPORT_SYMBOL(get_camera_phys);

#endif

/*!
 * Board specific fixup function. It is called by \b setup_arch() in
 * setup.c file very early on during kernel starts. It allows the user to
 * statically fill in the proper values for the passed-in parameters. None of
 * the parameters is used currently.
 *
 * @param  desc         pointer to \b struct \b machine_desc
 * @param  tags         pointer to \b struct \b tag
 * @param  cmdline      pointer to the command line
 * @param  mi           pointer to \b struct \b meminfo
 */
static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	char *str;
	struct tag *t;
	struct tag *mem_tag = 0;
	int total_mem = SZ_512M;
	int temp_mem = 0;
#if defined(CONFIG_MXC_AMD_GPU) || defined(CONFIG_MXC_AMD_GPU_MODULE)
	int gpu_mem = SZ_16M ;
#endif

	mxc_set_cpu_type(MXC_CPU_MX51);
	get_cpu_wp = mx51_nitrogen_get_cpu_wp;
	set_num_cpu_wp = mx51_nitrogen_set_num_cpu_wp;

	for_each_tag(t, tags) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			str = t->u.cmdline.cmdline;
			str = strstr(str, "mem=");
			if (str != NULL) {
				str += 4;
				temp_mem = memparse(str, &str);
			}

#if defined(CONFIG_MXC_AMD_GPU) || defined(CONFIG_MXC_AMD_GPU_MODULE)
			str = t->u.cmdline.cmdline;
			str = strstr(str, "gpu_memory=");
			if (str != NULL) {
				str += 11;
				gpu_mem = memparse(str, &str);
			}
#endif
			break;
		}
	}

	for_each_tag(mem_tag, tags) {
		if (mem_tag->hdr.tag == ATAG_MEM) {
			total_mem = mem_tag->u.mem.size;
			break;
		}
	}

	if (temp_mem > 0 && temp_mem < total_mem)
		total_mem = temp_mem;

	if (mem_tag) {
#if defined(CONFIG_MXC_AMD_GPU) || defined(CONFIG_MXC_AMD_GPU_MODULE)
		/*reserve memory for gpu*/
		gpu_device.resource[5].end = mem_tag->u.mem.start + total_mem - 1 ;
		gpu_device.resource[5].start = mem_tag->u.mem.start + total_mem - gpu_mem ;
		total_mem -= gpu_mem ;
#endif

#ifdef CONFIG_ANDROID_PMEM
		android_pmem_data.start = mem_tag->u.mem.start + total_mem - android_pmem_data.size ;
		total_mem -= android_pmem_data.size ;
		android_pmem_gpu_data.start = mem_tag->u.mem.start + total_mem - android_pmem_gpu_data.size ;
		total_mem -= android_pmem_gpu_data.size ;
#endif
#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
		camera_buf_phys = mem_tag->u.mem.start + total_mem - MAX_CAMERA_MEM ;
		total_mem -= MAX_CAMERA_MEM ;
		printk (KERN_INFO "0x%x bytes of camera mem at 0x%lx\n", MAX_CAMERA_MEM, camera_buf_phys);
#endif
		mem_tag->u.mem.size = total_mem ;
	}
#ifdef CONFIG_DEBUG_LL
	mx5_map_uart();
#endif
}

#define PWGT1SPIEN (1<<15)
#define PWGT2SPIEN (1<<16)
#define USEROFFSPI (1<<3)

static void mxc_power_off(void)
{
	/* We can do power down one of two ways:
	   Set the power gating
	   Set USEROFFSPI */

	/* Set the power gate bits to power down */
	pmic_write_reg(REG_POWER_MISC, (PWGT1SPIEN|PWGT2SPIEN),
		(PWGT1SPIEN|PWGT2SPIEN));
}

/*!
 * Power Key interrupt handler.
 */
static irqreturn_t power_key_int(int irq, void *dev_id)
{
	pwrkey_callback cb = (pwrkey_callback)dev_id;

	cb((void *)1);

	if (gpio_get_value(GP_POWER_KEY))
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	else
		set_irq_type(irq, IRQF_TRIGGER_RISING);

	return 0;
}

static void mxc_register_powerkey(pwrkey_callback pk_cb)
{
	/* Set power key as wakeup resource */
	int irq, ret;
	irq = IOMUX_TO_IRQ_V3(GP_POWER_KEY);

	if (gpio_get_value(GP_POWER_KEY))
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
	else
		set_irq_type(irq, IRQF_TRIGGER_RISING);

	ret = request_irq(irq, power_key_int, 0, "power_key", pk_cb);
	if (ret)
		pr_info("register on-off key interrupt failed\n");
	else
		enable_irq_wake(irq);
}

static int mxc_pwrkey_getstatus(int id)
{
	return gpio_get_value(GP_POWER_KEY);
}

static struct power_key_platform_data pwrkey_data = {
	.key_value = KEY_F4,
	.register_pwrkey = mxc_register_powerkey,
	.get_key_status = mxc_pwrkey_getstatus,
};

#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
static struct platform_device boundary_camera_device = {
	.name = "boundary_camera",
};

static struct platform_device boundary_camera_interfaces[] = {
#ifdef CONFIG_BOUNDARY_CAMERA_CSI0
	{ .name = "boundary_camera_csi0", },
#endif
#ifdef CONFIG_BOUNDARY_CAMERA_CSI1
	{ .name = "boundary_camera_csi1", },
#endif
};

static struct mxc_camera_platform_data camera_data = {
	.io_regulator = "VDD_IO",
	.analog_regulator = "VDD_A",
	.mclk = 26000000,
	.csi = 0,
	.power_down = CAMERA_POWERDOWN,
	.reset = CAMERA_RESET,
	.i2c_bus = 1,
	.i2c_id = 0x3c,
	.sensor_name = "ov5640",
};

static void init_camera(void)
{
	struct clk *clk ;
	int i ;
	clk = clk_get(NULL,"csi_mclk1");
	if(clk){
		clk_set_rate(clk,24000000);
	} else
		printk(KERN_ERR "%s: Error getting CSI clock\n", __func__ );

	mxc_register_device(&boundary_camera_device, &camera_data);
	for (i = 0 ; i < ARRAY_SIZE(boundary_camera_interfaces); i++ ){
		mxc_register_device(&boundary_camera_interfaces[i], &camera_data);
	}
}
#endif

static void __init mx51_nitrogen_io_init(void)
{
	/*
	 * Configure gpio direction/value before
	 * configuring the muxc to avoid glitches
	 */
	nitrogen_gpios[0].gpio = i2c_generic_data.gp;
	if (gpio_request_array(nitrogen_gpios, ARRAY_SIZE(nitrogen_gpios))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	gpio_free(MAKE_GP(4, 30));
	gpio_free(MAKE_GP(4, 31));

	mxc_iomux_v3_setup_multiple_pads(mx51nitrogen_pads,
			ARRAY_SIZE(mx51nitrogen_pads));
	if (i2c_generic_data.gp == MAKE_GP(1, 22)) {
		iomux_v3_cfg_t pd = MX51_PAD_UART3_RXD__GPIO_1_22;
		mxc_iomux_v3_setup_pad(pd);
	}

	/* release usbh1 hub reset */
	msleep(1);
	gpio_set_value(GP_USBH1_HUB_RST, 1);

	/* release FEC PHY reset */
	msleep(9);
	gpio_set_value(GP_FEC_PHY_RESET, 1);

	/* release FM reset */
	gpio_set_value(GP_FM_RESET, 1);

	if (enable_w1) {
		/* OneWire */
		iomux_v3_cfg_t onewire = MX51_PAD_OWIRE_LINE__OWIRE_LINE;
		mxc_iomux_v3_setup_pad(onewire);
	}
#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
	init_camera();
#endif
}

/*!
 * Board specific initialization.
 */
static void __init mxc_board_init(void)
{
	mxc_ipu_data.di_clk[0] = clk_get(NULL, "ipu_di0_clk");
	mxc_ipu_data.di_clk[1] = clk_get(NULL, "ipu_di1_clk");
	mxc_ipu_data.csi_clk[0] = clk_get(NULL, "csi_mclk1");
	mxc_ipu_data.csi_clk[1] = clk_get(NULL, "csi_mclk2");

	mxc_spdif_data.spdif_core_clk = clk_get(NULL, "spdif_xtal_clk");
	clk_put(mxc_spdif_data.spdif_core_clk);

	/* SD card detect irqs */
	mxcsdhc2_device.resource[2].start = IOMUX_TO_IRQ_V3(MAKE_GP(1, 6));
	mxcsdhc2_device.resource[2].end = IOMUX_TO_IRQ_V3(MAKE_GP(1, 6));
	mxcsdhc1_device.resource[2].start = IOMUX_TO_IRQ_V3(GP_SD1_CD);
	mxcsdhc1_device.resource[2].end = IOMUX_TO_IRQ_V3(GP_SD1_CD);

	mxc_cpu_common_init();
	mx51_nitrogen_io_init();

	mxc_register_device(&mxcspi1_device, &mxcspi1_data);
	mx51_nitrogen_init_mc13892();

	mxc_register_device(&mxc_dma_device, NULL);
	mxc_register_device(&mxc_wdt_device, NULL);
	mxc_register_device(&i2c_devices[0], &mxci2c_data[0]);
	mxc_register_device(&i2c_devices[1], &mxci2c_data[1]);
	mxc_register_device(&mxci2c_hs_device, &mxci2c_hs_data);
	mxc_register_device(&mxc_rtc_device, NULL);
	mxc_register_device(&mxc_w1_master_device, &mxc_w1_data);
	mxc_register_device(&mxc_ipu_device, &mxc_ipu_data);
	mxc_register_device(&mxc_tve_device, &tve_data);
	mxc_register_device(&mxcvpu_device, &mxc_vpu_data);
	mxc_register_device(&gpu_device, NULL);
	mxc_register_device(&mxcscc_device, NULL);
	mxc_register_device(&mx51_lpmode_device, NULL);
	mxc_register_device(&busfreq_device, &bus_freq_data);
	mxc_register_device(&sdram_autogating_device, NULL);
	mxc_register_device(&mxc_dvfs_core_device, &dvfs_core_data);
	mxc_register_device(&mxc_dvfs_per_device, &dvfs_per_data);
	mxc_register_device(&mxc_iim_device, &iim_data);
	mxc_register_device(&mxc_pwm1_device, NULL);
#if defined(CONFIG_MXC_PWM) && defined(CONFIG_BACKLIGHT_PWM)
	mxc_register_device(&mxc_pwm1_backlight_device,&mxc_pwm_backlight_data);
#endif
	mxc_register_device(&mxc_keypad_device, &keypad_plat_data);
	mxc_register_device(&mxcsdhc1_device, &mmc1_data);
	mxc_register_device(&mxcsdhc2_device, &mmc2_data);
	mxc_register_device(&mxc_ssi1_device, NULL);
	mxc_register_device(&mxc_ssi2_device, NULL);
	mxc_register_device(&mxc_ssi3_device, NULL);
	mxc_register_device(&mxc_alsa_spdif_device, &mxc_spdif_data);
	mxc_register_device(&mxc_fec_device, NULL);
	mxc_register_device(&mxc_v4l2_device, NULL);
	mxc_register_device(&mxc_v4l2out_device, NULL);
	mxc_register_device(&mxc_powerkey_device, &pwrkey_data);

#ifdef CONFIG_ANDROID_PMEM
	mxc_register_device(&mxc_android_pmem_device, &android_pmem_data);
	mxc_register_device(&mxc_android_pmem_gpu_device, &android_pmem_gpu_data);
#endif
	mxc_register_device(&usb_mass_storage_device, &mass_storage_data);
	mxc_register_device(&usb_rndis_device, &rndis_data);
	mxc_register_device(&android_usb_device, &android_usb_data);

	if (board_is_rev(BOARD_REV_2))
		/* BB2.5 */
		spi_register_board_info(mxc_dataflash_device,
					ARRAY_SIZE(mxc_dataflash_device));
	else
		/* BB2.0 */
		spi_register_board_info(mxc_spi_nor_device,
					ARRAY_SIZE(mxc_spi_nor_device));

	if (!machine_is_nitrogen_p_imx51()) {
		i2c_register_board_info(0, mxc_i2c0_board_info,
				ARRAY_SIZE(mxc_i2c0_board_info));
		i2c_register_board_info(1, mxc_i2c1_board_info,
				ARRAY_SIZE(mxc_i2c1_board_info));
		i2c_register_board_info(3, mxc_i2c_hs_board_info,
				ARRAY_SIZE(mxc_i2c_hs_board_info));
	}

	pm_power_off = mxc_power_off;

	if (cpu_rev(mx51, CHIP_REV_1_1) == 2) {
		sgtl5000_data.sysclk = 26000000;
	}
	mxc_register_device(&mxc_sgtl5000_device, &sgtl5000_data);

	mx5_usb_dr_init();
	mx5_usbh1_init();


#ifdef CONFIG_KEYBOARD_GPIO
	platform_device_register(&nitrogen_gpio_keys_device);
#endif

#ifdef CONFIG_MAGSTRIPE_MODULE
	platform_device_register(&magstripe_device);
#endif
	dont_sleep_yet = 0 ;
}

static void __init mx51_nitrogen_timer_init(void)
{
	struct clk *uart_clk;
printk (KERN_ERR "%s\n", __func__ );
	/* Change the CPU voltages for TO2*/
	if (cpu_rev(mx51, CHIP_REV_2_0) <= 1) {
		cpu_wp_auto[0].cpu_voltage = 1175000;
		cpu_wp_auto[1].cpu_voltage = 1100000;
		cpu_wp_auto[2].cpu_voltage = 1000000;
	}

printk (KERN_ERR "%s: clocks init\n", __func__ );
	mx51_clocks_init(32768, 24000000, 22579200, 24576000);

	uart_clk = clk_get_sys("mxcintuart.0", NULL);
printk (KERN_ERR "%s: setup console 0x%08x\n", __func__,UART1_BASE_ADDR );
	early_console_setup(UART1_BASE_ADDR, uart_clk);
}

static struct sys_timer mxc_timer = {
	.init	= mx51_nitrogen_timer_init,
};

/*
 * The following uses standard kernel macros define in arch.h in order to
 * initialize __mach_desc_NITROGEN_IMX51 data structure.
 */
/* *INDENT-OFF* */
#ifdef CONFIG_MACH_NITROGEN_IMX51
MACHINE_START(NITROGEN_IMX51, "Boundary Devices Nitrogen E MX51 Board")
	/* Maintainer: Boundary Devices */
	.phys_io	= AIPS1_BASE_ADDR,
	.io_pg_offst	= ((AIPS1_BASE_ADDR_VIRT) >> 18) & 0xfffc,
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine = mxc_board_init,
	.timer = &mxc_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_NITROGEN_VM_IMX51
static void __init mxc_board_init_nitrogen_vm(void)
{
	/* NITROGEN_VM */
	i2c_generic_data.irq = IOMUX_TO_IRQ_V3(MAKE_GP(2, 1));	/* EIM_D17 Nitrogen-VM */
	i2c_generic_data.gp = MAKE_GP(2, 1);
	mxc_board_init();
}

/* NITROGEN_VM - setenv machid C61 */
MACHINE_START(NITROGEN_VM_IMX51, "Boundary Devices Nitrogen VM MX51 Board")
	/* Maintainer: Boundary Devices */
	.phys_io	= AIPS1_BASE_ADDR,
	.io_pg_offst	= ((AIPS1_BASE_ADDR_VIRT) >> 18) & 0xfffc,
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine = mxc_board_init_nitrogen_vm,
	.timer = &mxc_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_NITROGEN_P_IMX51
static struct i2c_board_info mxc_i2c0_board_info_nitrogen_p[] __initdata = {
	{
	 .type = "Pic16F616-ts",
	 .addr = 0x22,
	 .platform_data  = &i2c_generic_data,
	},
};

static struct i2c_board_info mxc_i2c1_board_info_nitrogen_p[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	 },
};

static struct i2c_board_info mxc_i2c_hs_board_info_nitrogen_p[] __initdata = {
	{
	 .type = "mma7660",
	 .addr = 0x4c,
	 .platform_data  = &i2c_generic_data,
	},
	{
	 .type = "tfp410",
	 .addr = 0x38,
	 .platform_data  = &i2c_tfp410_data,
	}
};

struct gpio extra_gpios[] __initdata = {
	{.label="usb-clk_en_b",		.gpio = MAKE_GP(2, 1),			.flags = GPIOF_INIT_HIGH},
	{.label="audio-clk-en",		.gpio = MAKE_GP(4, 26),			.flags = 0},
};

static void __init mxc_board_init_nitrogen_p(void)
{
	/* NITROGEN_P keys */
#ifdef CONFIG_KEYBOARD_GPIO
	nitrogen_gpio_keys[0].gpio = MAKE_GP(4, 31);
	nitrogen_gpio_keys[1].gpio = MAKE_GP(1, 4);
	nitrogen_gpio_keys[2].gpio = MAKE_GP(4, 30);
	nitrogen_gpio_keys[3].gpio = MAKE_GP(1, 3);
#endif
	mxc_board_init();
	if (gpio_request_array(extra_gpios, ARRAY_SIZE(extra_gpios))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	i2c_register_board_info(0, mxc_i2c0_board_info_nitrogen_p,
			ARRAY_SIZE(mxc_i2c0_board_info_nitrogen_p));
	i2c_register_board_info(1, mxc_i2c1_board_info_nitrogen_p,
			ARRAY_SIZE(mxc_i2c1_board_info_nitrogen_p));
	i2c_register_board_info(3, mxc_i2c_hs_board_info_nitrogen_p,
			ARRAY_SIZE(mxc_i2c_hs_board_info_nitrogen_p));
}

/* NITROGEN_P - setenv machid C62 */
MACHINE_START(NITROGEN_P_IMX51, "Boundary Devices Nitrogen P MX51 Board")
	/* Maintainer: Boundary Devices */
	.phys_io	= AIPS1_BASE_ADDR,
	.io_pg_offst	= ((AIPS1_BASE_ADDR_VIRT) >> 18) & 0xfffc,
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine = mxc_board_init_nitrogen_p,
	.timer = &mxc_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_NITROGEN_EJ_IMX51
static void __init mxc_board_init_nitrogen_ej(void)
{
	/* NITROGEN_EJ(jumpered) */
	i2c_generic_data.irq = IOMUX_TO_IRQ_V3(MAKE_GP(1, 22));	/* ttymxc2 rxd Nitrogen-EJ(jumpered) */
	i2c_generic_data.gp = MAKE_GP(1, 22);
	mxc_board_init();
}

/* NITROGEN_EJ - setenv machid C63 */
MACHINE_START(NITROGEN_EJ_IMX51, "Boundary Devices Nitrogen EJ MX51 Board")
	/* Maintainer: Boundary Devices */
	.phys_io	= AIPS1_BASE_ADDR,
	.io_pg_offst	= ((AIPS1_BASE_ADDR_VIRT) >> 18) & 0xfffc,
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine = mxc_board_init_nitrogen_ej,
	.timer = &mxc_timer,
MACHINE_END
#endif
