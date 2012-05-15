/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/ata.h>
#include <linux/clk.h>
#include <linux/delay.h>
#if defined(CONFIG_DUMB_BATTERY) || defined (CONFIG_DUMB_BATTERY_MODULE)
#include <linux/dumb_battery.h>
#endif
#include <linux/fec.h>
#include <linux/fsl_devices.h>
#include <linux/gpio.h>
#include <linux/gpio-i2cmux.h>
#ifdef CONFIG_KEYBOARD_GPIO
#include <linux/gpio_keys.h>
#endif
#include <linux/i2c.h>
#include <linux/i2c/bq2416x.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/ipu.h>
#include <linux/irq.h>
#include <linux/ldb.h>
#include <linux/mfd/da9052/da9052.h>
#include <linux/mfd/sc16is7xx-reg.h>
#if defined(CONFIG_MTD)
#include <linux/mtd/map.h>
#endif
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mxcfb.h>
#include <linux/nodemask.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/powerkey.h>
#include <linux/pwm_backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/sched.h>
#include <linux/spi/flash.h>
#include <linux/spi/ltc1960.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/usb/android_composite.h>
#include <linux/usb/f_accessory.h>
#include <linux/wl12xx.h>
#include <linux/android_pmem.h>
#include <linux/ektf2k.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/i2c.h>
#include <mach/imx-uart.h>
#include <mach/iomux-mx53.h>
#include <mach/memory.h>
#include <mach/mmc.h>
#include <mach/mxc_dvfs.h>
#include <mach/mxc_iim.h>

#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/keypad.h>
#include <asm/mach/time.h>
#include <asm/mach-types.h>
#include <asm/setup.h>

#include "crm_regs.h"
#include "devices.h"
#include "usb.h"

#if defined (CONFIG_TOUCHSCREEN_ATMEL_MXT) || defined (CONFIG_TOUCHSCREEN_ATMEL_MXT_MODULE)
#include <linux/i2c/atmel_mxt_ts.h>
#endif

#if defined (CONFIG_BOUNDARY_CAMERA_OV5640) || defined (CONFIG_BOUNDARY_CAMERA_OV5640_MODULE) \
 || defined (CONFIG_VIDEO_MXC_CAMERA)       || defined (CONFIG_VIDEO_MXC_CAMERA_MODULE)
#define HAVE_CAMERA
#endif

//#define REV0		//this board should no longer exist
#define BUTTON_PAD_CTRL	(PAD_CTL_HYS | PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_22K_UP | PAD_CTL_DSE_HIGH)
#define BUTTON_100K	(PAD_CTL_HYS | PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP | PAD_CTL_DSE_LOW)

#define MUX_SION_MASK	((iomux_v3_cfg_t)(IOMUX_CONFIG_SION) << MUX_MODE_SHIFT)
#define PAD_CTRL_1	(PAD_CTL_HYS | PAD_CTL_DSE_HIGH)
#define PAD_CTRL_4	(PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_DSE_HIGH | PAD_CTL_HYS | PAD_CTL_PUS_100K_UP)
#define PAD_CTRL_5	(PAD_CTL_DSE_HIGH | PAD_CTL_SRE_FAST)
#define PAD_CTRL_7	(PAD_CTL_DSE_HIGH | PAD_CTL_SRE_SLOW)
#define PAD_CTRL_8	(PAD_CTL_HYS | PAD_CTL_PKE)
#define PAD_CTRL_9 	(PAD_CTL_SRE_FAST | PAD_CTL_ODE | PAD_CTL_DSE_HIGH | PAD_CTL_PUS_100K_UP | PAD_CTL_HYS)
#define PAD_CTRL_12	(PAD_CTL_DSE_HIGH | PAD_CTL_PKE | PAD_CTL_SRE_FAST)
#define NEW_PAD_CTRL(cfg, pad)	(((cfg) & ~MUX_PAD_CTRL_MASK) | MUX_PAD_CTRL(pad))
#define MX53_I2C1_PAD_CTRL	(PAD_CTL_HYS | PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_22K_UP | PAD_CTL_ODE | PAD_CTL_DSE_HIGH | PAD_CTL_SRE_FAST)

#define MX53_PAD_CSI0_D4__AUD3_TXC		IOMUX_PAD(0x3FC, 0xD0, 5, 0x0, 0, PAD_CTRL_4)
#define MX53_PAD_ATA_BUFFER_EN__UART2_RXD	IOMUX_PAD(0x5FC, 0x27C, 3, 0x880, 3, MX53_UART_PAD_CTRL)
#define MX53_PAD_ATA_CS_0__GPIO_7_9     IOMUX_PAD(0x61C, 0x29C, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_ATA_CS_0__UART3_TXD		IOMUX_PAD(0x61C, 0x29C, 4, 0x0, 0, MX53_UART_PAD_CTRL)
#define MX53_PAD_ATA_CS_1__UART3_RXD		IOMUX_PAD(0x620, 0x2A0, 4, 0x888, 3, MX53_UART_PAD_CTRL)
#define MX53_PAD_ATA_DA_1__SD4_CMD		IOMUX_PAD(0x614, 0x294, 2, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DA_2__SD4_CLK		IOMUX_PAD(0x618, 0x298, 2, 0x0, 0, MX53_SDHC_PAD_CTRL | PAD_CTL_HYS)
#define MX53_PAD_ATA_DATA12__SD4_DAT0		IOMUX_PAD(0x658, 0x2D4, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA13__SD4_DAT1		IOMUX_PAD(0x65C, 0x2D8, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA14__SD4_DAT2		IOMUX_PAD(0x660, 0x2DC, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA15__SD4_DAT3		IOMUX_PAD(0x664, 0x2E0, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA0__SD3_DAT4		IOMUX_PAD(0x628, 0x2A4, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA10__SD3_DAT2		IOMUX_PAD(0x650, 0x2CC, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA11__SD3_DAT3		IOMUX_PAD(0x654, 0x2D0, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA1__SD3_DAT5		IOMUX_PAD(0x62C, 0x2A8, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA2__SD3_DAT6		IOMUX_PAD(0x630, 0x2AC, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA3__SD3_DAT7		IOMUX_PAD(0x634, 0x2B0, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA4__SD4_DAT4		IOMUX_PAD(0x638, 0x2B4, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA5__SD4_DAT5		IOMUX_PAD(0x63C, 0x2B8, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA6__SD4_DAT6		IOMUX_PAD(0x640, 0x2BC, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA7__SD4_DAT7		IOMUX_PAD(0x644, 0x2C0, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA8__SD3_DAT0		IOMUX_PAD(0x648, 0x2C4, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DATA9__SD3_DAT1		IOMUX_PAD(0x64C, 0x2C8, 4, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_ATA_DIOR__UART2_RTS		IOMUX_PAD(0x604, 0x284, 3, 0x87C, 3, MX53_UART_PAD_CTRL)
#define MX53_PAD_ATA_DIOW__UART1_TXD		IOMUX_PAD(0x5F0, 0x270, 3, 0x0, 0, MX53_UART_PAD_CTRL)
#define MX53_PAD_ATA_DMACK__UART1_RXD		IOMUX_PAD(0x5F4, 0x274, 3, 0x880, 3, MX53_UART_PAD_CTRL)
#define MX53_PAD_ATA_DMARQ__UART2_TXD		IOMUX_PAD(0x5F8, 0x278, 3, 0x0, 0, MX53_UART_PAD_CTRL)
#define MX53_PAD_ATA_INTRQ__UART2_CTS		IOMUX_PAD(0x600, 0x280, 3, 0x0, 0, MX53_UART_PAD_CTRL)
#define MX53_PAD_ATA_IORDY__SD3_CLK		IOMUX_PAD(0x60C, 0x28C, 2, 0x0, 0, MX53_SDHC_PAD_CTRL | PAD_CTL_HYS)
#define MX53_PAD_ATA_RESET_B__SD3_CMD		IOMUX_PAD(0x608, 0x288, 2, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_CSI0_D12__CSI0_D12		IOMUX_PAD(0x41C, 0xF0, 0, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_D13__CSI0_D13		IOMUX_PAD(0x420, 0xF4, 0, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_D14__CSI0_D14		IOMUX_PAD(0x424, 0xF8, 0, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_D15__CSI0_D15		IOMUX_PAD(0x428, 0xFC, 0, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_D16__CSI0_D16		IOMUX_PAD(0x42C, 0x100, 0, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_D17__CSI0_D17		IOMUX_PAD(0x430, 0x104, 0, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_D18__CSI0_D18		IOMUX_PAD(0x434, 0x108, 0, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_D19__CSI0_D19		IOMUX_PAD(0x438, 0x10C, 0, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_D4__AUD3_TXC		IOMUX_PAD(0x3FC, 0xD0, 5, 0x0, 0, PAD_CTRL_4)
#define MX53_PAD_CSI0_D5__AUD3_TXD		IOMUX_PAD(0x400, 0xD4, 5, 0x0, 0, PAD_CTRL_4)
#define MX53_PAD_CSI0_D6__AUD3_TXFS		IOMUX_PAD(0x404, 0xD8, 5, 0x0, 0, PAD_CTRL_4)
#define MX53_PAD_CSI0_D7__GPIO_5_25		IOMUX_PAD(0x408, 0xDC, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_DATA_EN__GPIO_5_20	IOMUX_PAD(0x3F4, 0xC8, 1, 0x0, 0, PAD_CTRL_12)
#define MX53_PAD_CSI0_MCLK__CSI0_HSYNC		IOMUX_PAD(0x3F0, 0xC4, 0|IOMUX_CONFIG_SION, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_PIXCLK__CSI0_PIXCLK	IOMUX_PAD(0x3EC, 0xC0, 0|IOMUX_CONFIG_SION, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_CSI0_VSYNC__CSI0_VSYNC		IOMUX_PAD(0x3F8, 0xCC, 0|IOMUX_CONFIG_SION, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_DI0_DISP_CLK__DI0_DISP_CLK	IOMUX_PAD(0x378, 0x4C, 0, 0x0, 0, PAD_CTRL_5)
#define MX53_PAD_DI0_PIN15__DI0_PIN15		IOMUX_PAD(0x37C, 0x50, 0, 0x0, 0, PAD_CTRL_5)
#define MX53_PAD_DI0_PIN2__DI0_PIN2		IOMUX_PAD(0x380, 0x54, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DI0_PIN3__DI0_PIN3		IOMUX_PAD(0x384, 0x58, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT0__DISP0_DAT0		IOMUX_PAD(0x38C, 0x60, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT10__DISP0_DAT10	IOMUX_PAD(0x3B4, 0x88, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT11__DISP0_DAT11	IOMUX_PAD(0x3B8, 0x8C, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT12__DISP0_DAT12	IOMUX_PAD(0x3BC, 0x90, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT13__DISP0_DAT13	IOMUX_PAD(0x3C0, 0x94, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT14__DISP0_DAT14	IOMUX_PAD(0x3C4, 0x98, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT15__DISP0_DAT15	IOMUX_PAD(0x3C8, 0x9C, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT16__DISP0_DAT16	IOMUX_PAD(0x3CC, 0xA0, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT17__DISP0_DAT17	IOMUX_PAD(0x3D0, 0xA4, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT18__DISP0_DAT18	IOMUX_PAD(0x3D4, 0xA8, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT19__DISP0_DAT19	IOMUX_PAD(0x3D8, 0xAC, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT1__DISP0_DAT1		IOMUX_PAD(0x390, 0x64, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT20__DISP0_DAT20	IOMUX_PAD(0x3DC, 0xB0, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT21__DISP0_DAT21	IOMUX_PAD(0x3E0, 0xB4, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT22__DISP0_DAT22	IOMUX_PAD(0x3E4, 0xB8, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT23__DISP0_DAT23	IOMUX_PAD(0x3E8, 0xBC, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT2__DISP0_DAT2		IOMUX_PAD(0x394, 0x68, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT3__DISP0_DAT3		IOMUX_PAD(0x398, 0x6C, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT4__DISP0_DAT4		IOMUX_PAD(0x39C, 0x70, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT5__DISP0_DAT5		IOMUX_PAD(0x3A0, 0x74, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT6__DISP0_DAT6		IOMUX_PAD(0x3A4, 0x78, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT7__DISP0_DAT7		IOMUX_PAD(0x3A8, 0x7C, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT8__DISP0_DAT8		IOMUX_PAD(0x3AC, 0x80, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_DISP0_DAT9__DISP0_DAT9		IOMUX_PAD(0x3B0, 0x84, 0, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_EIM_A16__GPIO_2_22		IOMUX_PAD(0x4C8, 0x17C, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_A17__GPIO_2_21		IOMUX_PAD(0x4C4, 0x178, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_A18__GPIO_2_20		IOMUX_PAD(0x4C0, 0x174, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_A19__GPIO_2_19		IOMUX_PAD(0x4BC, 0x170, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_A20__GPIO_2_18		IOMUX_PAD(0x4B8, 0x16C, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_A21__GPIO_2_17		IOMUX_PAD(0x4B4, 0x168, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_A22__GPIO_2_16		IOMUX_PAD(0x4B0, 0x164, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_A23__GPIO_6_6		IOMUX_PAD(0x4AC, 0x160, 1|IOMUX_CONFIG_SION, 0x0, 0, PAD_CTRL_4)
#define MX53_PAD_EIM_A24__GPIO_5_4		IOMUX_PAD(0x4A8, 0x15C, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_A25__GPIO_5_2		IOMUX_PAD(0x458, 0x110, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_CS0__GPIO_2_23		IOMUX_PAD(0x4CC, 0x180, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_CS0_CSPI2_SCLK     	IOMUX_PAD(0x4CC, 0x180, 2, 0x7B8, 2, NO_PAD_CTRL)
#define MX53_PAD_EIM_CS1__GPIO_2_24		IOMUX_PAD(0x4D0, 0x184, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_CS1_CSPI2_MOSI		IOMUX_PAD(0x4D0, 0x184, 2, 0x7C0, 2, NO_PAD_CTRL)
#define MX53_PAD_EIM_DA8__GPIO_3_8              IOMUX_PAD(0x50C, 0x1BC, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_D16__CSPI1_SCLK		IOMUX_PAD(0x460, 0x118, 4, 0x79C, 3, PAD_CTRL_1)
#define MX53_PAD_EIM_D17__CSPI1_MISO		IOMUX_PAD(0x464, 0x11C, 4, 0x7A0, 3, PAD_CTRL_1)
#define MX53_PAD_EIM_D18__CSPI1_MOSI		IOMUX_PAD(0x468, 0x120, 4, 0x7A4, 3, PAD_CTRL_1)
#define MX53_PAD_EIM_D19__GPIO_3_19		IOMUX_PAD(0x46C, 0x124, 1, 0x0, 0, PAD_CTRL_4)
#define MX53_PAD_EIM_D20__GPIO_3_20		IOMUX_PAD(0x470, 0x128, 1, 0x0, 0, PAD_CTL_PUS_100K_UP)
#define MX53_PAD_EIM_D21__GPIO_3_21		IOMUX_PAD(0x474, 0x12C, 1, 0x0, 0, MX53_I2C1_PAD_CTRL)
#undef MX53_PAD_EIM_D21__I2C1_SCL
#define MX53_PAD_EIM_D21__I2C1_SCL		IOMUX_PAD(0x474, 0x12C, 5 | IOMUX_CONFIG_SION, 0x814, 1, MX53_I2C1_PAD_CTRL)
#define MX53_PAD_EIM_D22__GPIO_3_22		IOMUX_PAD(0x478, 0x130, 1, 0x0, 0, PAD_CTRL_12)
#define MX53_PAD_EIM_D23__GPIO_3_23		IOMUX_PAD(0x47C, 0x134, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_D24__UART3_TXD		IOMUX_PAD(0x484, 0x13C, 2, 0x0, 0, MX53_UART_PAD_CTRL)
#define MX53_PAD_EIM_D25__UART3_RXD		IOMUX_PAD(0x488, 0x140, 2, 0x888, 1, MX53_UART_PAD_CTRL)
#define MX53_PAD_EIM_D26__GPIO_3_26		IOMUX_PAD(0x48C, 0x144, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_D27__GPIO_3_27		IOMUX_PAD(0x490, 0x148, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_D28__GPIO_3_28		IOMUX_PAD(0x494, 0x14C, 1, 0x0, 0, MX53_I2C1_PAD_CTRL)
#undef MX53_PAD_EIM_D28__I2C1_SDA
#define MX53_PAD_EIM_D28__I2C1_SDA		IOMUX_PAD(0x494, 0x14C, 5 | IOMUX_CONFIG_SION, 0x818, 1, MX53_I2C1_PAD_CTRL)
#define MX53_PAD_EIM_D29__GPIO_3_29		IOMUX_PAD(0x498, 0x150, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_D30__GPIO_3_30		IOMUX_PAD(0x49C, 0x154, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_D31__GPIO_3_31		IOMUX_PAD(0x4A0, 0x158, 1, 0x0, 0, NO_PAD_CTRL)
#undef MX53_PAD_EIM_D31__UART3_RTS
#define MX53_PAD_EIM_D31__UART3_RTS		IOMUX_PAD(0x4A0, 0x158, 2, 0x884, 3, MX53_UART_PAD_CTRL)
#define MX53_PAD_EIM_DA0__GPIO_3_0		IOMUX_PAD(0x4EC, 0x19C, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_DA10__GPIO_3_10		IOMUX_PAD(0x514, 0x1C4, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_DA11__GPIO_3_11		IOMUX_PAD(0x518, 0x1C8, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_DA12__GPIO_3_12		IOMUX_PAD(0x51C, 0x1CC, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_DA13__GPIO_3_13		IOMUX_PAD(0x520, 0x1D0, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_DA14__GPIO_3_14		IOMUX_PAD(0x524, 0x1D4, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_DA1__GPIO_3_1		IOMUX_PAD(0x4F0, 0x1A0, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_DA6__GPIO_3_6		IOMUX_PAD(0x504, 0x1B4, 1, 0x0, 0, BUTTON_100K)
#define MX53_PAD_EIM_DA9__GPIO_3_9		IOMUX_PAD(0x510, 0x1C0, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_EB1__GPIO_2_29		IOMUX_PAD(0x4E8, 0x198, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_EB2__GPIO_2_30		IOMUX_PAD(0x45C, 0x114, 1, 0x0, 0, PAD_CTRL_4)
#define MX53_PAD_EIM_LBA_CSPI2_CS2      	IOMUX_PAD(0x4DC, 0x190, 2, 0x7C8, 1, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)
#define MX53_PAD_EIM_OE__DI1_PIN7		IOMUX_PAD(0x4D4, 0x188, 3, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_OE_CSPI2_MISO		IOMUX_PAD(0x4D4, 0x188, 2, 0x7BC, 2, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_22K_UP)
#define MX53_PAD_EIM_RW__DI1_PIN8		IOMUX_PAD(0x4D8, 0x18C, 3, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_EIM_WAIT__GPIO_5_0		IOMUX_PAD(0x534, 0x1E4, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_FEC_CRS_DV__FEC_CRS_DV		IOMUX_PAD(0x5D0, 0x254, 0, 0x0, 0, PAD_CTRL_8)
#define MX53_PAD_FEC_REF_CLK__FEC_REF_CLK	IOMUX_PAD(0x5C8, 0x24C, 0, 0x0, 0, PAD_CTRL_8)
#define MX53_PAD_FEC_RXD0__FEC_RXD0		IOMUX_PAD(0x5D8, 0x25C, 0, 0x0, 0, PAD_CTRL_8)
#define MX53_PAD_FEC_RXD1__FEC_RXD1		IOMUX_PAD(0x5D4, 0x258, 0, 0x0, 0, PAD_CTRL_8)
#define MX53_PAD_FEC_TXD0__FEC_TXD0		IOMUX_PAD(0x5E4, 0x268, 0, 0x0, 0, PAD_CTL_DSE_HIGH)
#define MX53_PAD_FEC_TXD1__FEC_TXD1		IOMUX_PAD(0x5E0, 0x264, 0, 0x0, 0, PAD_CTL_DSE_HIGH)
#define MX53_PAD_GPIO_0__SSI_EXT1_CLK		IOMUX_PAD(0x6A4, 0x314, 3, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_10__OSC32K_32K_OUT	IOMUX_PAD(0x540, 0x214, 1, 0x0, 0, PAD_CTRL_7)
#define MX53_PAD_GPIO_16__GPIO_7_11		IOMUX_PAD(0x6CC, 0x33C, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_17__GPIO_7_12		IOMUX_PAD(0x6D0, 0x340, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_18__GPIO_7_13		IOMUX_PAD(0x6D4, 0x344, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_19__FEC_TXD3		IOMUX_PAD(0x348, 0x20, 6, 0x0, 0, PAD_CTL_DSE_HIGH)
#define MX53_PAD_GPIO_1__PWMO			IOMUX_PAD(0x6A8, 0x318, 4, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_2__GPIO_1_2		IOMUX_PAD(0x6B8, 0x328, 1, 0x0, 0, BUTTON_100K)
#define MX53_PAD_GPIO_3__GPIO_1_3		IOMUX_PAD(0x6B0, 0x320, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_4__GPIO_1_4		IOMUX_PAD(0x6BC, 0x32C, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_5__GPIO_1_5		IOMUX_PAD(0x6C0, 0x330, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_6__GPIO_1_6		IOMUX_PAD(0x6B4, 0x324, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_7__GPIO_1_7		IOMUX_PAD(0x6C4, 0x334, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_8__GPIO_1_8		IOMUX_PAD(0x6C8, 0x338, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_GPIO_9__PWMO			IOMUX_PAD(0x6AC, 0x31C, 4, 0x0, 0, PAD_CTRL_1)
#define MX53_PAD_GPIO_9__GPIO_1_9		IOMUX_PAD(0x6AC, 0x31C, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_KEY_COL0__FEC_RXD3		IOMUX_PAD(0x34C, 0x24, 6, 0x0, 0, PAD_CTRL_8)
#define MX53_PAD_KEY_COL2__FEC_RXD2		IOMUX_PAD(0x35C, 0x34, 6, 0x0, 0, PAD_CTRL_8)
#define MX53_PAD_KEY_COL4__GPIO_4_14		IOMUX_PAD(0x36C, 0x44, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_KEY_ROW0__GPIO_4_7		IOMUX_PAD(0x350, 0x28, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_KEY_ROW2__FEC_TXD2		IOMUX_PAD(0x360, 0x38, 6, 0x0, 0, PAD_CTL_DSE_HIGH)
#define MX53_PAD_KEY_ROW3__GPIO_4_13		IOMUX_PAD(0x368, 0x40, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_KEY_ROW4__GPIO_4_15		IOMUX_PAD(0x370, 0x48, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS0_CLK_P__LVDS0_CLK		IOMUX_PAD(NON_PAD_I, 0x204, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS0_TX0_P__LVDS0_TX0		IOMUX_PAD(NON_PAD_I, 0x210, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS0_TX1_P__LVDS0_TX1		IOMUX_PAD(NON_PAD_I, 0x20C, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS0_TX2_P__LVDS0_TX2		IOMUX_PAD(NON_PAD_I, 0x208, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS0_TX3_P__LVDS0_TX3		IOMUX_PAD(NON_PAD_I, 0x200, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS1_CLK_P__LVDS1_CLK		IOMUX_PAD(NON_PAD_I, 0x1F4, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS1_TX0_P__LVDS1_TX0		IOMUX_PAD(NON_PAD_I, 0x1FC, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS1_TX1_P__LVDS1_TX1		IOMUX_PAD(NON_PAD_I, 0x1F8, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS1_TX2_P__LVDS1_TX2		IOMUX_PAD(NON_PAD_I, 0x1F0, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_LVDS1_TX3_P__LVDS1_TX3		IOMUX_PAD(NON_PAD_I, 0x1EC, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_NANDF_CS0__GPIO6_11_KEY	IOMUX_PAD(0x5B0, 0x238, 1|IOMUX_CONFIG_SION, 0x0, 0, BUTTON_PAD_CTRL)
#define MX53_PAD_NANDF_CS1__GPIO6_14_KEY	IOMUX_PAD(0x5B4, 0x23C, 1|IOMUX_CONFIG_SION, 0x0, 0, BUTTON_PAD_CTRL)
#define MX53_PAD_NANDF_CS2__CSI0_MCLK		IOMUX_PAD(0x5B8, 0x240, 5|IOMUX_CONFIG_SION, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_NANDF_CS3__GPIO_6_16		IOMUX_PAD(0x5BC, 0x244, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_SD1_CLK__SD1_CLK		IOMUX_PAD(0x67C, 0x2F4, IOMUX_CONFIG_SION, 0x0, 0, MX53_SDHC_PAD_CTRL | PAD_CTL_HYS)
#define MX53_PAD_SD1_CMD__SD1_CMD		IOMUX_PAD(0x674, 0x2EC, IOMUX_CONFIG_SION, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_SD1_DATA0__SD1_DATA0		IOMUX_PAD(0x66C, 0x2E4, 0, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_SD1_DATA1__SD1_DATA1		IOMUX_PAD(0x670, 0x2E8, 0, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_SD1_DATA2__SD1_DATA2		IOMUX_PAD(0x678, 0x2F0, 0, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_SD1_DATA3__SD1_DATA3		IOMUX_PAD(0x680, 0x2F8, 0, 0x0, 0, MX53_SDHC_PAD_CTRL)
#define MX53_PAD_SD2_CLK__GPIO_1_10		IOMUX_PAD(0x688, 0x2FC, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_SD2_CMD__GPIO_1_11		IOMUX_PAD(0x68C, 0x300, 1, 0x0, 0, NO_PAD_CTRL)
#define MX53_PAD_SD2_DATA0__AUD4_RXD		IOMUX_PAD(0x69C, 0x310, 3, 0x730, 1, NO_PAD_CTRL)
#define MX53_PAD_SD2_DATA1__AUD4_TXFS		IOMUX_PAD(0x698, 0x30C, 3, 0x744, 1, NO_PAD_CTRL)
#define MX53_PAD_SD2_DATA2__AUD4_TXD		IOMUX_PAD(0x694, 0x308, 3, 0x734, 1, NO_PAD_CTRL)
#define MX53_PAD_SD2_DATA3__AUD4_TXC		IOMUX_PAD(0x690, 0x304, 3, 0x740, 1, NO_PAD_CTRL)
/*
 * board changes needed for esai1 pins
 * Pad		gpio		nitrogen53_v1/2	nitrogen53/nitrogen53_a
 * EIM_A22	gpio2[20]			Display enable for chimei 7" panel (high active)
 * EIM_A17	gpio2[21]			PMIC_IRQ
 * GPIO_16	gpio7[11]	PMIC_IRQ	i2c3-sda
 * GPIO_6	gpio1[6]	i2c3-sda	SCKT of esai1
 */

extern int __init mx53_nitrogen_init_da9052(unsigned irq);

#define MAKE_GP(port, bit) ((port - 1) * 32 + bit)

/* newer boards use GP7:11 for I2C3 SDA pin, older use GP1:6 */
#define N53_I2C_2_SDA_CURRENT MAKE_GP(7, 11)
#define N53_I2C_2_SDA_PREVIOUS MAKE_GP(1, 6)

struct gpio nitrogen53_gpios[] __initdata = {
#ifdef REV0
	{.label = "touch_int_gp1_0",	.gpio = MAKE_GP(1, 0),		.flags = GPIOF_DIR_IN},
#endif
#define N53_I2C_2_SCL				MAKE_GP(1, 3)
	{.label = "i2c-2-scl",		.gpio = MAKE_GP(1, 3),		.flags = GPIOF_DIR_IN},
//	{.label = "???Menu key",	.gpio = MAKE_GP(1, 4),		.flags = GPIOF_DIR_IN},

#define N53_I2C_1_SCL				MAKE_GP(2, 30)
	{.label = "i2c-1-scl",		.gpio = MAKE_GP(2, 30),		.flags = GPIOF_DIR_IN},
#define N53_SD1_CD				MAKE_GP(3, 13)
	{.label = "sdhc1-cd",		.gpio = MAKE_GP(3, 13),		.flags = GPIOF_DIR_IN},
#define N53_SD1_WP				MAKE_GP(3, 14)
	{.label = "sdhc1-wp",		.gpio = MAKE_GP(3, 14),		.flags = GPIOF_DIR_IN},
#define N53_SC16IS7XX_INT			MAKE_GP(3, 20)
	{.label = "sc16is7xx-int",	.gpio = MAKE_GP(3, 20),		.flags = GPIOF_DIR_IN},		/* EIM_D20 */
#define N53_I2C_0_SCL				MAKE_GP(3, 21)
	{.label = "i2c-0-scl",		.gpio = MAKE_GP(3, 21),		.flags = GPIOF_DIR_IN},
#define N53_I2C_0_SDA				MAKE_GP(3, 28)
	{.label = "i2c-0-sda",		.gpio = MAKE_GP(3, 28),		.flags = GPIOF_DIR_IN},
//The gpio_keys.c file will request these, they are here for documentation only
//	{.label = "Menu key",		.gpio = MAKE_GP(3, 25),		.flags = GPIOF_DIR_IN},
//	{.label = "Back key",		.gpio = MAKE_GP(3, 26),		.flags = GPIOF_DIR_IN},
//	{.label = "Search key",		.gpio = MAKE_GP(3, 27),		.flags = GPIOF_DIR_IN},
//	{.label = "Home key",		.gpio = MAKE_GP(3, 29),		.flags = GPIOF_DIR_IN},
#if !defined(CONFIG_N_9BIT) && !defined(CONFIG_N_9BIT_MODULE)
#ifndef CONFIG_MACH_MX53_NITROGEN_K
	{.label = "On/Off key",		.gpio = MAKE_GP(3, 30),		.flags = GPIOF_DIR_IN},
#endif
#endif
#define N53_I2C_1_SDA				MAKE_GP(4, 13)
	{.label = "i2c-1-sda",		.gpio = MAKE_GP(4, 13),		.flags = GPIOF_DIR_IN},
#define N53_TFP410_INT				MAKE_GP(4, 15)
	{.label = "tfp410int",		.gpio = MAKE_GP(4, 15),		.flags = GPIOF_DIR_IN},		/* KEY_ROW4 */

#ifdef CONFIG_MACH_MX53_NITROGEN_K
#define N53_I2C_CONNECTOR_INT			MAKE_GP(6, 6)
#else
#define N53_I2C_CONNECTOR_INT			MAKE_GP(7, 12)
#endif
	{.label = "i2c_int",		.gpio = N53_I2C_CONNECTOR_INT,	.flags = GPIOF_DIR_IN},

/* Outputs */
#define CAMERA_STROBE				MAKE_GP(1, 7)
	{.label = "Camera strobe",	.gpio = MAKE_GP(1, 7),		.flags = 0},
	// make sure gp2[29] is high, i2c_sel for tfp410
#define N53_TFP410_I2CMODE			MAKE_GP(2, 29)
	{.label = "tfp410_i2cmode",	.gpio = MAKE_GP(2, 29),		.flags = GPIOF_INIT_HIGH},	/* EIM_EB1 */
#define N53_SS1					MAKE_GP(3, 19)
	{.label = "ecspi_ss1",		.gpio = MAKE_GP(3, 19),		.flags = GPIOF_INIT_HIGH},	/* low active */
//	{.label = "Shutdown output",	.gpio = MAKE_GP(3, 31),		.flags = 0},
#define N53_AMP_ENABLE				MAKE_GP(4, 7)	/* KEY_ROW0 */
	{.label = "speaker_amp",	.gpio = MAKE_GP(4, 7),		.flags = 0},
#define CAMERA_RESET				MAKE_GP(4, 14)
	{.label = "Camera reset",	.gpio = MAKE_GP(4, 14),		.flags = 0},
#define N53_USB_HUB_RESET			MAKE_GP(5, 0)
	{.label = "USB HUB reset",	.gpio = MAKE_GP(5, 0),		.flags = 0},
#define N53_CAMERA_STANDBY			MAKE_GP(5, 20)
	{.label = "Camera standby",	.gpio = MAKE_GP(5, 20),		.flags = 0},
#define N53_PHY_RESET				MAKE_GP(7, 13)
	{.label = "ICS1893 reset",	.gpio = MAKE_GP(7, 13),		.flags = 0},	/* ICS1893 Ethernet PHY reset */
#if defined(CONFIG_SERIAL_IMX_RS485)
	{.label = "RS485 transmit enable",.gpio = CONFIG_SERIAL_IMX_RS485_GPIO,	.flags = 0},
#endif
};

extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void (*set_num_cpu_wp)(int num);
static int num_cpu_wp = 3;

static iomux_v3_cfg_t mx53common_pads[] = {
	/* AUD3, J8 - PCM */
	MX53_PAD_CSI0_D4__AUD3_TXC,
	MX53_PAD_CSI0_D5__AUD3_TXD,
	MX53_PAD_CSI0_D6__AUD3_TXFS,
//	MX53_PAD_CSI0_D7__AUD3_RXD,

	/* AUD4, sgtl5000 */
	MX53_PAD_SD2_CLK__GPIO_1_10,	/* temp AUD4_RXFS */
	MX53_PAD_SD2_CMD__GPIO_1_11,	/* temp AUD4_RXC */
	MX53_PAD_SD2_DATA0__AUD4_RXD,
	MX53_PAD_SD2_DATA1__AUD4_TXFS,
	MX53_PAD_SD2_DATA3__AUD4_TXC,
	MX53_PAD_SD2_DATA2__AUD4_TXD,

	/* ECSPI1 */
	MX53_PAD_EIM_D16__CSPI1_SCLK,
	MX53_PAD_EIM_D17__CSPI1_MISO,
	MX53_PAD_EIM_D18__CSPI1_MOSI,
	MX53_PAD_EIM_D19__GPIO_3_19,	/* SS1 */

#ifdef REV0
	MX53_PAD_GPIO_0__GPIO_1_0,
#else
	/* SGTL5000 clock sys_mclk */
	MX53_PAD_GPIO_0__SSI_EXT1_CLK,
#endif

	/* esdhc1 */
	MX53_PAD_SD1_CMD__SD1_CMD,
	MX53_PAD_SD1_CLK__SD1_CLK,
	MX53_PAD_SD1_DATA0__SD1_DATA0,
	MX53_PAD_SD1_DATA1__SD1_DATA1,
	MX53_PAD_SD1_DATA2__SD1_DATA2,
	MX53_PAD_SD1_DATA3__SD1_DATA3,
	MX53_PAD_EIM_DA13__GPIO_3_13,	/* SDHC1 SD_CD */
	MX53_PAD_EIM_DA14__GPIO_3_14,	/* SDHC1 SD_WP */

	/* esdhc3 */
	MX53_PAD_ATA_DATA8__SD3_DAT0,
	MX53_PAD_ATA_DATA9__SD3_DAT1,
	MX53_PAD_ATA_DATA10__SD3_DAT2,
	MX53_PAD_ATA_DATA11__SD3_DAT3,
	MX53_PAD_ATA_RESET_B__SD3_CMD,
	MX53_PAD_ATA_IORDY__SD3_CLK,
	MX53_PAD_EIM_DA11__GPIO_3_11,	/* SDHC3 SD_CD */
	MX53_PAD_EIM_DA12__GPIO_3_12,	/* SDHC3 SD_WP */

	/* FEC pins */
	MX53_PAD_GPIO_18__GPIO_7_13,	/* low active reset pin*/
	MX53_PAD_FEC_MDIO__FEC_MDIO,
	MX53_PAD_FEC_REF_CLK__FEC_REF_CLK,
	MX53_PAD_FEC_RX_ER__FEC_RX_ER,
	MX53_PAD_FEC_CRS_DV__FEC_CRS_DV,
	MX53_PAD_FEC_RXD1__FEC_RXD1,
	MX53_PAD_FEC_RXD0__FEC_RXD0,
	MX53_PAD_KEY_COL2__FEC_RXD2,	/* Nitrogen53 */
	MX53_PAD_KEY_COL0__FEC_RXD3,	/* Nitrogen53 */
	MX53_PAD_FEC_TX_EN__FEC_TX_EN,
	MX53_PAD_FEC_TXD1__FEC_TXD1,
	MX53_PAD_FEC_TXD0__FEC_TXD0,
	MX53_PAD_KEY_ROW2__FEC_TXD2,	/* Nitrogen53 */
	MX53_PAD_GPIO_19__FEC_TXD3,	/* Nitrogen53 */
	/* FEC TX_ER - unused output from mx53 */
	MX53_PAD_KEY_ROW1__FEC_COL,	/* Nitrogen53 */
	MX53_PAD_KEY_COL3__FEC_CRS,	/* Nitrogen53 */
	MX53_PAD_KEY_COL1__FEC_RX_CLK,	/* Nitrogen53 */
	MX53_PAD_FEC_MDC__FEC_MDC,

	/* GPIO1 */
	NEW_PAD_CTRL(MX53_PAD_GPIO_4__GPIO_1_4, BUTTON_PAD_CTRL) | MUX_SION_MASK,	/* ??Menu */
	NEW_PAD_CTRL(MX53_PAD_GPIO_5__GPIO_1_5, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	MX53_PAD_GPIO_8__GPIO_1_8,	/* J23 - rgb gp */

	/* GPIO2 */
	NEW_PAD_CTRL(MX53_PAD_EIM_A22__GPIO_2_16, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A21__GPIO_2_17, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A20__GPIO_2_18, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A19__GPIO_2_19, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A18__GPIO_2_20, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A17__GPIO_2_21, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Pmic IRQ */
	NEW_PAD_CTRL(MX53_PAD_EIM_A16__GPIO_2_22, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Camera power down(nitrogenA) */
	MX53_PAD_EIM_EB1__GPIO_2_29,									/* tpf410_i2cmode */

	/* GPIO3 */
	MX53_PAD_EIM_DA0__GPIO_3_0,	/* wl1271 wl_en */
	MX53_PAD_EIM_DA1__GPIO_3_1,	/* wl1271 bt_en */
	MX53_PAD_EIM_DA6__GPIO_3_6,	/* GPIO spare/Pic16F616 int on Nitrogen53A */
	MX53_PAD_EIM_DA10__GPIO_3_10,	/* I2C Connector Buffer enable */
	/* Keyboard, NitrogenA - SC16IS7XX i2c serial interrupt */
	NEW_PAD_CTRL(MX53_PAD_EIM_D20__GPIO_3_20, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_D22__GPIO_3_22, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_D26__GPIO_3_26, BUTTON_PAD_CTRL) | MUX_SION_MASK,	/* Back key */
	NEW_PAD_CTRL(MX53_PAD_EIM_D27__GPIO_3_27, BUTTON_PAD_CTRL) | MUX_SION_MASK,	/* Search Key */
	NEW_PAD_CTRL(MX53_PAD_EIM_D29__GPIO_3_29, BUTTON_PAD_CTRL) | MUX_SION_MASK,	/* Home Key */

	/* GPIO4 */
	MX53_PAD_KEY_ROW0__GPIO_4_7,	/* N53_AMP_ENABLE, Speaker Amp Enable */
	NEW_PAD_CTRL(MX53_PAD_KEY_ROW4__GPIO_4_15, PAD_CTL_HYS | PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_22K_UP),	/* tfp410int */

	/* GPIO5 */
	MX53_PAD_EIM_WAIT__GPIO_5_0,	/* USB HUB reset */
	NEW_PAD_CTRL(MX53_PAD_EIM_A25__GPIO_5_2, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* NitrogenA - eMMC reset */
	MX53_PAD_EIM_A24__GPIO_5_4,	/* USB OTG USB_OC */

	/* GPIO6 */
	MX53_PAD_EIM_A23__GPIO_6_6,	/* USB OTG USB_PWR */
	MX53_PAD_NANDF_CS0__GPIO6_11_KEY, /* 53K volume up */
	MX53_PAD_NANDF_CS1__GPIO6_14_KEY, /* 53K volume down */
	MX53_PAD_NANDF_CS3__GPIO_6_16,	/* NitrogenA mic mux, Nitrogen53K back button */

	/* GPIO7 */
	MX53_PAD_GPIO_17__GPIO_7_12,	/* I2C Connector interrupt */

	/* I2C1 */
	MX53_PAD_EIM_D21__I2C1_SCL,	/* GPIO3[21] */
	MX53_PAD_EIM_D28__I2C1_SDA,	/* GPIO3[28] */

	/* I2C2 */
	MX53_PAD_EIM_EB2__I2C2_SCL,	/* GPIO2[30] */
	MX53_PAD_KEY_ROW3__I2C2_SDA,	/* GPIO4[13] */

	/* IPU - Camera */
//	MX53_PAD_CSI0_D8__CSI0_D8,	/* ov5640 doesn't use D8-D11 */
//	MX53_PAD_CSI0_D9__CSI0_D9,
//	MX53_PAD_CSI0_D10__CSI0_D10,
//	MX53_PAD_CSI0_D11__CSI0_D11,
	MX53_PAD_CSI0_D12__CSI0_D12,
	MX53_PAD_CSI0_D13__CSI0_D13,
	MX53_PAD_CSI0_D14__CSI0_D14,
	MX53_PAD_CSI0_D15__CSI0_D15,
	MX53_PAD_CSI0_D16__CSI0_D16,
	MX53_PAD_CSI0_D17__CSI0_D17,
	MX53_PAD_CSI0_D18__CSI0_D18,
	MX53_PAD_CSI0_D19__CSI0_D19,
	MX53_PAD_CSI0_VSYNC__CSI0_VSYNC,
	MX53_PAD_CSI0_MCLK__CSI0_HSYNC,
	MX53_PAD_CSI0_PIXCLK__CSI0_PIXCLK,
	MX53_PAD_NANDF_CS2__CSI0_MCLK,
	MX53_PAD_GPIO_2__GPIO_1_2,	/* CAMERA_POWERDOWN (nitrogen53) */
	MX53_PAD_GPIO_7__GPIO_1_7,	/* CAMERA_STROBE */
	MX53_PAD_KEY_COL4__GPIO_4_14,	/* CAMERA_RESET */
	MX53_PAD_CSI0_DATA_EN__GPIO_5_20, /* Camera Standby */

	/* IPU - Display */
	MX53_PAD_DI0_DISP_CLK__DI0_DISP_CLK,	/* DI0 display clock */
	MX53_PAD_DI0_PIN15__DI0_PIN15,		/* DI0 data enable */
	MX53_PAD_DI0_PIN2__DI0_PIN2,		/* DI0 HSYNC */
	MX53_PAD_DI0_PIN3__DI0_PIN3,		/* DI0 VSYNC */
	MX53_PAD_DISP0_DAT0__DISP0_DAT0,
	MX53_PAD_DISP0_DAT1__DISP0_DAT1,
	MX53_PAD_DISP0_DAT2__DISP0_DAT2,
	MX53_PAD_DISP0_DAT3__DISP0_DAT3,
	MX53_PAD_DISP0_DAT4__DISP0_DAT4,
	MX53_PAD_DISP0_DAT5__DISP0_DAT5,
	MX53_PAD_DISP0_DAT6__DISP0_DAT6,
	MX53_PAD_DISP0_DAT7__DISP0_DAT7,
	MX53_PAD_DISP0_DAT8__DISP0_DAT8,
	MX53_PAD_DISP0_DAT9__DISP0_DAT9,
	MX53_PAD_DISP0_DAT10__DISP0_DAT10,
	MX53_PAD_DISP0_DAT11__DISP0_DAT11,
	MX53_PAD_DISP0_DAT12__DISP0_DAT12,
	MX53_PAD_DISP0_DAT13__DISP0_DAT13,
	MX53_PAD_DISP0_DAT14__DISP0_DAT14,
	MX53_PAD_DISP0_DAT15__DISP0_DAT15,
	MX53_PAD_DISP0_DAT16__DISP0_DAT16,
	MX53_PAD_DISP0_DAT17__DISP0_DAT17,
	MX53_PAD_DISP0_DAT18__DISP0_DAT18,
	MX53_PAD_DISP0_DAT19__DISP0_DAT19,
	MX53_PAD_DISP0_DAT20__DISP0_DAT20,
	MX53_PAD_DISP0_DAT21__DISP0_DAT21,
	MX53_PAD_DISP0_DAT22__DISP0_DAT22,
	MX53_PAD_DISP0_DAT23__DISP0_DAT23,
	MX53_PAD_EIM_RW__DI1_PIN8,	/* VGA VSync */

	/* LDB - LVDS0 */
	MX53_PAD_LVDS0_TX3_P__LVDS0_TX3,
	MX53_PAD_LVDS0_CLK_P__LVDS0_CLK,
	MX53_PAD_LVDS0_TX2_P__LVDS0_TX2,
	MX53_PAD_LVDS0_TX1_P__LVDS0_TX1,
	MX53_PAD_LVDS0_TX0_P__LVDS0_TX0,

	/* LDB - LVDS1 */
	MX53_PAD_LVDS1_TX3_P__LVDS1_TX3,
	MX53_PAD_LVDS1_CLK_P__LVDS1_CLK,
	MX53_PAD_LVDS1_TX2_P__LVDS1_TX2,
	MX53_PAD_LVDS1_TX1_P__LVDS1_TX1,
	MX53_PAD_LVDS1_TX0_P__LVDS1_TX0,

	/* PWM1 backlight */
	MX53_PAD_GPIO_9__PWMO,		/* pwm1 */

	/* PWM2 backlight */
	MX53_PAD_GPIO_1__PWMO,		/* pwm2 */

	/* UART1 */
	MX53_PAD_ATA_DIOW__UART1_TXD,
	MX53_PAD_ATA_DMACK__UART1_RXD,

	/* UART2 */
	MX53_PAD_ATA_DMARQ__UART2_TXD,
	MX53_PAD_ATA_BUFFER_EN__UART2_RXD,
	MX53_PAD_ATA_INTRQ__UART2_CTS,
	MX53_PAD_ATA_DIOR__UART2_RTS,
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
	 .cpu_voltage = 1150000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 800000000,
	 .pdf = 0,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 0,
	 .cpu_voltage = 1050000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 160000000,
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
	 /* 1080i50 TV output */
	 "1080I50", 50, 1920, 1080, 13468,
	 192, 527,
	 20, 24,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED | FB_VMODE_ODD_FLD_FIRST,
	 0,},
	{
	 /* 1080i60 TV output */
	 "1080I60", 60, 1920, 1080, 13468,
	 192, 87,
	 20, 24,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED | FB_VMODE_ODD_FLD_FIRST,
	 0,},
	{
	 /* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	 "CLAA-WVGA", 57, 800, 480, 37037, 40, 60, 10, 10, 20, 10,
	 FB_SYNC_CLK_LAT_FALL,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 "XGA", 60, 1024, 768, 15385,
	 220, 40,
	 21, 7,
	 60, 10,
	 0,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 /* 720p30 TV output */
	 "720P30", 30, 1280, 720, 13468,
	 260, 1759,
	 25, 4,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 "720P60", 60, 1280, 720, 13468,
	 260, 109,
	 25, 4,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	/* VGA 1280x1024 108M pixel clk output */
	"SXGA", 60, 1280, 1024, 9259,
	48, 248,
	1, 38,
	112, 3,
	0,
	FB_VMODE_NONINTERLACED,
	0,},
	{
	/* 1600x1200 @ 60 Hz 162M pixel clk*/
	"UXGA", 60, 1600, 1200, 6172,
	304, 64,
	1, 46,
	192, 3,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
	FB_VMODE_NONINTERLACED,
	0,},
	{
	 /* 1080p24 TV output */
	 "1080P24", 24, 1920, 1080, 13468,
	 192, 637,
	 38, 6,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 /* 1080p25 TV output */
	 "1080P25", 25, 1920, 1080, 13468,
	 192, 527,
	 38, 6,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 /* 1080p30 TV output */
	 "1080P30", 30, 1920, 1080, 13468,
	 192, 87,
	 38, 6,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 "1080P60", 60, 1920, 1080, 7692,
	 100, 40,
	 30, 3,
	 10, 2,
	 0,
	 FB_VMODE_NONINTERLACED,
	 0,},
};

struct cpu_wp *mx53_nitrogen_get_cpu_wp(int *wp)
{
	*wp = num_cpu_wp;
	return cpu_wp_auto;
}

void mx53_nitrogen_set_num_cpu_wp(int num)
{
	num_cpu_wp = num;
	return;
}

static struct mxc_w1_config mxc_w1_data = {
	.search_rom_accelerator = 1,
};

#if defined(CONFIG_MXC_PWM) && defined(CONFIG_BACKLIGHT_PWM)
static struct mxc_pwm_platform_data mxc_pwm1_platform_data = {
	.pwmo_invert = 0,	/* GPIO_9 */
	.clk_select = PWM_CLK_HIGHPERF,
};

static struct mxc_pwm_platform_data mxc_pwm2_platform_data = {
	.pwmo_invert = 0,	/* GPIO_1 */
};

/* GPIO_1 lcd backlight(pwm2) */
static struct platform_pwm_backlight_data mxc_backlight_data_pwm2 = {
	.pwm_id = 1,
	.max_brightness = 256,
	.dft_brightness = CONFIG_DEFAULT_PWM0_BACKLIGHT,
	.pwm_period_ns = 1000000000/32768,	/* 30517 */
#ifdef CONFIG_MACH_MX53_NITROGEN_K
	.usable_range = {226,256},
#endif
};
#endif

#if defined(CONFIG_MXC_PWM) && defined(CONFIG_BACKLIGHT_PWM)
/* GPIO_9 backlight (pwm1) */
static struct platform_pwm_backlight_data mxc_backlight_data_pwm1 = {
	.pwm_id = 0,
	.max_brightness = 256,
	.dft_brightness = CONFIG_DEFAULT_PWM1_BACKLIGHT,
	.pwm_period_ns = 1000000000/32768,	/* 30517 */
};
#endif

extern void mx5_ipu_reset(void);
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 3,
	.reset = mx5_ipu_reset,
};

extern void mx5_vpu_reset(void);
static struct mxc_vpu_platform_data mxc_vpu_data = {
	.reset = mx5_vpu_reset,
};

static struct fec_platform_data fec_data = {
	.phy = PHY_INTERFACE_MODE_GMII,
};

/* workaround for ecspi chipselect pin may not keep correct level when idle */
static void mx53_nitrogen_gpio_spi_chipselect_active(int cspi_mode, int status,
					     int chipselect)
{
	if ((cspi_mode == 1) && (chipselect == 2)) {
//		pr_info("spi cs active\n");
		gpio_set_value(N53_SS1, 0);		/* low active */
	}
}

static void mx53_nitrogen_gpio_spi_chipselect_inactive(int cspi_mode, int status,
					       int chipselect)
{
	if ((cspi_mode == 1) && (chipselect == 2)) {
		gpio_set_value(N53_SS1, 1);		/* low active */
//		pr_info("spi cs inactive\n");
	}
}

static struct mxc_spi_master mxcspi1_data = {
	.maxchipselect = 4,
	.spi_version = 23,
	.chipselect_active = mx53_nitrogen_gpio_spi_chipselect_active,
	.chipselect_inactive = mx53_nitrogen_gpio_spi_chipselect_inactive,
};

static struct mxc_spi_master mxcspi2_data = {
	.maxchipselect = 4,
	.spi_version = 23,
};

#define PRINT_SDA
#define PD_CLK_I2C	0
#define PD_CLK_GP	1
#define PD_SDA_I2C	2
#define PD_SDA_GP	3
/* Generate a pulse on the i2c clock pin. */
static void i2c_clock_toggle(unsigned gp_clk, unsigned gp_dat, const iomux_v3_cfg_t *pd)
{
	unsigned i;
	printk(KERN_INFO "%s, gp_clk=0x%x, gp_dat=0x%x\n", __FUNCTION__, gp_clk, gp_dat);
	gpio_direction_input(gp_clk);
	mxc_iomux_v3_setup_pad(pd[PD_CLK_GP]);

#ifdef PRINT_SDA
	gpio_direction_input(gp_dat);
	mxc_iomux_v3_setup_pad(pd[PD_SDA_GP]);
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

        mxc_iomux_v3_setup_pad(pd[PD_CLK_I2C]);
#ifdef PRINT_SDA
	mxc_iomux_v3_setup_pad(pd[PD_SDA_I2C]);
#endif
}

static void i2c_clock_toggle0(void)
{
	const iomux_v3_cfg_t pd[] = {
		MX53_PAD_EIM_D21__I2C1_SCL, MX53_PAD_EIM_D21__GPIO_3_21,
		MX53_PAD_EIM_D28__I2C1_SDA, MX53_PAD_EIM_D28__GPIO_3_28,
	};
	i2c_clock_toggle(N53_I2C_0_SCL, N53_I2C_0_SDA, pd);
}

static void i2c_clock_toggle1(void)
{
	const iomux_v3_cfg_t pd[] = {
		MX53_PAD_EIM_EB2__I2C2_SCL, NEW_PAD_CTRL(MX53_PAD_EIM_EB2__GPIO_2_30, PAD_CTRL_9) | MUX_SION_MASK,
		MX53_PAD_KEY_ROW3__I2C2_SDA, NEW_PAD_CTRL(MX53_PAD_KEY_ROW3__GPIO_4_13, PAD_CTRL_9) | MUX_SION_MASK,
	};
	i2c_clock_toggle(N53_I2C_1_SCL, N53_I2C_1_SDA, pd);
}

static void i2c_clock_toggle2_current(void)
{
	const iomux_v3_cfg_t pd[] = {
		MX53_PAD_GPIO_3__I2C3_SCL, NEW_PAD_CTRL(MX53_PAD_GPIO_3__GPIO_1_3, PAD_CTRL_9) | MUX_SION_MASK,
		MX53_PAD_GPIO_16__I2C3_SDA, NEW_PAD_CTRL(MX53_PAD_GPIO_16__GPIO_7_11, PAD_CTRL_9) | MUX_SION_MASK,
	};
	i2c_clock_toggle(N53_I2C_2_SCL, N53_I2C_2_SDA_CURRENT, pd);
}

static struct imxi2c_platform_data mxci2c0_data = {
	.bitrate = 400000,
	.i2c_clock_toggle = i2c_clock_toggle0,
};
static struct imxi2c_platform_data mxci2c1_data = {
	.bitrate = 100000,
	.i2c_clock_toggle = i2c_clock_toggle1,
};
static struct imxi2c_platform_data mxci2c2_data = {
	.bitrate = 100000,
	.i2c_clock_toggle = i2c_clock_toggle2_current,
};

static struct mxc_dvfs_platform_data dvfs_core_data = {
	.reg_id = "DA9052_BUCK_CORE",
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

static struct mxc_bus_freq_platform_data bus_freq_data = {
	.gp_reg_id = "DA9052_BUCK_CORE",
	.lp_reg_id = "DA9052_BUCK_PRO",
};

static struct tve_platform_data tve_data = {
	.dac_reg = "VVIDEO",
};

static struct ldb_platform_data ldb_data = {
	.lvds_bg_reg = "VAUDIO",
	.ext_ref = LDB_EXT_REF,
};

static void mxc_iim_enable_fuse(void)
{
	u32 reg;

	if (!ccm_base)
		return;

	/* enable fuse blown */
	reg = readl(ccm_base + 0x64);
	reg |= 0x10;
	writel(reg, ccm_base + 0x64);
}

static void mxc_iim_disable_fuse(void)
{
	u32 reg;

	if (!ccm_base)
		return;
	/* enable fuse blown */
	reg = readl(ccm_base + 0x64);
	reg &= ~0x10;
	writel(reg, ccm_base + 0x64);
}

static struct mxc_iim_data iim_data = {
	.bank_start = MXC_IIM_MX53_BANK_START_ADDR,
	.bank_end   = MXC_IIM_MX53_BANK_END_ADDR,
	.enable_fuse = mxc_iim_enable_fuse,
	.disable_fuse = mxc_iim_disable_fuse,
};


static void mx53_gpio_host1_driver_vbus(bool on)
{
	pr_info("%s: on=%d\n", __func__, on);
}

static struct resource mxcfb_resources[] = {
	{
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .flags = IORESOURCE_MEM,
	 },
};

static struct mxc_fb_platform_data fb_data[] = {
	{
	 .interface_pix_fmt = IPU_PIX_FMT_RGB565,
	 .mode_str = "CLAA-WVGA",
	 .mode = video_modes,
	 .num_modes = ARRAY_SIZE(video_modes),
	 },
	{
	 .interface_pix_fmt = IPU_PIX_FMT_GBR24,
	 .mode_str = "1024x768M-16@60",
	 .mode = video_modes,
	 .num_modes = ARRAY_SIZE(video_modes),
	 },
};

extern int primary_di;
static int __init mxc_init_fb(void)
{
	if (primary_di) {
		printk(KERN_INFO "DI1 is primary\n");
		/* DI1 -> DP-BG channel: */
		mxc_fb_devices[1].num_resources = 1;
		mxc_fb_devices[1].resource = &mxcfb_resources[1];
		mxc_register_device(&mxc_fb_devices[1], &fb_data[1]);

		/* DI0 -> DC channel: */
		mxc_fb_devices[0].num_resources = 1;
		mxc_fb_devices[0].resource = &mxcfb_resources[1];
		mxc_register_device(&mxc_fb_devices[0], &fb_data[0]);
	} else {
		printk(KERN_INFO "DI0 is primary\n");

		/* DI0 -> DP-BG channel: */
		mxc_fb_devices[0].num_resources = 1;
		mxc_fb_devices[0].resource = &mxcfb_resources[0];
		mxc_register_device(&mxc_fb_devices[0], &fb_data[0]);

		/* DI1 -> DC channel: */
		mxc_register_device(&mxc_fb_devices[1], &fb_data[1]);
	}

	/*
	 * DI0/1 DP-FG channel:
	 */
	mxc_fb_devices[2].num_resources = 1;
	mxc_fb_devices[2].resource = &mxcfb_resources[2];
	mxc_register_device(&mxc_fb_devices[2], NULL);

	return 0;
}
device_initcall(mxc_init_fb);

struct plat_i2c_generic_data {
	unsigned irq;
	unsigned gp;
};

static struct plat_i2c_generic_data i2c_generic_data = {
	.irq = gpio_to_irq(N53_I2C_CONNECTOR_INT),
	.gp = N53_I2C_CONNECTOR_INT,
};

struct plat_i2c_tfp410_data {
	int irq;
	int gp;
	int gp_i2c_sel;
};

static struct plat_i2c_tfp410_data i2c_tfp410_data = {
	.irq = gpio_to_irq(N53_TFP410_INT), .gp = N53_TFP410_INT,
	.gp_i2c_sel = N53_TFP410_I2CMODE
};

#if defined(CONFIG_MTD)
static struct mtd_partition mxc_spi_nor_partitions[] = {
	{
	 .name = "bootloader",
	 .offset = 0,
	 .size = 0x000100000,},
	{
	 .name = "kernel",
	 .offset = MTDPART_OFS_APPEND,
	 .size = MTDPART_SIZ_FULL,},
};

static struct flash_platform_data mxc_spi_flash_data = {
	.name = "m25p80",
	.parts = mxc_spi_nor_partitions,
	.nr_parts = ARRAY_SIZE(mxc_spi_nor_partitions),
	.type = "sst25vf016b",
};


static struct spi_board_info mxc_spi_nor_device[] __initdata = {
	{
	 .modalias = "m25p80",
	 .max_speed_hz = 25000000,	/* max spi clock (SCK) speed in HZ */
	 .bus_num = 1,
	 .chip_select = 1,
	 .platform_data = &mxc_spi_flash_data,
	},
};
#endif

#if defined(CONFIG_LTC1960) || defined(CONFIG_LTC1960_MODULE)
#if defined (CONFIG_BATTERY_BQ20Z75) || defined (CONFIG_BATTERY_BQ20Z75)
static struct ltc1960_battery_info_t removable_battery = {
	.name = "bq20z75"
,	.charge_uv = 12600000
,	.charge_ua = 2500000
,	.trickle_seconds = 5*60
};
#endif

static struct ltc1960_battery_info_t permanent_battery = {
	.name = "dumb"
,	.charge_uv = 12600000
,	.charge_ua = 1500000
,	.trickle_seconds = 60
};

static struct ltc1960_platform_data_t ltc1960_pdata = {
	.batteries = {
		&permanent_battery,
#if defined (CONFIG_BATTERY_BQ20Z75) || defined (CONFIG_BATTERY_BQ20Z75)
		&removable_battery,
#endif
	}
};
#endif

static struct spi_board_info spidev[] __initdata = {
	{
#if defined(CONFIG_LTC1960) || defined(CONFIG_LTC1960_MODULE)
	.modalias = "ltc1960",
	.platform_data = &ltc1960_pdata,
	 .max_speed_hz = 100000,	/* max spi clock (SCK) speed in HZ */
#else
	 .modalias = "spidev",
	 .max_speed_hz = 1000000,	/* max spi clock (SCK) speed in HZ */
#endif
	 .bus_num = 2,
	 .chip_select = 1
	}
};

static unsigned int sdhc_get_card_det_status1(struct device *dev)
{
	return gpio_get_value(N53_SD1_CD);
}

static int sdhc_write_protect1(struct device *dev)
{
	return gpio_get_value(N53_SD1_WP);
}

static unsigned int sdhc_get_card_det_true(struct device *dev)
{
	return 0 ;
}

static struct mxc_mmc_platform_data mmc1_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status1,
	.wp_status = sdhc_write_protect1,
	.power_mmc = NULL,
};

static struct mxc_mmc_platform_data mmc3_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_DATA_DDR | MMC_CAP_POWER_OFF_CARD,
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 1,
	.status = sdhc_get_card_det_true,
	.power_mmc = "vmmc",
};

static int mxc_sgtl5000_amp_enable(int enable)
{
	gpio_set_value(N53_AMP_ENABLE, enable);
	return 0;
}

static int headphone_det_status(void)
{
	return 0;
}

static int mxc_sgtl5000_init(void);

static struct mxc_audio_platform_data sgtl5000_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 4,
	.hp_status = headphone_det_status,
	.amp_enable = mxc_sgtl5000_amp_enable,
	.sysclk = 26000000,
	.init = mxc_sgtl5000_init,
};

static int mxc_sgtl5000_init(void)
{
#ifndef REV0
	struct clk *ssi_ext1;
	int rate;

	ssi_ext1 = clk_get(NULL, "ssi_ext1_clk");
	if (IS_ERR(ssi_ext1))
		return -1;

	rate = clk_round_rate(ssi_ext1, 26000000);
	if (rate < 8000000 || rate > 27000000) {
		printk(KERN_ERR "Error: SGTL5000 mclk freq %d out of range!\n",
		       rate);
		clk_put(ssi_ext1);
		return -1;
	}

	clk_set_rate(ssi_ext1, rate);
	printk(KERN_INFO "SGTL5000 mclk freq is %d\n", rate);
	clk_enable(ssi_ext1);
	sgtl5000_data.sysclk = rate;
#endif
	return 0;
}

static struct platform_device mxc_sgtl5000_device = {
	.name = "imx-3stack-sgtl5000",
};

static struct mxc_mlb_platform_data mlb_data = {
	.reg_nvcc = "VCAM",
	.mlb_clk = "mlb_clk",
};

#ifdef CONFIG_KEYBOARD_GPIO
static struct gpio_keys_button gpio_keys[] = {
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(3,29),
		.code	= KEY_HOME,		/* 102 (0x66) */
		.desc	= "Home Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
#ifndef CONFIG_MACH_MX53_NITROGEN_K
		.gpio	= MAKE_GP(3,26),
#else
		.gpio	= MAKE_GP(6,16),
#endif
		.code	= KEY_BACK,		/* 158 (0x9E) */
		.desc	= "Back Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(1,4),		/* menu key, new Nitrogen53 A rev will fix up */
		.code	= KEY_MENU,		/* 139 (0x8B) */
		.desc	= "Menu Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(3,27),
		.code	= KEY_SEARCH,		/* 217 (0xD9) */
		.desc	= "Search Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	/* Below this point only applies to new rev of nitrogen53A */
#if defined(CONFIG_MACH_NITROGEN_A_IMX53)
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(3,22),
		.code	= KEY_POWER,		/* 116 (0x74) */
		.desc	= "Power Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
#ifdef CONFIG_GPIO_SC16IS7XX_IRQ
	{
		.type	= EV_KEY,
		.gpio	= 255,			/* J2 Pin 1 */
		.code	= KEY_CAMERA,
		.desc	= "Camera Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 252,			/* J4 Pin 4 */
		.code	= KEY_PHONE,
		.desc	= "Phone Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 253,			/* J2 Pin 3 */
		.code	= KEY_VOLUMEUP,
		.desc	= "Volume+ Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 248,			/* J2 Pin 4 */
		.code	= KEY_VOLUMEDOWN,
		.desc	= "Volume- Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 251,			/* J4 Pin 3 */
		.code	= KEY_DOCUMENTS,
		.desc	= "Documents Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 249,			/* J4 Pin 1 */
		.code	= KEY_F1,
		.desc	= "Torch LED Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 250,			/* J4 Pin 2 */
		.code	= KEY_F2,
		.desc	= "Splice Monitor Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 254,			/* J2 Pin 2 */
		.code	= KEY_F3,
		.desc	= "Belt Survey Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
#endif
#endif
#if defined(CONFIG_MACH_MX53_NITROGEN_K)
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(6,14),
		.code	= KEY_VOLUMEUP,
		.desc	= "Volume+ Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(6,11),
		.code	= KEY_VOLUMEDOWN,
		.desc	= "Volume- Button",
		.wakeup	= 0,
		.active_low = 1,
		.debounce_interval = 30,
	},
#endif
};

static struct gpio_keys_platform_data gpio_keys_platform_data = {
	.buttons        = gpio_keys,
	.nbuttons       = ARRAY_SIZE(gpio_keys),
};

static struct platform_device gpio_keys_device = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &gpio_keys_platform_data,
	},
};
#endif


static struct mxc_spdif_platform_data mxc_spdif_data = {
	.spdif_tx = 1,
	.spdif_rx = 0,
	.spdif_clk_44100 = 0,	/* Souce from CKIH1 for 44.1K */
	.spdif_clk_48000 = 7,	/* Source from CKIH2 for 48k and 32k */
	.spdif_clkid = 0,
	.spdif_clk = NULL,	/* spdif bus clk */
};


static int __initdata enable_w1 = { 0 };
static int __init w1_setup(char *__unused)
{
	enable_w1 = 1;
	return cpu_is_mx53();
}
__setup("w1", w1_setup);


static int __initdata enable_spdif = { 0 };
static int __init spdif_setup(char *__unused)
{
	enable_spdif = 1;
	return 1;
}
__setup("spdif", spdif_setup);

static struct android_pmem_platform_data android_pmem_data = {
	.name = "pmem_adsp",
	.size = SZ_64M,
};

static struct android_pmem_platform_data android_pmem_gpu_data = {
	.name = "pmem_gpu",
	.size = SZ_64M,
	.cached = 1,
};

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

static char *usb_functions_accessory[] = {
	"accessory",
};

static char *usb_functions_accessory_adb[] = {
	"accessory",
	"adb",
};

static char *usb_functions_all[] = {
	"usb_mass_storage",
	"adb",
	"rndis",
	"accessory",
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
		.product_id	= 0x0c10,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis),
		.functions	= usb_functions_rndis,
	},
	{
		.vendor_id	= USB_ACCESSORY_VENDOR_ID,
		.product_id	= USB_ACCESSORY_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_accessory),
		.functions	= usb_functions_accessory,
	},
	{
		.vendor_id	= USB_ACCESSORY_VENDOR_ID,
		.product_id	= USB_ACCESSORY_ADB_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_accessory_adb),
		.functions	= usb_functions_accessory_adb,
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

#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
#define MAX_CAMERA_FRAME_SIZE (((2592*1944*2+PAGE_SIZE-1)/PAGE_SIZE)*PAGE_SIZE)
#define MAX_CAMERA_FRAME_COUNT 4
#define MAX_CAMERA_MEM SZ_64M

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
	int total_mem = SZ_1G;
	int temp_mem = 0;
#if defined(CONFIG_MXC_AMD_GPU) || defined(CONFIG_MXC_AMD_GPU_MODULE)
	int gpu_mem = SZ_64M;
#endif

	mxc_set_cpu_type(MXC_CPU_MX53);
	get_cpu_wp = mx53_nitrogen_get_cpu_wp;
	set_num_cpu_wp = mx53_nitrogen_set_num_cpu_wp;

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
#endif

#ifdef HAVE_CAMERA
static struct mxc_camera_platform_data camera_data;
static void camera_pwdn(int pwdn)
{
//	pr_info("pwdn=%d camera_data.power_down=%x camera_data.reset=%x\n", pwdn, camera_data.power_down, camera_data.reset);
	gpio_set_value(camera_data.power_down, pwdn ? 1 : 0);
	gpio_set_value(camera_data.reset, (0==pwdn));
}

static struct mxc_camera_platform_data camera_data = {
	.io_regulator = "VDD_IO",
	.analog_regulator = "VDD_A",
	.mclk_name = "csi_mclk1",
	.mclk = 26000000,
	.csi = 0,
	.pwdn = camera_pwdn,
	.power_down = MAKE_GP(1, 2),	/* Nitrogen53 A/K override */
	.reset = CAMERA_RESET,
	.i2c_bus = 1,
	.i2c_id = 0x3c,
	.sensor_name = "ov5640",
};
#endif

#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
static void init_camera(void)
{
	struct clk *clk ;
	int i ;
	clk = clk_get(NULL,"csi_mclk1");
	if(clk){
		clk_set_rate(clk,24000000);
	} else
		printk(KERN_ERR "%s: Error getting CSI clock\n", __func__ );

#ifdef HAVE_CAMERA
	mxc_register_device(&boundary_camera_device, &camera_data);
	for (i = 0 ; i < ARRAY_SIZE(boundary_camera_interfaces); i++ ){
		mxc_register_device(&boundary_camera_interfaces[i], &camera_data);
	}
#endif
}
#endif

#if defined(CONFIG_DUMB_BATTERY) || defined (CONFIG_DUMB_BATTERY_MODULE)
static struct dumb_battery_platform_t dumb_plat = {
	.init_level = 30
,	.charge_sec = 60*60
,	.discharge_sec = 120*60
};

static struct platform_device dumb_battery_device = {
	.name   = "dumb_battery",
	.id     = -1,
	.dev    = {
		.platform_data  = &dumb_plat
	},
};
#endif

#if defined (CONFIG_DA905X_CHARDEV) || defined (CONFIG_DA905X_CHARDEV_MODULE)
static struct platform_device da905x_chardev_dev = {
	.name   = "da905x_chardev",
	.id     = -1,
	.dev    = {
		.platform_data  = 0
	},
};
#endif

#if defined(CONFIG_GPIO_OUTPUT) || defined (CONFIG_GPIO_OUTPUT_MODULE)
static struct platform_device gpio_output_pdev = {
       .name = "gpio_output",
};
#endif

#if defined (CONFIG_BOUNDARY_CAMERA_HDMI_IN) || defined (CONFIG_BOUNDARY_CAMERA_HDMI_IN_MODULE)
static struct mxc_camera_platform_data hdmi_camera_data = {
	.csi = 0,
	.sensor_name = "tfp410",
};
static struct platform_device hdmi_input = {
	.name = "tfp401",
	.dev    = {
		.platform_data  = &hdmi_camera_data
	},
};
#endif

static void __init mx53_nitrogen_io_init(void)
{
	/* MX53 Nitrogen board */
	if (gpio_request_array(nitrogen53_gpios, ARRAY_SIZE(nitrogen53_gpios))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	mxc_iomux_v3_setup_multiple_pads(mx53common_pads,
			ARRAY_SIZE(mx53common_pads));
	pr_info("MX53 Nitrogen board \n");
	mdelay(5);
	gpio_set_value(N53_USB_HUB_RESET, 1);		/* release USB Hub reset */
	gpio_set_value(N53_PHY_RESET, 1);		/* release ICS1893 Ethernet PHY reset */

#if defined(CONFIG_SERIAL_IMX_RS485)
	gpio_set_value(CONFIG_SERIAL_IMX_RS485_GPIO,CONFIG_SERIAL_IMX_RS485_GPIO_ACTIVE_HIGH^1);
#endif

}

#if defined(CONFIG_MACH_MX53_NITROGEN_K)
static unsigned char const poweroff_regs[] = {
	5, 0xff,	/* clear events */
	6, 0xff,
	7, 0xff,
	8, 0xff,
	9, 0xff,
	10, 0xff,	/* mask interrupts */
	11, 0xfe,	/* except nONKEY */
	12, 0xff,
	13, 0xff,
	15, 0x68,	/* power-off */
	0		/* end-of-list */
};
#endif

static void nitrogen_power_off(void)
{
#if defined(CONFIG_MACH_NITROGEN_A_IMX53)
#define POWER_DOWN	MAKE_GP(3, 23)
	gpio_set_value(POWER_DOWN, 0);
#elif defined(CONFIG_MACH_MX53_NITROGEN_K)
	unsigned char const *regs = poweroff_regs ;
	struct i2c_adapter *adap = i2c_get_adapter(0);
	union i2c_smbus_data data;
	int ret;
	BUG_ON (!adap);
	while (0 != *regs) {
		data.byte = regs[1];
		while (0 != (ret = i2c_smbus_xfer(adap, 0x48,0,
						  I2C_SMBUS_WRITE,*regs,
						  I2C_SMBUS_BYTE_DATA,&data))) {
			printk (KERN_ERR "%s: write error %d:%02x:0x%02x\n", __func__, ret, *regs, data.byte );
		}
		regs += 2 ;
	}
	while (0 != (ret = i2c_smbus_xfer(adap, 0x48,0,
					  I2C_SMBUS_READ,0x1,
					  I2C_SMBUS_BYTE_DATA,&data))) {
		printk (KERN_ERR "%s: read error %d:0x%02x\n", __func__, ret, data.byte );
	}
	printk (KERN_ERR "%s: read back data: %02x\n", __func__,data.byte);
#endif
	while (1) {
	}
}

static void nitrogen_suspend_enter(void)
{
	printk (KERN_ERR "%s\n", __func__ );
}

static void nitrogen_suspend_exit(void)
{
	printk (KERN_ERR "%s\n", __func__ );
}

static struct mxc_pm_platform_data nitrogen_pm_data = {
	.suspend_enter = nitrogen_suspend_enter,
	.suspend_exit = nitrogen_suspend_exit,
};


/*!
 * Board specific initialization.
 */
static void __init mxc_board_init(struct i2c_board_info *bi0, int bi0_size,
	struct i2c_board_info *bi1, int bi1_size,
	struct i2c_board_info *bi2, int bi2_size,
	unsigned da9052_irq, struct imxi2c_platform_data *i2c2_data)
{

	mxc_ipu_data.di_clk[0] = clk_get(NULL, "ipu_di0_clk");
	mxc_ipu_data.di_clk[1] = clk_get(NULL, "ipu_di1_clk");
	mxc_ipu_data.csi_clk[0] = clk_get(NULL, "csi_mclk1");
	mxc_spdif_data.spdif_core_clk = clk_get(NULL, "spdif_xtal_clk");
	clk_put(mxc_spdif_data.spdif_core_clk);

	/* SD card detect irqs */
	mxcsdhc1_device.resource[2].start = gpio_to_irq(N53_SD1_CD);
	mxcsdhc1_device.resource[2].end = gpio_to_irq(N53_SD1_CD);

	mxc_cpu_common_init();
	mx53_nitrogen_io_init();

	mxc_register_device(&mxc_dma_device, NULL);
	mxc_register_device(&mxc_wdt_device, NULL);
	mxc_register_device(&mxcspi1_device, &mxcspi1_data);
	mxc_register_device(&mxcspi2_device, &mxcspi2_data);

	mxc_register_device(&mxci2c_devices[0], &mxci2c0_data);
	mxc_register_device(&mxci2c_devices[1], &mxci2c1_data);
	mxc_register_device(&mxci2c_devices[2], i2c2_data);
	mxc_register_device(&mxc_rtc_device, NULL);

	mx53_nitrogen_init_da9052(da9052_irq);

	mxc_register_device(&mxc_w1_master_device, &mxc_w1_data);
	mxc_register_device(&mxc_ipu_device, &mxc_ipu_data);
	mxc_register_device(&mxc_ldb_device, &ldb_data);
	mxc_register_device(&mxc_tve_device, &tve_data);
	mxc_register_device(&mxcvpu_device, &mxc_vpu_data);
	mxc_register_device(&gpu_device, &gpu_data);
	mxc_register_device(&mxcscc_device, NULL);
        mxc_register_device(&pm_device, &nitrogen_pm_data);
	mxc_register_device(&mxc_dvfs_core_device, &dvfs_core_data);
	mxc_register_device(&busfreq_device, &bus_freq_data);
	mxc_register_device(&mxc_iim_device, &iim_data);
	mxc_register_device(&mxc_pwm1_device, &mxc_pwm1_platform_data);
	mxc_register_device(&mxc_pwm2_device, &mxc_pwm2_platform_data);
#if defined(CONFIG_MXC_PWM) && defined(CONFIG_BACKLIGHT_PWM)
	mxc_register_device(&mxc_pwm1_backlight_device,	&mxc_backlight_data_pwm2);
	mxc_register_device(&mxc_pwm2_backlight_device,	&mxc_backlight_data_pwm1);
#endif
	mxc_register_device(&mxcsdhc1_device, &mmc1_data);
	mxc_register_device(&mxcsdhc3_device, &mmc3_data);
	mxc_register_device(&mxc_ssi1_device, NULL);
	mxc_register_device(&mxc_ssi2_device, NULL);
	mxc_register_device(&mxc_alsa_spdif_device, &mxc_spdif_data);
	mxc_register_device(&mxc_android_pmem_device, &android_pmem_data);
	mxc_register_device(&mxc_android_pmem_gpu_device, &android_pmem_gpu_data);
	mxc_register_device(&usb_mass_storage_device, &mass_storage_data);
	mxc_register_device(&usb_rndis_device, &rndis_data);
	mxc_register_device(&android_usb_device, &android_usb_data);
	mxc_register_device(&ahci_fsl_device, &sata_data);
	mxc_register_device(&mxc_fec_device, &fec_data);
	mxc_register_device(&mxc_ptp_device, NULL);

	if (bi0)
		i2c_register_board_info(0, bi0, bi0_size);
	if (bi1)
		i2c_register_board_info(1, bi1, bi1_size);
	if (bi2)
		i2c_register_board_info(2, bi2, bi2_size);

#if defined(CONFIG_MTD)
	spi_register_board_info(mxc_spi_nor_device,
				ARRAY_SIZE(mxc_spi_nor_device));
#endif
	spi_register_board_info(spidev,
				ARRAY_SIZE(spidev));

	pm_power_off = nitrogen_power_off;

	mxc_register_device(&mxc_sgtl5000_device, &sgtl5000_data);
	mxc_register_device(&mxc_mlb_device, &mlb_data);
	mx5_usb_dr_init();
	mx5_set_host1_vbus_func(mx53_gpio_host1_driver_vbus);
	mx5_usbh1_init();
	mxc_register_device(&mxc_v4l2_device, NULL);
	mxc_register_device(&mxc_v4l2out_device, NULL);

#if defined(CONFIG_DUMB_BATTERY) || defined (CONFIG_DUMB_BATTERY_MODULE)
	platform_device_register(&dumb_battery_device);
#endif

#if defined (CONFIG_DA905X_CHARDEV) || defined (CONFIG_DA905X_CHARDEV_MODULE)
	platform_device_register(&da905x_chardev_dev);
#endif

#if defined(CONFIG_GPIO_OUTPUT) || defined (CONFIG_GPIO_OUTPUT_MODULE)
       platform_device_register(&gpio_output_pdev);
#endif

#ifdef CONFIG_KEYBOARD_GPIO
	platform_device_register(&gpio_keys_device);
#endif
#if defined(CONFIG_FB_MXC_PMIC_LCD_MODULE) || defined(CONFIG_FB_MXC_PMIC_LCD)
	platform_device_register(&lcd_pmic_device);
#endif

#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
	init_camera();
#endif

#if defined (CONFIG_BOUNDARY_CAMERA_HDMI_IN) || defined (CONFIG_BOUNDARY_CAMERA_HDMI_IN_MODULE)
	platform_device_register(&hdmi_input);
#endif
}

static void __init mx53_nitrogen_timer_init(void)
{
	mx53_clocks_init(32768, 24000000, 0, 0);
}

static struct sys_timer mxc_timer = {
	.init	= mx53_nitrogen_timer_init,
};

extern struct regulator_consumer_supply ldo8_consumers[];
extern struct regulator_consumer_supply ldo9_consumers[];

/*****************************************************************************/
#if defined(CONFIG_MACH_NITROGEN_A_IMX53)
struct gpio nitrogen53_gpios_specific_a[] __initdata = {
	{.label = "pmic-int",		.gpio = MAKE_GP(2, 21),		.flags = GPIOF_DIR_IN},
	{.label = "Camera power down",	.gpio = MAKE_GP(2, 22),		.flags = GPIOF_INIT_HIGH},	/* EIM_A16 */
//	{.label = "i2c2b sw-camera",		.gpio = MAKE_GP(3, 2),		.flags = GPIOF_INIT_LOW},	/* EIM_DA2 */
#define N53A_I2C_PIC16F616_INT				MAKE_GP(3, 8)
#if defined (CONFIG_TOUCHSCREEN_I2C) || defined (CONFIG_TOUCHSCREEN_I2C_MODULE)
	{.label = "i2c_pic_int",		.gpio = MAKE_GP(3, 8),		.flags = GPIOF_DIR_IN},		/* EIM_DA6 */
#endif
//	{.label = "i2c2a hub-PIC16F616_TOUCH",	.gpio = MAKE_GP(3, 7),		.flags = GPIOF_INIT_LOW},	/* EIM_DA7 */
//gpio3[9] - must always be high on early board rev, (empty i2c hub without pullups)
	{.label = "i2c2 empty hub",		.gpio = MAKE_GP(3, 9),		.flags = GPIOF_INIT_LOW},	/* EIM_DA9 */
//	{.label = "i2c2a hub-TFP410_ACCEL",	.gpio = MAKE_GP(3, 1),		.flags = GPIOF_INIT_LOW},	/* EIM_DA10 */
//	{.label = "led0",		.gpio = MAKE_GP(4, 2),		.flags = 0},
	{.label = "led1",		.gpio = MAKE_GP(4, 3),		.flags = 0},
//	{.label = "led2",		.gpio = MAKE_GP(4, 4),		.flags = 0},
	{.label = "eMMC reset",		.gpio = MAKE_GP(5, 2),		.flags = GPIOF_INIT_HIGH},	/* EIM_A25 */
#define N53A_OTG_VBUS				MAKE_GP(6, 6)
	{.label = "otg-vbus",		.gpio = MAKE_GP(6, 6),		.flags = 0},	/* disable VBUS */
//	{.label = "i2c3 sw-SC16IS7XX",		.gpio = MAKE_GP(6, 10),		.flags = GPIOF_INIT_LOW},	/* NANDF_RB0 */
//	{.label = "i2c2a hub-BATT_EDID",		.gpio = MAKE_GP(6, 11),		.flags = GPIOF_INIT_LOW},	/* NANDF_CS0 */
	{.label = "mic_mux",		.gpio = MAKE_GP(6, 16),		.flags = 0},
	{.label = "i2c-2-sda",		.gpio = MAKE_GP(7, 11),		.flags = GPIOF_DIR_IN},
	{.label = "power_down_req",	.gpio = POWER_DOWN,		.flags = GPIOF_INIT_HIGH},
};

extern struct da9052_tsi_platform_data da9052_tsi;

static struct plat_i2c_generic_data i2c_pic16f616_data = {
	.irq = gpio_to_irq(N53A_I2C_PIC16F616_INT),
	.gp = N53A_I2C_PIC16F616_INT,
};

struct sc16is7xx_gpio_platform_data i2c_sc16is7xx_gpio_data = {
	.gpio_base = -1,
	.irq_base = MXC_BOARD_IRQ_START,
};

static struct sc16is7xx_platform_data i2c_sc16is7xx_data = {
	.irq = gpio_to_irq(N53_SC16IS7XX_INT),
	.gp = N53_SC16IS7XX_INT,
	.gpio_data = &i2c_sc16is7xx_gpio_data,
};

static struct i2c_board_info mxc_i2c1_board_info_a[] __initdata = {
};

static struct i2c_board_info mxc_i2c2_board_info_a[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	},
};

static struct i2c_board_info n53a_i2c3_board_info[] __initdata = {
	{
	 .type = "Pic16F616-ts",
	 .addr = 0x22,
	 .platform_data  = &i2c_pic16f616_data,
	},
};

static struct i2c_board_info n53a_i2c4_board_info[] __initdata = {
#ifdef HAVE_CAMERA
	{
	 .type = "ov5642",
	 .addr = 0x3c,
	 .platform_data  = &camera_data,
	},
#endif
};

static struct i2c_board_info n53a_i2c5_board_info[] __initdata = {
	{
	 .type = "tfp410",
	 .addr = 0x38,
	 .platform_data  = &i2c_tfp410_data,
	},
	{
	 .type = "lsm303a",		/* Accelerometer */
	 .addr = 0x18,			//Nitrogen_AP will override this, so keep 1st in array
	 .platform_data  = &i2c_generic_data,
	},
	{
	 .type = "lsm303c",		/* Compass */
	 .addr = 0x1e,
	 .platform_data  = &i2c_generic_data,
	},
};

/* EDID is here too */
static struct i2c_board_info n53a_i2c6_board_info[] __initdata = {
	{
	 .type = "bq20z75",		/* Battery */
	 .addr = 0x0b,
	},
};

static struct i2c_board_info n53a_i2c7_board_info[] __initdata = {
	{
	 .type = "sc16is7xx-uart",
	 .addr = 0x49,
	 .platform_data  = &i2c_sc16is7xx_data,
	},
};

#endif

#ifdef CONFIG_MACH_NITROGEN_A_IMX53
/* i2c3-i2c6, Middle i2C bus has a switch */
static const unsigned n53a_i2c2_gpiomux_gpios[] = {
	/* i2c3- i2c6*/
	MAKE_GP(3, 7),		/* EIM_DA7 - PIC16F616_TOUCH */
	MAKE_GP(3, 10),		/* EIM_DA10 - Camera */
	MAKE_GP(3, 11),		/* EIM_DA11 - TFP410_ACCEL*/
	MAKE_GP(6, 11)		/* NANDF_CS0 - BATT_EDID */
};

static const unsigned n53a_i2c2_gpiomux_values[] = {
	1, 2, 4, 8
};

static struct gpio_i2cmux_platform_data n53a_i2c2_i2cmux_data = {
	.parent		= 1,
	.base_nr	= 3, /* optional */
	.values		= n53a_i2c2_gpiomux_values,
	.n_values	= ARRAY_SIZE(n53a_i2c2_gpiomux_values),
	.gpios		= n53a_i2c2_gpiomux_gpios,
	.n_gpios	= ARRAY_SIZE(n53a_i2c2_gpiomux_gpios),
	.idle		= 0,
};

static struct platform_device n53a_i2c2_i2cmux = {
        .name           = "gpio-i2cmux",
        .id             = 0,
        .dev            = {
                .platform_data  = &n53a_i2c2_i2cmux_data,
        },
};

/* i2c7, Last i2C bus has a buffer for UART */
static const unsigned n53a_i2c3_gpiomux_gpios[] = {
	/* i2c7*/
	MAKE_GP(6, 10)		/* NANDF_RB0 - SC16IS7XX */
};

static const unsigned n53a_i2c3_gpiomux_values[] = {
	1
};

static struct gpio_i2cmux_platform_data n53a_i2c3_i2cmux_data = {
	.parent		= 2,
	.base_nr	= 7, /* optional */
	.values		= n53a_i2c3_gpiomux_values,
	.n_values	= ARRAY_SIZE(n53a_i2c3_gpiomux_values),
	.gpios		= n53a_i2c3_gpiomux_gpios,
	.n_gpios	= ARRAY_SIZE(n53a_i2c3_gpiomux_gpios),
	.idle		= 0,
};

static struct platform_device n53a_i2c3_i2cmux = {
        .name           = "gpio-i2cmux",
        .id             = 1,
        .dev            = {
                .platform_data  = &n53a_i2c3_i2cmux_data,
        },
};

/* Nitrogen53A Pads */

static iomux_v3_cfg_t nitrogen53_pads_specific_a[] __initdata = {
	/* ECSPI2, Nitrogen53A only */
	MX53_PAD_EIM_CS1_CSPI2_MOSI,	/* Nitrogen uses as WL1271_irq */
	MX53_PAD_EIM_OE_CSPI2_MISO,	/* Nitrogen uses as VGA Hsync */
	MX53_PAD_EIM_LBA_CSPI2_CS2,
	MX53_PAD_EIM_CS0_CSPI2_SCLK,

	/* I2C3 */
	NEW_PAD_CTRL(MX53_PAD_GPIO_3__I2C3_SCL, MX53_I2C1_PAD_CTRL),	/* GPIO1[3] */
	NEW_PAD_CTRL(MX53_PAD_GPIO_16__I2C3_SDA, MX53_I2C1_PAD_CTRL),	/* gpio7[11] */

	/* Nitrogen uses the following pin for UART3, CTS, TXD, RXD */
	NEW_PAD_CTRL(MX53_PAD_EIM_D23__GPIO_3_23, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* board power down gpio */

	MX53_PAD_EIM_D24__UART3_TXD,
	MX53_PAD_EIM_D25__UART3_RXD,

	MX53_PAD_EIM_D30__UART3_CTS,
	NEW_PAD_CTRL(MX53_PAD_EIM_D31__GPIO_3_31, BUTTON_PAD_CTRL) | MUX_SION_MASK,     /* ??Menu */

	/* esdhc3 */
	MX53_PAD_ATA_DATA0__SD3_DAT4,
	MX53_PAD_ATA_DATA1__SD3_DAT5,
	MX53_PAD_ATA_DATA2__SD3_DAT6,
	MX53_PAD_ATA_DATA3__SD3_DAT7,

	/* esdhc4 */
	MX53_PAD_ATA_DA_1__SD4_CMD,
	MX53_PAD_ATA_DA_2__SD4_CLK,
	MX53_PAD_ATA_DATA12__SD4_DAT0,
	MX53_PAD_ATA_DATA13__SD4_DAT1,
	MX53_PAD_ATA_DATA14__SD4_DAT2,
	MX53_PAD_ATA_DATA15__SD4_DAT3,
	/* PWM1 backlight */
	MX53_PAD_GPIO_9__PWMO, /* pwm1 */

	/* i2c2 hub */
	MX53_PAD_EIM_DA7__GPIO3_7,	/* Pic16F616 Touch */
	MX53_PAD_EIM_DA9__GPIO_3_9,	/* SC16IS7XX */
//	MX53_PAD_EIM_DA11__GPIO_3_11,	/* TFP410, accelerometer */
//	MX53_PAD_NANDF_CS0__GPIO6_11_KEY, /* Battery/EDID */

	/* i2c3 switch */
	MX53_PAD_NANDF_RB0__GPIO6_10,	/* Camera buffer enable */

	/* Pic16F616 touch int */
	NEW_PAD_CTRL(MX53_PAD_EIM_DA8__GPIO_3_8, BUTTON_100K) | MUX_SION_MASK,
};

static int sdhc_write_protect4(struct device *dev)
{
	printk(KERN_ERR "%s\n", __func__ );
	return 0;
}

static struct mxc_mmc_platform_data mmc4_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_DATA_DDR,
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 1,
	.status = sdhc_get_card_det_true,
	.wp_status = sdhc_write_protect4,
};

static void mx53a_gpio_usbotg_driver_vbus(bool on)
{
	/* Enable OTG VBus with GPIO high */
	/* Disable OTG VBus with GPIO low */
	gpio_set_value(N53A_OTG_VBUS, on ? 1 : 0);
	pr_info("%s: on=%d\n", __func__, on);
}

static void n53a_i2c_clock_toggle2(void)
{
	const iomux_v3_cfg_t pd[] = {
		NEW_PAD_CTRL(MX53_PAD_GPIO_3__I2C3_SCL, MX53_I2C1_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_GPIO_3__GPIO_1_3, PAD_CTRL_9) | MUX_SION_MASK,
		NEW_PAD_CTRL(MX53_PAD_GPIO_16__I2C3_SDA, MX53_I2C1_PAD_CTRL),
		NEW_PAD_CTRL(MX53_PAD_GPIO_16__GPIO_7_11, PAD_CTRL_9) | MUX_SION_MASK,
	};
	i2c_clock_toggle(N53_I2C_2_SCL, N53_I2C_2_SDA_CURRENT, pd);
}

const char n53a_camera_i2c_dev_name[] = "8-003c";

static void __init mxc_board_init_nitrogen_a(void)
{
	unsigned da9052_irq = gpio_to_irq(MAKE_GP(2, 21));	/* pad EIM_A17 */
	ldo8_consumers[0].dev_name = n53a_camera_i2c_dev_name;
	ldo9_consumers[0].dev_name = n53a_camera_i2c_dev_name;
	mxci2c2_data.i2c_clock_toggle = n53a_i2c_clock_toggle2;

#ifdef HAVE_CAMERA
	camera_data.power_down = MAKE_GP(2, 22);
	camera_data.i2c_bus = 4;
#endif

	if (gpio_request_array(nitrogen53_gpios_specific_a,
			ARRAY_SIZE(nitrogen53_gpios_specific_a))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	mxc_iomux_v3_setup_multiple_pads(nitrogen53_pads_specific_a,
			ARRAY_SIZE(nitrogen53_pads_specific_a));
#ifdef CONFIG_KEYBOARD_GPIO
	gpio_keys[2].gpio = MAKE_GP(3,31);	/* new rev - menu key */
#endif
#if CONFIG_DA905X_TS_MODE == DA9052_5_WIRE_YXSXY_IO1
	da9052_tsi.config_index = DA9052_5_WIRE_XYSXY_IO2;
#endif

	mxc_board_init(NULL, 0,
		mxc_i2c1_board_info_a, ARRAY_SIZE(mxc_i2c1_board_info_a),
		mxc_i2c2_board_info_a, ARRAY_SIZE(mxc_i2c2_board_info_a),
		da9052_irq, &mxci2c2_data);
	mx5_set_otghost_vbus_func(mx53a_gpio_usbotg_driver_vbus);

	mxc_register_device(&mxcsdhc4_device, &mmc4_data);
	mxc_register_device(&n53a_i2c2_i2cmux, &n53a_i2c2_i2cmux_data);
	i2c_register_board_info(3, n53a_i2c3_board_info, ARRAY_SIZE(n53a_i2c3_board_info));
	i2c_register_board_info(4, n53a_i2c4_board_info, ARRAY_SIZE(n53a_i2c4_board_info));
	i2c_register_board_info(5, n53a_i2c5_board_info, ARRAY_SIZE(n53a_i2c5_board_info));
	i2c_register_board_info(6, n53a_i2c6_board_info, ARRAY_SIZE(n53a_i2c6_board_info));

	mxc_register_device(&n53a_i2c3_i2cmux, &n53a_i2c3_i2cmux_data);
	i2c_register_board_info(7, n53a_i2c7_board_info, ARRAY_SIZE(n53a_i2c7_board_info));
}

MACHINE_START(NITROGEN_A_IMX53, "Boundary Devices Nitrogen_A MX53 Board")
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.timer = &mxc_timer,
	.init_machine = mxc_board_init_nitrogen_a,
MACHINE_END
#endif

/*****************************************************************************/
extern struct platform_device mxc_uart_device3;
extern struct platform_device mxc_uart_device2;
static struct imxuart_platform_data uart_pdata = {
	.flags = IMXUART_HAVE_RTSCTS,
};

#ifdef CONFIG_MACH_NITROGEN_IMX53
struct gpio nitrogen53_gpios_specific[] __initdata = {
	{.label = "Camera power down",	.gpio = MAKE_GP(1, 2),		.flags = GPIOF_INIT_HIGH},
	{.label = "pmic-int",		.gpio = MAKE_GP(2, 21),		.flags = GPIOF_DIR_IN},
#ifdef CONFIG_WL12XX_PLATFORM_DATA
	{.label = "wl1271_btfunct5",	.gpio = MAKE_GP(1, 6),		.flags = GPIOF_DIR_IN},		/* GPIO_1_6 - (HOST_WU) */
#define N53_WL1271_INT				MAKE_GP(2, 24)
	{.label = "wl1271_int",		.gpio = MAKE_GP(2, 24),		.flags = GPIOF_DIR_IN},		/* EIM_CS1 */
#define N53_WL1271_WL_EN			MAKE_GP(3, 0)
	{.label = "wl1271_wl_en",	.gpio = MAKE_GP(3, 0),		.flags = 0},			/* EIM_DA0, high active */
#define N53_WL1271_BT_EN			MAKE_GP(3, 1)
	{.label = "wl1271_bt_en",	.gpio = MAKE_GP(3, 1),		.flags = 0},			/* EIM_DA1, high active */
#define N53_I2C_CONNECTOR_BUFFER_ENABLE		MAKE_GP(3, 10)
	{.label = "I2C conn. buf en",	.gpio = MAKE_GP(3, 10),		.flags = 0},			/* EIM_DA10 */
	{.label = "wl1271_btfunct2",	.gpio = MAKE_GP(5, 25),		.flags = 0},			/* CSIO0_D7, (BT_WU) */
#else
#define N53_AX8817X_RESET_CS0			MAKE_GP(2, 23)
	{.label = "AX8817X reset cs0",	.gpio = MAKE_GP(2, 23),		.flags = 0},			/* EIM_CS0 */
#define N53_AX8817X_RESET_CS1			MAKE_GP(2, 24)
	{.label = "AX8817X reset cs1",	.gpio = MAKE_GP(2, 24),		.flags = 0},			/* EIM_CS1 */
#endif

#define N53_SD3_CD				MAKE_GP(3, 11)
	{.label = "sdhc3-cd",		.gpio = MAKE_GP(3, 11),		.flags = GPIOF_DIR_IN},
#define N53_SD3_WP				MAKE_GP(3, 12)
	{.label = "sdhc3-wp",		.gpio = MAKE_GP(3, 12),		.flags = GPIOF_DIR_IN},
#define N53_OTG_VBUS				MAKE_GP(6, 6)
	{.label = "otg-vbus",		.gpio = MAKE_GP(6, 6),		.flags = 0},	/* disable VBUS */
	{.label = "i2c-2-sda",		.gpio = MAKE_GP(7, 11),		.flags = GPIOF_DIR_IN},
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
#if defined (CONFIG_TOUCHSCREEN_I2C) || defined (CONFIG_TOUCHSCREEN_I2C_MODULE)
	{
	 .type = "Pic16F616-ts",
	 .addr = 0x22,
	 .platform_data  = &i2c_generic_data,
	},
#endif
#if defined (CONFIG_TOUCHSCREEN_EP0700M01) || defined (CONFIG_TOUCHSCREEN_EP0700M01_MODULE)
	{
	 .type = "ep0700m01-ts",
	 .addr = 0x38,
	 .platform_data  = &i2c_generic_data,
	},
#endif
	{
	 .type = "tfp410",
	 .addr = 0x38,
	 .platform_data  = &i2c_tfp410_data,
	},
#ifdef HAVE_CAMERA
	{
	 .type = "ov5642",
	 .addr = 0x3c,
	 .platform_data  = &camera_data,
	},
#endif
};

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	},
};

static iomux_v3_cfg_t nitrogen53_pads_specific[] __initdata = {
	MX53_PAD_GPIO_6__GPIO_1_6,	/* BT_FUNCT5 */
	MX53_PAD_EIM_CS1__GPIO_2_24,	/* WL1271_irq, NitrogenA uses as ECSPI2 MOSI */
	MX53_PAD_EIM_OE__DI1_PIN7,	/* VGA HSync, NitrogenA uses as ECSPI2 MISO */
	MX53_PAD_EIM_CS0__GPIO_2_23,	/* Keyboard, NitrogenA uses as ECSPI2 SCLK */
	MX53_PAD_CSI0_D7__GPIO_5_25,	/* BT_FUNCT2 */

	/* I2C3 */
	MX53_PAD_GPIO_3__I2C3_SCL,	/* GPIO1[3] */
	MX53_PAD_GPIO_16__I2C3_SDA,	/* gpio7[11] */
	MX53_PAD_EIM_D30__GPIO_3_30,	/* On/Off key */

	/* esdhc3 */
	MX53_PAD_ATA_DATA0__SD3_DAT4,
	MX53_PAD_ATA_DATA1__SD3_DAT5,
	MX53_PAD_ATA_DATA2__SD3_DAT6,
	MX53_PAD_ATA_DATA3__SD3_DAT7,

	/* UART3  daughter board connector*/
#ifdef CONFIG_WL12XX_PLATFORM_DATA
	MX53_PAD_EIM_D24__UART3_TXD,
	MX53_PAD_EIM_D25__UART3_RXD,
	MX53_PAD_EIM_D23__UART3_CTS,
	MX53_PAD_EIM_D31__UART3_RTS,
#else
	/* UART3 for J3 4 pin connector*/
	MX53_PAD_ATA_CS_0__UART3_TXD,
	MX53_PAD_ATA_CS_1__UART3_RXD,
#endif
	/* PWM1 backlight */
	MX53_PAD_GPIO_9__PWMO, /* pwm1 */
};

#ifdef CONFIG_WL12XX_PLATFORM_DATA
static struct regulator_consumer_supply nitrogen53_vmmc2_supply =
	REGULATOR_SUPPLY("vmmc", "mxsdhci.2");

/* VMMC2 for driving the WL12xx module */
static struct regulator_init_data nitrogen53_vmmc2 = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies = &nitrogen53_vmmc2_supply,
};

static struct fixed_voltage_config nitrogen53_vwlan = {
	.supply_name		= "vwl1271",
	.microvolts		= 1800000, /* 1.80V */
	.gpio			= N53_WL1271_WL_EN,
	.startup_delay		= 70000, /* 70ms */
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &nitrogen53_vmmc2,
};

static struct platform_device nitrogen53_wlan_regulator = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data	= &nitrogen53_vwlan,
	},
};

struct wl12xx_platform_data nitrogen53_wlan_data __initdata = {
	.irq = gpio_to_irq(N53_WL1271_INT),
	.board_ref_clock = WL12XX_REFCLOCK_38, /* 38.4 MHz */
};

#endif

static void mx53_gpio_usbotg_driver_vbus(bool on)
{
	/* Enable OTG VBus with GPIO high */
	/* Disable OTG VBus with GPIO low */
	gpio_set_value(N53_OTG_VBUS, on ? 1 : 0);
	pr_info("%s: on=%d\n", __func__, on);
}

static unsigned int sdhc_get_card_det_status3(struct device *dev)
{
	return gpio_get_value(N53_SD3_CD);
}

static int sdhc_write_protect3(struct device *dev)
{
	return gpio_get_value(N53_SD3_WP);
}

static void __init mxc_board_init_nitrogen(void)
{
	unsigned da9052_irq = gpio_to_irq(MAKE_GP(2, 21));	/* pad EIM_A17 */
#ifdef CONFIG_WL12XX_PLATFORM_DATA
	mxc_uart_device3.dev.platform_data = &uart_pdata;
	/* PWM2 - GPIO_1 is slow clock */
	mxc_pwm2_platform_data.clk_select = PWM_CLK_HIGHPERF;
	mxc_backlight_data_pwm2.dft_brightness = 128;	/* slow clock 50% duty */

#else
	mmc3_data.status = sdhc_get_card_det_status3;
	mmc3_data.wp_status = sdhc_write_protect3;
	mxcsdhc3_device.resource[2].start = gpio_to_irq(N53_SD3_CD);
	mxcsdhc3_device.resource[2].end = gpio_to_irq(N53_SD3_CD);
	mmc3_data.card_inserted_state = 0;
	mmc3_data.power_mmc = 0;
	mmc3_data.caps &= ~(MMC_CAP_DATA_DDR | MMC_CAP_POWER_OFF_CARD);
#endif
#ifdef CONFIG_KEYBOARD_GPIO
	gpio_keys_platform_data.nbuttons = 4;
#endif
	if (gpio_request_array(nitrogen53_gpios_specific,
			ARRAY_SIZE(nitrogen53_gpios_specific))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	mxc_iomux_v3_setup_multiple_pads(nitrogen53_pads_specific,
			ARRAY_SIZE(nitrogen53_pads_specific));
	mxc_board_init(NULL, 0,
		mxc_i2c1_board_info, ARRAY_SIZE(mxc_i2c1_board_info),
		mxc_i2c2_board_info, ARRAY_SIZE(mxc_i2c2_board_info),
		da9052_irq, &mxci2c2_data);
	mx5_set_otghost_vbus_func(mx53_gpio_usbotg_driver_vbus);

#if defined (CONFIG_TOUCHSCREEN_I2C) || defined (CONFIG_TOUCHSCREEN_I2C_MODULE) \
 ||  defined (CONFIG_TOUCHSCREEN_EP0700M01) || defined (CONFIG_TOUCHSCREEN_EP0700M01_MODULE)
#ifdef N53_I2C_CONNECTOR_BUFFER_ENABLE
	gpio_set_value(N53_I2C_CONNECTOR_BUFFER_ENABLE,1);
	gpio_export(N53_I2C_CONNECTOR_BUFFER_ENABLE, 0);
#endif
#endif

#ifdef CONFIG_WL12XX_PLATFORM_DATA
	/* WL12xx WLAN Init */
	if (wl12xx_set_platform_data(&nitrogen53_wlan_data))
		pr_err("error setting wl12xx data\n");
#if 1
	platform_device_register(&nitrogen53_wlan_regulator);
#else
	gpio_direction_output(N53_WL1271_INT, 0);		/* WL1271_irq */
	mxc_iomux_v3_setup_pad(MX53_PAD_ATA_IORDY__GPIO_7_5);			/* SD3_CLK */
	gpio_direction_output(MAKE_GP(7, 5), 0);
#endif

	gpio_set_value(N53_WL1271_WL_EN, 1);		/* momentarily enable */
	gpio_set_value(N53_WL1271_BT_EN, 1);
	mdelay(2);
	gpio_set_value(N53_WL1271_WL_EN, 0);
	gpio_set_value(N53_WL1271_BT_EN, 0);
	gpio_free(N53_WL1271_WL_EN);
	gpio_free(N53_WL1271_BT_EN);
	mdelay(1);
#endif
#ifdef N53_AX8817X_RESET_CS0
	gpio_set_value(N53_AX8817X_RESET_CS0, 1);	/* Enable usb ethernet */
#endif
#ifdef N53_AX8817X_RESET_CS1
	gpio_set_value(N53_AX8817X_RESET_CS1, 1);	/* Enable usb ethernet */
#endif
}

MACHINE_START(NITROGEN_IMX53, "Boundary Devices Nitrogen MX53 Board")
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.timer = &mxc_timer,
	.init_machine = mxc_board_init_nitrogen,
MACHINE_END
#endif

/*****************************************************************************/
#ifdef CONFIG_MACH_MX53_NITROGEN_K

#if defined(CONFIG_TOUCHSCREEN_EKTF2K) || defined(CONFIG_TOUCHSCREEN_EKTF2K_MODULE)
static int elan_ktf2k_ts_power(int on)
{
	pr_info("%s: power %d\n", __func__, on);
	return 0;
}

struct elan_ktf2k_i2c_platform_data ts_elan_ktf2k_data[] = {
	{
	.version = 0x0001,
	.abs_x_min = 11,
	.abs_x_max = 2111,
	.abs_y_min = 24,
	.abs_y_max = 1255,
	.intr_gpio = N53_I2C_CONNECTOR_INT,
	.power = elan_ktf2k_ts_power,
	},
};
#endif

#define CONFIG_K2

struct gpio n53k_gpios_specific[] __initdata = {
	{.label = "pmic-int",		.gpio = MAKE_GP(2, 21),		.flags = GPIOF_DIR_IN},
	{.label = "Dispay w3 sda",	.gpio = MAKE_GP(2, 19),		.flags = GPIOF_INIT_HIGH},
	{.label = "Dispay w3 scl",	.gpio = MAKE_GP(2, 20),		.flags = GPIOF_INIT_HIGH},
//	{.label = "i2c touchscreen int", .gpio = MAKE_GP(6, 6),		.flags = GPIOF_DIR_IN},
#ifdef CONFIG_K2
#define	N53K_HEADPHONE_DET			MAKE_GP(1, 2)
	{.label = "headphone detect",	.gpio = MAKE_GP(1, 2),		.flags = GPIOF_DIR_IN},
	{.label = "Display power",	.gpio = MAKE_GP(2, 16),		.flags = GPIOF_INIT_HIGH},
#define N53K_CAMERA_POWER_DOWN			MAKE_GP(2, 22)
	{.label = "Camera power down",	.gpio = MAKE_GP(2, 22),		.flags = GPIOF_INIT_HIGH},
	{.label = "accelerometer int1",	.gpio = MAKE_GP(2, 23),		.flags = GPIOF_DIR_IN},
	{.label = "accelerometer int2",	.gpio = MAKE_GP(2, 24),		.flags = GPIOF_DIR_IN},
	{.label = "da9053 fault",	.gpio = MAKE_GP(2, 28),		.flags = GPIOF_DIR_IN},
	{.label = "uart1 cts",		.gpio = MAKE_GP(2, 31),		.flags = GPIOF_INIT_LOW},
	{.label = "Dispay w3 cs",	.gpio = MAKE_GP(3, 3),		.flags = GPIOF_INIT_HIGH},
	{.label = "da9053 shutdown",	.gpio = MAKE_GP(3, 4),		.flags = GPIOF_INIT_HIGH},
	{.label = "ambient int",	.gpio = MAKE_GP(3, 7),		.flags = GPIOF_INIT_HIGH},
#define I2C2_HUB_EDID				MAKE_GP(3, 8)
//	{.label = "i2c2 hub-EDID",	.gpio = MAKE_GP(3, 8),		.flags = GPIOF_INIT_LOW},
#define I2C2_HUB_BQ24163			MAKE_GP(3, 9)
//	{.label = "i2c2 hub-bq2416x",	.gpio = MAKE_GP(3, 9),		.flags = GPIOF_INIT_LOW},
#define I2C2_HUB_AMBIENT			MAKE_GP(3, 10)
//	{.label = "i2c2 hub-ambient",	.gpio = MAKE_GP(3, 10),		.flags = GPIOF_INIT_LOW},
	{.label = "barcode scan power",	.gpio = MAKE_GP(3, 23),		.flags = GPIOF_INIT_HIGH},
#define N53K_BQ24163_INT			MAKE_GP(4, 2)
	{.label = "bq2416x interrupt",	.gpio = MAKE_GP(4, 2),		.flags = GPIOF_DIR_IN},
	{.label = "i2c touchscreen reset", .gpio = MAKE_GP(5, 2),	.flags = GPIOF_INIT_HIGH},
#define I2C2_HUB_CAMERA				MAKE_GP(6, 10)
//	{.label = "i2c2 hub-Camera",	.gpio = MAKE_GP(6, 10),		.flags = GPIOF_INIT_LOW},
	{.label = "main power",		.gpio = MAKE_GP(6, 12),		.flags = GPIOF_INIT_HIGH},
	{.label = "eMMC reset",		.gpio = MAKE_GP(7, 10),		.flags = GPIOF_INIT_HIGH},	/* PATA_CS_1 - active low reset */
//The gpio_keys.c file will request these, they are here for documentation only
//	{.label = "Menu key",		.gpio = MAKE_GP(1, 4),		.flags = GPIOF_DIR_IN},
//	{.label = "Search key",		.gpio = MAKE_GP(3, 27),		.flags = GPIOF_DIR_IN},
//	{.label = "Home key",		.gpio = MAKE_GP(3, 29),		.flags = GPIOF_DIR_IN},
//	{.label = "Volume down key",	.gpio = MAKE_GP(6, 11),		.flags = GPIOF_DIR_IN},
//	{.label = "Volume up key",	.gpio = MAKE_GP(6, 14),		.flags = GPIOF_DIR_IN},
//	{.label = "Back key",		.gpio = MAKE_GP(6, 16),		.flags = GPIOF_DIR_IN},
#else
	{.label = "eMMC reset",		.gpio = MAKE_GP(3, 8),		.flags = GPIOF_INIT_HIGH},	/* EIM_DA8, GPIO3[8] - active low reset */
	{.label = "Dispay w3 cs",	.gpio = MAKE_GP(3, 24),		.flags = GPIOF_INIT_HIGH},
#define N53K_CAMERA_POWER_DOWN			MAKE_GP(1, 2)
	{.label = "Camera power down",	.gpio = MAKE_GP(1, 2),		.flags = GPIOF_INIT_HIGH},
#define N53K_I2C_CONNECTOR_BUFFER_ENABLE	MAKE_GP(3, 10)
	{.label = "I2C conn. buf en",	.gpio = MAKE_GP(3, 10),		.flags = 0},			/* EIM_DA10 */
#endif

#ifdef CONFIG_WL12XX_PLATFORM_DATA
#ifdef CONFIG_K2
	{.label = "wl1271_btfunct5",	.gpio = MAKE_GP(2, 0),		.flags = GPIOF_DIR_IN},	/* PATA_DATA0 - (HOST_WU) */
#define N53K_WL1271_INT				MAKE_GP(2, 1)
	{.label = "wl1271_int",		.gpio = MAKE_GP(2, 1),		.flags = GPIOF_DIR_IN},	/* PATA_DATA1 */
#define N53K_WL1271_BT_EN			MAKE_GP(2, 2)
	{.label = "wl1271_bt_en",	.gpio = MAKE_GP(2, 2),		.flags = 0},		/* PATA_DATA2, high active */
#define N53K_WL1271_WL_EN			MAKE_GP(2, 3)
	{.label = "wl1271_wl_en",	.gpio = MAKE_GP(2, 3),		.flags = 0},		/* PATA_DATA3, high active */
#else
//	{.label = "wl1271_btfunct2",	.gpio = MAKE_GP(1, 9),		.flags = 0},		/* GPIO_9, (BT_WU) */
#define N53K_WL1271_WL_EN			MAKE_GP(3, 0)
	{.label = "wl1271_wl_en",	.gpio = MAKE_GP(3, 0),		.flags = 0},		/* EIM_DA0, high active */
#define N53K_WL1271_BT_EN			MAKE_GP(3, 1)
	{.label = "wl1271_bt_en",	.gpio = MAKE_GP(3, 1),		.flags = 0},		/* EIM_DA1, high active */
	{.label = "wl1271_btfunct5",	.gpio = MAKE_GP(3, 9),		.flags = GPIOF_DIR_IN},	/* EIM_DA9, GPIO3[9] - (HOST_WU) */
#define N53K_WL1271_INT				MAKE_GP(7, 9)
	{.label = "wl1271_int",		.gpio = MAKE_GP(7, 9),		.flags = GPIOF_DIR_IN},	/* PATA_CS_0 */
#endif
#endif
	{.label = "i2c-2-sda",		.gpio = MAKE_GP(7, 11),		.flags = GPIOF_DIR_IN},
	{.label = "USBH1 Power",	.gpio = MAKE_GP(2, 17),		.flags = GPIOF_INIT_HIGH},	/* EIM_A21, active high power enable */
#if !defined(CONFIG_GPIO_OUTPUT) && !defined(CONFIG_GPIO_OUTPUT_MODULE)
	{.label = "barcode scan trig",	.gpio = MAKE_GP(3, 30),		.flags = GPIOF_INIT_LOW},	/* EIM_D30, active high scanner trigger */
	{.label = "Led1",		.gpio = MAKE_GP(5, 4),		.flags = GPIOF_INIT_HIGH},
	{.label = "Led2",		.gpio = MAKE_GP(3, 31),		.flags = GPIOF_INIT_HIGH},	/* EIM_D31, active low, Yellow LED */
	{.label = "Led3",		.gpio = MAKE_GP(3, 22),		.flags = GPIOF_INIT_HIGH},	/* EIM_D22, active low, Blue LED */
	{.label = "Led4",		.gpio = MAKE_GP(2, 18),		.flags = GPIOF_INIT_HIGH},	/* EIM_A20, active low, Orange LED */
	{.label = "Camera flash",	.gpio = MAKE_GP(3, 2),		.flags = GPIOF_INIT_LOW},	/* EIM_DA2, active high, camera flash */
#endif
};

/* Middle i2C bus has a switch */
static const unsigned i2c2_gpiomux_gpios[] = {
	/* i2c3- i2c6*/
	I2C2_HUB_EDID, I2C2_HUB_BQ24163, I2C2_HUB_AMBIENT, I2C2_HUB_CAMERA
};

static const unsigned i2c2_gpiomux_values[] = {
	1, 0, 4, 8
};

static struct gpio_i2cmux_platform_data i2c2_i2cmux_data = {
	.parent		= 1,
	.base_nr	= 3, /* optional */
	.values		= i2c2_gpiomux_values,
	.n_values	= ARRAY_SIZE(i2c2_gpiomux_values),
	.gpios		= i2c2_gpiomux_gpios,
	.n_gpios	= ARRAY_SIZE(i2c2_gpiomux_gpios),
	.idle		= 0,
};

static struct platform_device i2c2_i2cmux = {
        .name           = "gpio-i2cmux",
        .id             = 0,
        .dev            = {
                .platform_data  = &i2c2_i2cmux_data,
        },
};


#ifdef CONFIG_K2
/* Only pmic da9052 on this bus */
static struct i2c_board_info n53k_i2c0_board_info[] __initdata = {
};

/* Nothing directly connected to this bus, goes through switch */
static struct i2c_board_info n53k_i2c1_board_info[] __initdata = {
};

/* edid */
static struct i2c_board_info n53k_i2c3_board_info[] __initdata = {
};

static struct plat_i2c_bq2416x_data i2c_bq2416x_data = {
	.gp = N53K_BQ24163_INT,
	.policy = {
		.charge_mV = 4200,			/* 3500mV - 4440mV, default 3600mV, charge voltage  */
		.charge_current_limit_mA = 1000,	/* 550mA - 2875mA, default 1000mA */
		.terminate_current_mA = 150,		/* 50mA - 400mA, default 150mA, Stop charging when current drops to this */
		.usb_voltage_limit_mV = 4200,		/* 4200mV - 47600mV, default 4200mV, reduce charge current to stay above this */
		.usb_current_limit_mA = 100,		/* 100mA - 1500mA, default 100mA */
		.in_voltage_limit_mV = 4200,		/* 4200mV - 47600mV, default 4200mV,  reduce charge current to stay above this */
		.in_current_limit_mA = 2500,		/* 1500mA or 2500mA, default 1500mA */
		.charge_timeout_minutes = 6*60,		/* 27 minute, 6 hours, 9 hours, infinite are choices, default 27 minutes */
		.otg_lock = 0,
		.prefer_usb_charging = 0,
		.disable_charging = 0,
	}
};

static struct i2c_board_info n53k_i2c4_board_info[] __initdata = {
	{
	 .type = "bq27200",
	 .addr = 0x55,
	},
	{
	 .type = "bq2416x",
	 .addr = 0x6b,
	 .platform_data  = &i2c_bq2416x_data,
	},
};

/* Ambient light sensor */
static struct i2c_board_info n53k_i2c5_board_info[] __initdata = {
	{
	 .type = "apds9300",
	 .addr = 0x39,	/* addr_sel floating */
	},
};

static struct i2c_board_info n53k_i2c6_board_info[] __initdata = {
#ifdef HAVE_CAMERA
	{
	 .type = "ov5642",
	 .addr = 0x3c,
	 .platform_data  = &camera_data,
	},
#endif
};
#else

static struct i2c_board_info n53k_i2c0_board_info[] __initdata = {
	{
	 .type = "bq27200",
	 .addr = 0x55,
	 .platform_data  = &i2c_generic_data,
	},
};
static struct i2c_board_info n53k_i2c1_board_info[] __initdata = {
#ifdef HAVE_CAMERA
	{
	 .type = "ov5642",
	 .addr = 0x3c,
	 .platform_data  = &camera_data,
	},
#endif
	{
	 .type = "mma8451",
	 .addr = 0x1c,
	},
	{
	 .type = "tfp410",
	 .addr = 0x38,
	 .platform_data  = &i2c_tfp410_data,
	},
};
#endif



#if defined (CONFIG_TOUCHSCREEN_ATMEL_MXT) || defined (CONFIG_TOUCHSCREEN_ATMEL_MXT_MODULE)
static struct mxt_platform_data mxt224_data = {
       .x_line         = 18,
       .y_line         = 12,
       .x_size         = 1024,
       .y_size         = 1024,
       .blen           = 0x1,
       .threshold      = 0x28,
       .voltage        = 2800000,              /* 2.8V */
       .orient         = MXT_VERTICAL_FLIP,
       .irqflags       = IRQF_TRIGGER_FALLING,
};
#endif

static struct i2c_board_info n53k_i2c2_board_info[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	},
#if defined (CONFIG_TOUCHSCREEN_ATMEL_MXT) || defined (CONFIG_TOUCHSCREEN_ATMEL_MXT_MODULE)
	{
	 .type = "mXT224",
	 .addr = 0x4b,
	 .platform_data  = &mxt224_data,
	 .irq = gpio_to_irq(N53_I2C_CONNECTOR_INT),	/* EIM_A23 */
	},
#endif
#if defined(CONFIG_TOUCHSCREEN_EKTF2K) || defined(CONFIG_TOUCHSCREEN_EKTF2K_MODULE)
	{
	 I2C_BOARD_INFO(ELAN_KTF2K_NAME, 0x10),
	 .platform_data = &ts_elan_ktf2k_data,
	 .irq = gpio_to_irq(N53_I2C_CONNECTOR_INT),	/* EIM_A23 */
	},
#endif
#ifdef CONFIG_K2
	{
	 .type = "mma8451",
	 .addr = 0x1c,
	},
	{
	 .type = "tfp410",
	 .addr = 0x38,
	 .platform_data  = &i2c_tfp410_data,
	},
#else
	{
	 .type = "bq27200",
	 .addr = 0x55,
	},
#endif
};

static iomux_v3_cfg_t n53k_pads_specific[] __initdata = {
	/* Audmux */
	MX53_PAD_CSI0_DAT7__AUDMUX_AUD3_RXD,

	/* esdhc4 */
	MX53_PAD_ATA_DA_1__SD4_CMD,
	MX53_PAD_ATA_DA_2__SD4_CLK,
	MX53_PAD_ATA_DATA12__SD4_DAT0,
	MX53_PAD_ATA_DATA13__SD4_DAT1,
	MX53_PAD_ATA_DATA14__SD4_DAT2,
	MX53_PAD_ATA_DATA15__SD4_DAT3,
	MX53_PAD_ATA_DATA4__SD4_DAT4,
	MX53_PAD_ATA_DATA5__SD4_DAT5,
	MX53_PAD_ATA_DATA6__SD4_DAT6,
	MX53_PAD_ATA_DATA7__SD4_DAT7,

#ifdef CONFIG_K2
	/* TiWi */
	MX53_PAD_PATA_DATA0__GPIO2_0,	/* BT_FUNCT5 */
	NEW_PAD_CTRL(MX53_PAD_PATA_DATA1__GPIO2_1, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* WL1271_irq */
	MX53_PAD_PATA_DATA2__GPIO2_2,	/* BT_EN */
	MX53_PAD_PATA_DATA3__GPIO2_3,	/* WL_EN */
	/* i2c2 hub */
	MX53_PAD_EIM_DA8__GPIO_3_8,	/* i2c2 hub - EDID */
	MX53_PAD_EIM_DA9__GPIO_3_9,	/* i2c2 hub - BQ24163 enable */
	MX53_PAD_GPIO_12__GPIO4_2,	/* BQ24163 int/status */
	MX53_PAD_ATA_CS_0__GPIO_7_9,	/* accelerometer int1 on i2c3 */
	MX53_PAD_EIM_DA3__GPIO3_3,	/* Dispay w3 cs */
	/* UART3 */
	MX53_PAD_EIM_D24__UART3_TXD,
	MX53_PAD_EIM_D25__UART3_RXD,
#else
	/* TiWi */
	MX53_PAD_EIM_DA9__GPIO_3_9,	/* BT_FUNCT5 */
	MX53_PAD_ATA_CS_0__GPIO_7_9,	/* WL1271_irq */
	MX53_PAD_EIM_D24__GPIO3_24,	/* Dispay w3 cs */
	/* UART3 */
	MX53_PAD_ATA_CS_0__UART3_TXD,
	MX53_PAD_ATA_CS_1__UART3_RXD,
	MX53_PAD_EIM_DA8__GPIO_3_8,	/* EMMC Reset */
#endif
	MX53_PAD_GPIO_9__PWMO,		/* pwm1 */
	MX53_PAD_GPIO_10__OSC32K_32K_OUT,	/* Slow clock */

	/* I2C3 */
	MX53_PAD_GPIO_3__I2C3_SCL,	/* GPIO1[3] */
	MX53_PAD_GPIO_16__I2C3_SDA,	/* gpio7[11] */
	NEW_PAD_CTRL(MX53_PAD_EIM_D23__GPIO_3_23, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* barcode scan power */
	MX53_PAD_EIM_D30__GPIO_3_30,	/* Barcode scanner trigger */

	/* LED daughter board */
	MX53_PAD_EIM_A20__GPIO_2_18,	/* Led4 */
	MX53_PAD_EIM_D22__GPIO_3_22,	/* Led3 */
	NEW_PAD_CTRL(MX53_PAD_EIM_D31__GPIO_3_31, BUTTON_PAD_CTRL) | MUX_SION_MASK,     /* Led2 */

	/* Camera daughter board */
	MX53_PAD_GPIO_7__GPIO1_7,		/* strobe input (from camera) */
	MX53_PAD_EIM_DA2__GPIO3_2,		/* flash output (active high) */
	MX53_PAD_EIM_CS0__GPIO_2_23,		/* accelerometer int1 */
	MX53_PAD_EIM_CS1__GPIO_2_24,		/* accelerometer int2 */
	NEW_PAD_CTRL(MX53_PAD_EIM_EB0__GPIO2_28, BUTTON_PAD_CTRL) | MUX_SION_MASK,     /* da9053 fault */
	MX53_PAD_EIM_EB3__GPIO2_31,		/* uart1 cts */
	MX53_PAD_EIM_DA4__GPIO3_4,		/* da9053 shutdown */
	MX53_PAD_EIM_DA7__GPIO3_7,		/* ambient int */
	MX53_PAD_NANDF_RB0__GPIO6_10,		/* i2c2 hub-Camera */
	MX53_PAD_NANDF_WE_B__GPIO6_12,		/* main power */
	MX53_PAD_PATA_CS_1__GPIO7_10,		/* eMMC reset */
};

#ifdef CONFIG_WL12XX_PLATFORM_DATA
static struct regulator_consumer_supply n53k_vmmc2_supply =
REGULATOR_SUPPLY("vmmc", "mxsdhci.2");

/* VMMC2 for driving the WL12xx module */
static struct regulator_init_data n53k_vmmc2 = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &n53k_vmmc2_supply,
};

static struct fixed_voltage_config n53k_vwlan = {
	.supply_name = "vwl1271",
	.microvolts = 1800000, /* 1.80V */
	.gpio = N53K_WL1271_WL_EN,
	.startup_delay = 70000, /* 70ms */
	.enable_high = 1,
	.enabled_at_boot = 0,
	.init_data = &n53k_vmmc2,
};

static struct platform_device n53k_wlan_regulator = {
	.name = "reg-fixed-voltage",
	.id = 1,
	.dev = {
		.platform_data  = &n53k_vwlan,
	},
};

struct wl12xx_platform_data n53k_wlan_data __initdata = {
	.irq = gpio_to_irq(N53K_WL1271_INT),
	.board_ref_clock = WL12XX_REFCLOCK_38,		/* 38.4 MHz */
};

#endif

static int n53k_sdhc_write_protect4(struct device *dev)
{
	printk(KERN_ERR "%s\n", __func__ );
	return 0;
}

static struct mxc_mmc_platform_data n53k_mmc4_data = {
	.ocr_mask = MMC_VDD_165_195,
	.caps = MMC_CAP_MMC_HIGHSPEED | MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,	// MMC_CAP_DATA_DDR causes errors
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 1,
	.status = sdhc_get_card_det_true,
	.wp_status = n53k_sdhc_write_protect4,
};

static int headphone_det_status_k(void)
{
	gpio_get_value(N53K_HEADPHONE_DET);
	return 0;
}

#ifdef CONFIG_K2
const char n53k_camera_i2c_dev_name[] = "6-003c";
#endif

static void __init n53k_board_init(void)
{
	unsigned da9052_irq = gpio_to_irq(MAKE_GP(2, 21));	/* pad EIM_A17 */
#ifdef CONFIG_K2
#ifdef HAVE_CAMERA
	camera_data.power_down = N53K_CAMERA_POWER_DOWN;
#endif
	sgtl5000_data.hp_status = headphone_det_status_k;
	sgtl5000_data.hp_irq = gpio_to_irq(N53K_HEADPHONE_DET);
	ldo8_consumers[0].dev_name = n53k_camera_i2c_dev_name;
	ldo9_consumers[0].dev_name = n53k_camera_i2c_dev_name;
#else
	/* PWM1 - GPIO_9 is slow clock */
	mxc_pwm1_platform_data.clk_select = PWM_CLK_HIGHPERF;
	mxc_backlight_data_pwm1.dft_brightness = 128;	/* slow clock 50% duty */
#endif

	mxc_uart_device2.dev.platform_data = &uart_pdata;

	if (gpio_request_array(n53k_gpios_specific,
			ARRAY_SIZE(n53k_gpios_specific))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	mxc_iomux_v3_setup_multiple_pads(n53k_pads_specific,
			ARRAY_SIZE(n53k_pads_specific));
	mxc_board_init(
		n53k_i2c0_board_info, ARRAY_SIZE(n53k_i2c0_board_info),
		n53k_i2c1_board_info, ARRAY_SIZE(n53k_i2c1_board_info),
		n53k_i2c2_board_info, ARRAY_SIZE(n53k_i2c2_board_info),
		da9052_irq, &mxci2c2_data);

#ifdef CONFIG_K2
	mxc_register_device(&i2c2_i2cmux, &i2c2_i2cmux_data);
	i2c_register_board_info(3, n53k_i2c3_board_info, ARRAY_SIZE(n53k_i2c3_board_info));
	i2c_register_board_info(4, n53k_i2c4_board_info, ARRAY_SIZE(n53k_i2c4_board_info));
	i2c_register_board_info(5, n53k_i2c5_board_info, ARRAY_SIZE(n53k_i2c5_board_info));
	i2c_register_board_info(6, n53k_i2c6_board_info, ARRAY_SIZE(n53k_i2c6_board_info));
#endif

	/* eMMC is sdhc4 */
	mxc_register_device(&mxcsdhc4_device, &n53k_mmc4_data);
#ifdef CONFIG_WL12XX_PLATFORM_DATA
	/* WL12xx WLAN Init */
	if (wl12xx_set_platform_data(&n53k_wlan_data))
		pr_err("error setting wl12xx data\n");
	platform_device_register(&n53k_wlan_regulator);

	gpio_set_value(N53K_WL1271_WL_EN, 1);		/* momentarily enable */
	gpio_set_value(N53K_WL1271_BT_EN, 1);
	mdelay(2);
	gpio_set_value(N53K_WL1271_WL_EN, 0);
	gpio_set_value(N53K_WL1271_BT_EN, 0);
	gpio_free(N53K_WL1271_WL_EN);
	gpio_free(N53K_WL1271_BT_EN);
	mdelay(1);
#endif
}

MACHINE_START(MX53_NITROGEN_K, "Boundary Devices MX53 Nitrogen_K Board")
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.timer = &mxc_timer,
	.init_machine = n53k_board_init,
MACHINE_END
#endif
