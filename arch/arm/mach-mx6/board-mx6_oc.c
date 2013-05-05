/*
 * Copyright (C) 2011-2012 Freescale Semiconductor, Inc. All Rights Reserved.
 *
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

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/nodemask.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>
#include <linux/ata.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/regulator/consumer.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/fec.h>
#include <linux/memblock.h>
#include <linux/gpio.h>
#include <linux/gpio-i2cmux.h>
#include <linux/etherdevice.h>
#include <linux/regulator/anatop-regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/mxc_dvfs.h>
#include <mach/memory.h>
#include <mach/iomux-mx6q.h>
#include <mach/iomux-mx6dl.h>
#include <mach/imx-uart.h>
#include <mach/viv_gpu.h>
#include <mach/ahci_sata.h>
#include <mach/ipu-v3.h>
#include <mach/mxc_hdmi.h>
#include <mach/mxc_asrc.h>
#include <linux/i2c/tsc2007.h>
#include <linux/wl12xx.h>

#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "usb.h"
#include "devices-imx6q.h"
#include "crm_regs.h"
#include "cpu_op-mx6.h"

#define MX6_OC_AUDIO_MUTE		IMX_GPIO_NR(5, 2)	/* EIM_A25 - active low */

#define MX6_OC_CAMERA_I2C_EN		IMX_GPIO_NR(3, 20)	/* EIM_D20 - active high */
#define MX6_OC_CAMERA_PWDN		IMX_GPIO_NR(1, 16)	/* SD1_DAT0 - active high */
#define MX6_OC_CAMERA_GP		IMX_GPIO_NR(1, 6)	/* GPIO_6  */
#define MX6_OC_CAMERA_RESET		IMX_GPIO_NR(1, 8)	/* GPIO_8 - active low */

#define MX6_OC_DISP_I2C_EN		IMX_GPIO_NR(2, 17)	/* EIM_A21 - active high */
#define MX6_OC_DISP_BACKLIGHT_EN	IMX_GPIO_NR(1, 17)	/* SD1_DAT1 - active high */
#define MX6_OC_DISP_BACKLIGHT_12V_EN	IMX_GPIO_NR(4, 5)	/* GPIO_19 - active high */

#define MX6_OC_ENET_PHY_RESET		IMX_GPIO_NR(1, 27)	/* ENET_RXD0 - active low */
#define MX6_OC_ENET_PHY_IRQ		IMX_GPIO_NR(1, 28)	/* ENET_TX_EN - active low */

#define MX6_OC_PCIE_RESET		IMX_GPIO_NR(3, 22)	/* EIM_CS0 - active low */

#define MX6_OC_RTC_I2C_EN		IMX_GPIO_NR(2, 23)	/* EIM_CS0 - active high */
#define MX6_OC_RTC_IRQ			IMX_GPIO_NR(2, 24)	/* EIM_CS1 - active low */

#define MX6_OC_ST_EMMC_RESET		IMX_GPIO_NR(2, 5)	/* NANDF_D5 - active low */
#define MX6_OC_ST_SD3_CD		IMX_GPIO_NR(7, 0)	/* SD3_DAT5 - active low */
#define MX6_OC_ST_ECSPI1_CS1		IMX_GPIO_NR(3, 19)	/* EIM_D19 - active low */

#define MX6_OC_TOUCH_RESET		IMX_GPIO_NR(1, 4)	/* GPIO_4 - active low */
#define MX6_OC_TOUCH_IRQ		IMX_GPIO_NR(1, 9)	/* GPIO_9 - active low */

#define MX6_OC_USB_HUB_RESET		IMX_GPIO_NR(7, 12)	/* GPIO_17 - active low */

#define MX6_OC_WL_BT_RESET		IMX_GPIO_NR(6, 8)	/* NANDF_ALE - active low */
#define MX6_OC_WL_BT_REG_EN		IMX_GPIO_NR(6, 15)	/* NANDF_CS2 - active high */
#define MX6_OC_WL_BT_WAKE_IRQ		IMX_GPIO_NR(6, 16)	/* NANDF_CS3 - active low */

#define MX6_OC_WL_EN			IMX_GPIO_NR(6, 7)	/* NANDF_CLE - active high */
#define MX6_OC_WL_CLK_REQ_IRQ		IMX_GPIO_NR(6, 9)	/* NANDF_WP_B - active low */
#define MX6_OC_WL_WAKE_IRQ		IMX_GPIO_NR(6, 14)	/* NANDF_CS1 - active low */


#include "pads-mx6_oc.h"
#define FOR_DL_SOLO
#include "pads-mx6_oc.h"

void __init early_console_setup(unsigned long base, struct clk *clk);
static struct clk *sata_clk;

extern char *gp_reg_id;
extern char *soc_reg_id;
extern char *pu_reg_id;

extern struct regulator *(*get_cpu_regulator)(void);
extern void (*put_cpu_regulator)(void);

#define IOMUX_SETUP(pad_list)	mxc_iomux_v3_setup_pads(mx6q_##pad_list, \
		mx6dl_solo_##pad_list)

int mxc_iomux_v3_setup_pads(iomux_v3_cfg_t *mx6q_pad_list,
		iomux_v3_cfg_t *mx6dl_solo_pad_list)
{
        iomux_v3_cfg_t *p = cpu_is_mx6q() ? mx6q_pad_list : mx6dl_solo_pad_list;
        int ret;

        while (*p) {
                ret = mxc_iomux_v3_setup_pad(*p);
                if (ret)
                        return ret;
                p++;
        }
        return 0;
}

#define GPIOF_HIGH	GPIOF_OUT_INIT_HIGH
struct gpio mx6_oc_init_gpios[] __initdata = {
	{.label = "audio_mute",		.gpio = MX6_OC_AUDIO_MUTE,	.flags = 0},		/* GPIO5[2]: EIM_A25 - active low */

//	{.label = "camera_i2c_en",	.gpio = MX6_OC_CAMERA_I2C_EN,	.flags = 0},		/* GPIO3[20]: EIM_D20 - active high */
	{.label = "camera_pwdn",	.gpio = MX6_OC_CAMERA_PWDN,	.flags = GPIOF_HIGH},	/* GPIO1[16]: SD1_DAT0 - active high */
	{.label = "camera_gp",		.gpio = MX6_OC_CAMERA_GP,	.flags = 0},		/* GPIO1[6]: GPIO_6  */
	{.label = "camera_reset",	.gpio = MX6_OC_CAMERA_RESET,	.flags = 0},		/* GPIO1[8]: GPIO_8 - active low */

//	{.label = "edid_i2c_en",	.gpio = MX6_OC_DISP_I2C_EN,	.flags = 0},		/* GPIO2[17]: EIM_A21 - active high */
	{.label = "backlight_en",	.gpio = MX6_OC_DISP_BACKLIGHT_EN, .flags = 0},		/* GPIO1[17]: SD1_DAT1 - active high */
	{.label = "backlight_12V_en",	.gpio = MX6_OC_DISP_BACKLIGHT_12V_EN, .flags = 0},	/* GPIO4[5]: GPIO_19 - active high */

	{.label = "phy_reset",		.gpio = MX6_OC_ENET_PHY_RESET,	.flags = GPIOF_HIGH},	/* GPIO1[27]: ENET_RXD0 - active low */
//	{.label = "phy_irq",		.gpio = MX6_OC_ENET_PHY_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO1[28]: ENET_TX_EN - active low */

//	{.label = "pcie_reset",		.gpio = MX6_OC_PCIE_RESET,	.flags = 0},		/* GPIO3[22]: EIM_D22 - active low */

//	{.label = "rtc_i2c_en",		.gpio = MX6_OC_RTC_I2C_EN,	.flags = 0},		/* GPIO2[23]: EIM_CS0 - active high */
	{.label = "rtc_irq",		.gpio = MX6_OC_RTC_IRQ,		.flags = GPIOF_DIR_IN},	/* GPIO2[24]:* EIM_CS1 - active low */

	{.label = "emmc_reset",		.gpio = MX6_OC_ST_EMMC_RESET,	.flags = GPIOF_HIGH},	/* GPIO2[5]: NANDF_D5 - active low */
	{.label = "sd3_cd",		.gpio = MX6_OC_ST_SD3_CD,	.flags = GPIOF_DIR_IN},	/* GPIO7[0]: SD3_DAT5 - active low */
//	{.label = "spi_no_cs1",		.gpio = MX6_OC_ST_ECSPI1_CS1,	.flags = GPIOF_HIGH},	/* GPIO3[19]: EIM_D19 - active low */

	{.label = "touch_reset",	.gpio = MX6_OC_TOUCH_RESET,	.flags = GPIOF_HIGH},		/* GPIO1[4]: GPIO_4 - active low */
	{.label = "touch_irq",		.gpio = MX6_OC_TOUCH_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO1[9]: GPIO_9 - active low */

	{.label = "usb_hub_reset",	.gpio = MX6_OC_USB_HUB_RESET,	.flags = 0},		/* GPIO7[12]: GPIO_17 - active low */

	{.label = "bt_reset",		.gpio = MX6_OC_WL_BT_RESET,	.flags = 0},		/* GPIO6[8]: NANDF_ALE - active low */
	{.label = "bt_reg_en",		.gpio = MX6_OC_WL_BT_REG_EN,	.flags = 0},		/* GPIO6[15]: NANDF_CS2 - active high */
	{.label = "bt_wake_irq",	.gpio = MX6_OC_WL_BT_WAKE_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO6[16]: NANDF_CS3 - active low */

	{.label = "wl_en",		.gpio = MX6_OC_WL_EN,		.flags = 0},		/* GPIO6[7]: NANDF_CLE - active high */
	{.label = "wl_clk_req_irq",	.gpio = MX6_OC_WL_CLK_REQ_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO6[9]: NANDF_WP_B - active low */
	{.label = "wl_wake_irq",	.gpio = MX6_OC_WL_WAKE_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO6[14]: NANDF_CS1 - active low */
};


enum sd_pad_mode {
	SD_PAD_MODE_LOW_SPEED,
	SD_PAD_MODE_MED_SPEED,
	SD_PAD_MODE_HIGH_SPEED,
};

static int plt_sd_pad_change(unsigned int index, int clock)
{
	/* LOW speed is the default state of SD pads */
	static enum sd_pad_mode pad_mode = SD_PAD_MODE_LOW_SPEED;
	int i = (index - 1) * SD_SPEED_CNT;

	if ((index < 1) || (index > 3)) {
		printk(KERN_ERR "no such SD host controller index %d\n", index);
		return -EINVAL;
	}

	if (clock > 100000000) {
		if (pad_mode == SD_PAD_MODE_HIGH_SPEED)
			return 0;
		pad_mode = SD_PAD_MODE_HIGH_SPEED;
		i += _200MHZ;
	} else if (clock > 52000000) {
		if (pad_mode == SD_PAD_MODE_MED_SPEED)
			return 0;
		pad_mode = SD_PAD_MODE_MED_SPEED;
		i += _100MHZ;
	} else {
		if (pad_mode == SD_PAD_MODE_LOW_SPEED)
			return 0;
		pad_mode = SD_PAD_MODE_LOW_SPEED;
		i += _50MHZ;
	}
	return IOMUX_SETUP(sd_pads[i]);
}

/* Broadcom wifi */
static struct esdhc_platform_data mx6_oc_sd2_data = {
	.always_present = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.keep_power_at_suspend = 0,
	.caps = MMC_CAP_POWER_OFF_CARD,
	.platform_pad_change = plt_sd_pad_change,
};

/* SD card */
static struct esdhc_platform_data mx6_oc_sd3_data = {
	.cd_gpio = MX6_OC_ST_SD3_CD,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

/* EMMC */
static const struct esdhc_platform_data mx6_oc_sd4_data __initconst = {
	.always_present = 1,
	.support_18v = 1,
	.support_8bit = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct anatop_thermal_platform_data
	mx6_oc_anatop_thermal_data __initconst = {
		.name = "anatop_thermal",
};

static const struct imxuart_platform_data mx6_arm2_uart2_data __initconst = {
	.flags      = IMXUART_HAVE_RTSCTS | IMXUART_SDMA,
	.dma_req_rx = MX6Q_DMA_REQ_UART3_RX,
	.dma_req_tx = MX6Q_DMA_REQ_UART3_TX,
};

static int mx6_oc_fec_phy_init(struct phy_device *phydev)
{
	/* prefer master mode */
	phy_write(phydev, 0x9, 0x1f00);

	/* min rx data delay */
	phy_write(phydev, 0x0b, 0x8105);
	phy_write(phydev, 0x0c, 0x0000);

	/* min tx data delay */
	phy_write(phydev, 0x0b, 0x8106);
	phy_write(phydev, 0x0c, 0x0000);

	/* max rx/tx clock delay, min rx/tx control delay */
	phy_write(phydev, 0x0b, 0x8104);
	phy_write(phydev, 0x0c, 0xf0f0);
	phy_write(phydev, 0x0b, 0x104);
	return 0;
}

static struct fec_platform_data fec_data __initdata = {
	.init = mx6_oc_fec_phy_init,
	.phy = PHY_INTERFACE_MODE_RGMII,
	.phy_irq = gpio_to_irq(MX6_OC_ENET_PHY_IRQ)
};

static int mx6_oc_spi_cs[] = {
	MX6_OC_ST_ECSPI1_CS1,
};

static const struct spi_imx_master mx6_oc_spi_data __initconst = {
	.chipselect     = mx6_oc_spi_cs,
	.num_chipselect = ARRAY_SIZE(mx6_oc_spi_cs),
};

#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
static struct mtd_partition imx6_oc_spi_nor_partitions[] = {
	{
	 .name = "bootloader",
	 .offset = 0,
	 .size = 768*1024,
	},
	{
	 .name = "ubparams",
	 .offset = MTDPART_OFS_APPEND,
	 .size = 8*1024,
	},
	{
	 .name = "unused",
	 .offset = MTDPART_OFS_APPEND,
	 .size = MTDPART_SIZ_FULL,
	},
};

static struct flash_platform_data imx6_oc__spi_flash_data = {
	.name = "m25p80",
	.parts = imx6_oc_spi_nor_partitions,
	.nr_parts = ARRAY_SIZE(imx6_oc_spi_nor_partitions),
	.type = "sst25vf016b",
};
#endif

static struct spi_board_info imx6_oc_spi_nor_device[] __initdata = {
#if defined(CONFIG_MTD_M25P80)
	{
		.modalias = "m25p80",
		.max_speed_hz = 20000000, /* max spi clock (SCK) speed in HZ */
		.bus_num = 0,
		.chip_select = 0,
		.platform_data = &imx6_oc__spi_flash_data,
	},
#endif
};

static void spi_device_init(void)
{
	spi_register_board_info(imx6_oc_spi_nor_device,
				ARRAY_SIZE(imx6_oc_spi_nor_device));
}

static struct mxc_audio_platform_data mx6_oc_audio_data;

static int mx6_oc_sgtl5000_init(void)
{
	struct clk *clko;
	struct clk *new_parent;
	int rate;

	clko = clk_get(NULL, "clko_clk");
	if (IS_ERR(clko)) {
		pr_err("can't get CLKO clock.\n");
		return PTR_ERR(clko);
	}
	new_parent = clk_get(NULL, "ahb");
	if (!IS_ERR(new_parent)) {
		clk_set_parent(clko, new_parent);
		clk_put(new_parent);
	}
	rate = clk_round_rate(clko, 16000000);
	if (rate < 8000000 || rate > 27000000) {
		pr_err("Error:SGTL5000 mclk freq %d out of range!\n", rate);
		clk_put(clko);
		return -1;
	}
	pr_info("%s: rate=%d\n", __func__, rate);
	mx6_oc_audio_data.sysclk = rate;
	clk_set_rate(clko, rate);
	clk_enable(clko);
	return 0;
}

int mx6_oc_amp_enable(int enable)
{
	gpio_set_value(MX6_OC_AUDIO_MUTE, enable ? 1 : 0);
	return 0;
}

static struct imx_ssi_platform_data mx6_oc_ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

static struct mxc_audio_platform_data mx6_oc_audio_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.init = mx6_oc_sgtl5000_init,
	.hp_gpio = -1,
	.mic_gpio = -1,
	.amp_enable = mx6_oc_amp_enable,
};

static struct platform_device mx6_oc_audio_device = {
	.name = "imx-sgtl5000",
};

static struct imxi2c_platform_data mx6_oc_i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("sgtl5000", 0x0a),
	},
};

/*
 **********************************************************************
 */
static struct tsc2007_platform_data tsc2007_info = {
	.model			= 2004,
	.x_plate_ohms		= 500,
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("tsc2004", 0x48),
		.platform_data	= &tsc2007_info,
		.irq = gpio_to_irq(MX6_OC_TOUCH_IRQ),
	},
};
/*
 **********************************************************************
 */
/* i2C bus has a switch */
static const unsigned i2c0_gpiomux_gpios[] = {
	MX6_OC_CAMERA_I2C_EN,	/* i2c3 */
	MX6_OC_RTC_I2C_EN,	/* i2c4 */
};

static const unsigned i2c0_gpiomux_values[] = {
	1, 2,
};

static struct gpio_i2cmux_platform_data i2c0_i2cmux_data = {
	.parent		= 0,
	.base_nr	= 3, /* optional */
	.values		= i2c0_gpiomux_values,
	.n_values	= ARRAY_SIZE(i2c0_gpiomux_values),
	.gpios		= i2c0_gpiomux_gpios,
	.n_gpios	= ARRAY_SIZE(i2c0_gpiomux_gpios),
	.idle		= 0,
};

static struct platform_device i2c0_i2cmux = {
        .name           = "gpio-i2cmux",
        .id             = 0,
        .dev            = {
                .platform_data  = &i2c0_i2cmux_data,
        },
};

/*
 **********************************************************************
 */
/* i2C bus has a switch */
static const unsigned i2c2_gpiomux_gpios[] = {
	MX6_OC_DISP_I2C_EN,	/* i2c5 - edid */
};

static const unsigned i2c2_gpiomux_values[] = {
	1,
};

static struct gpio_i2cmux_platform_data i2c2_i2cmux_data = {
	.parent		= 2,
	.base_nr	= 5, /* optional */
	.values		= i2c2_gpiomux_values,
	.n_values	= ARRAY_SIZE(i2c2_gpiomux_values),
	.gpios		= i2c2_gpiomux_gpios,
	.n_gpios	= ARRAY_SIZE(i2c2_gpiomux_gpios),
	.idle		= 0,
};

static struct platform_device i2c2_i2cmux = {
        .name           = "gpio-i2cmux",
        .id             = 1,
        .dev            = {
                .platform_data  = &i2c2_i2cmux_data,
        },
};

/*
 **********************************************************************
 */
static void camera_reset(int power_gp, int reset_gp, int reset_gp2)
{
	pr_info("%s: power_gp=0x%x, reset_gp=0x%x reset_gp2=0x%x\n",
			__func__, power_gp, reset_gp, reset_gp2);
	/* Camera power down */
	gpio_direction_output(power_gp, 1);
	/* Camera reset */
	gpio_direction_output(reset_gp, 0);
	if (reset_gp2 >= 0)
		gpio_direction_output(reset_gp2, 0);
	msleep(1);
	gpio_set_value(power_gp, 0);
	msleep(1);
	gpio_set_value(reset_gp, 1);
	if (reset_gp2 >= 0)
		gpio_set_value(reset_gp2, 1);
}


/*
 * OV5642 Camera
 */
static void mx6_csi0_io_init(void)
{
	camera_reset(MX6_OC_CAMERA_PWDN, MX6_OC_CAMERA_RESET,
			MX6_OC_CAMERA_GP);
	/* For MX6Q GPR1 bit19 and bit20 meaning:
	 * Bit19:       0 - Enable mipi to IPU1 CSI0
	 *                      virtual channel is fixed to 0
	 *              1 - Enable parallel interface to IPU1 CSI0
	 * Bit20:       0 - Enable mipi to IPU2 CSI1
	 *                      virtual channel is fixed to 3
	 *              1 - Enable parallel interface to IPU2 CSI1
	 * IPU1 CSI1 directly connect to mipi csi2,
	 *      virtual channel is fixed to 1
	 * IPU2 CSI0 directly connect to mipi csi2,
	 *      virtual channel is fixed to 2
	 */
	if (cpu_is_mx6q())
		mxc_iomux_set_gpr_register(1, 19, 1, 1);
	else
		mxc_iomux_set_gpr_register(13, 0, 3, 4);
}

static void mx6_csi0_powerdown(int powerdown)
{
	pr_info("%s: powerdown=%d, power_gp=0x%x\n",
			__func__, powerdown, MX6_OC_CAMERA_PWDN);
	gpio_set_value(MX6_OC_CAMERA_PWDN, powerdown ? 1 : 0);
	msleep(2);
}

static struct fsl_mxc_camera_platform_data camera_data = {
	.mclk = 24000000,
	.mclk_source = 0,
	.csi = 0,
	.io_init = mx6_csi0_io_init,
	.pwdn = mx6_csi0_powerdown,
};

static struct i2c_board_info mxc_i2c3_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("ov5642", 0x3c),
		.platform_data = (void *)&camera_data,
	},
};

/*
 **********************************************************************
 */
static struct i2c_board_info mxc_i2c4_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("isl1208", 0x6f),	/* Real time clock */
	},
};

static struct i2c_board_info mxc_i2c5_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("mxc_hdmi_i2c", 0x50),
	},
};
/*
 **********************************************************************
 */

static void imx6_oc_usbotg_vbus(bool on)
{
}

static void __init imx6_oc_init_usb(void)
{
	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);
	/* disable external charger detect,
	 * or it will affect signal quality at dp .
	 */
	mxc_iomux_set_gpr_register(1, 13, 1, 1);

	mx6_set_otghost_vbus_func(imx6_oc_usbotg_vbus);
}

/* HW Initialization, if return 0, initialization is successful. */
static int mx6_oc_sata_init(struct device *dev, void __iomem *addr)
{
	u32 tmpdata;
	int ret = 0;
	struct clk *clk;

	sata_clk = clk_get(dev, "imx_sata_clk");
	if (IS_ERR(sata_clk)) {
		dev_err(dev, "no sata clock.\n");
		return PTR_ERR(sata_clk);
	}
	ret = clk_enable(sata_clk);
	if (ret) {
		dev_err(dev, "can't enable sata clock.\n");
		goto put_sata_clk;
	}

	/* Set PHY Paremeters, two steps to configure the GPR13,
	 * one write for rest of parameters, mask of first write is 0x07FFFFFD,
	 * and the other one write for setting the mpll_clk_off_b
	 *.rx_eq_val_0(iomuxc_gpr13[26:24]),
	 *.los_lvl(iomuxc_gpr13[23:19]),
	 *.rx_dpll_mode_0(iomuxc_gpr13[18:16]),
	 *.sata_speed(iomuxc_gpr13[15]),
	 *.mpll_ss_en(iomuxc_gpr13[14]),
	 *.tx_atten_0(iomuxc_gpr13[13:11]),
	 *.tx_boost_0(iomuxc_gpr13[10:7]),
	 *.tx_lvl(iomuxc_gpr13[6:2]),
	 *.mpll_ck_off(iomuxc_gpr13[1]),
	 *.tx_edgerate_0(iomuxc_gpr13[0]),
	 */
	tmpdata = readl(IOMUXC_GPR13);
	writel(((tmpdata & ~0x07FFFFFD) | 0x0593A044), IOMUXC_GPR13);

	/* enable SATA_PHY PLL */
	tmpdata = readl(IOMUXC_GPR13);
	writel(((tmpdata & ~0x2) | 0x2), IOMUXC_GPR13);

	/* Get the AHB clock rate, and configure the TIMER1MS reg later */
	clk = clk_get(NULL, "ahb");
	if (IS_ERR(clk)) {
		dev_err(dev, "no ahb clock.\n");
		ret = PTR_ERR(clk);
		goto release_sata_clk;
	}
	tmpdata = clk_get_rate(clk) / 1000;
	clk_put(clk);

	ret = sata_init(addr, tmpdata);
	if (ret == 0)
		return ret;

release_sata_clk:
	clk_disable(sata_clk);
put_sata_clk:
	clk_put(sata_clk);

	return ret;
}

static void mx6_oc_sata_exit(struct device *dev)
{
	clk_disable(sata_clk);
	clk_put(sata_clk);
}

static struct ahci_platform_data mx6_oc_sata_data = {
	.init = mx6_oc_sata_init,
	.exit = mx6_oc_sata_exit,
};

static struct viv_gpu_platform_data imx6_gpu_pdata __initdata = {
	.reserved_mem_size = SZ_128M,
};

static struct imx_asrc_platform_data imx_asrc_data = {
	.channel_bits = 4,
	.clk_map_ver = 2,
};

static struct ipuv3_fb_platform_data oc_fb_data[] = {
	{ /*fb0*/
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "LDB-VGA",
	.default_bpp = 16,
	.int_clk = false,
	}, {
	.disp_dev = "lcd",
	.interface_pix_fmt = IPU_PIX_FMT_RGB565,
	.mode_str = "CLAA-WVGA",
	.default_bpp = 16,
	.int_clk = false,
	}, {
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "LDB-SVGA",
	.default_bpp = 16,
	.int_clk = false,
	},
};

static void hdmi_init(int ipu_id, int disp_id)
{
	int hdmi_mux_setting;

	if ((ipu_id > 1) || (ipu_id < 0)) {
		pr_err("Invalid IPU select for HDMI: %d. Set to 0\n", ipu_id);
		ipu_id = 0;
	}

	if ((disp_id > 1) || (disp_id < 0)) {
		pr_err("Invalid DI select for HDMI: %d. Set to 0\n", disp_id);
		disp_id = 0;
	}

	/* Configure the connection between IPU1/2 and HDMI */
	hdmi_mux_setting = 2*ipu_id + disp_id;

	/* GPR3, bits 2-3 = HDMI_MUX_CTL */
	mxc_iomux_set_gpr_register(3, 2, 2, hdmi_mux_setting);

	/* Set HDMI event as SDMA event2 while Chip version later than TO1.2 */
	if ((mx6q_revision() > IMX_CHIP_REVISION_1_1))
		mxc_iomux_set_gpr_register(0, 0, 1, 1);
}

static void hdmi_enable_ddc_pin(void)
{
}

static void hdmi_disable_ddc_pin(void)
{
}

static struct fsl_mxc_hdmi_platform_data hdmi_data = {
	.init = hdmi_init,
	.enable_pins = hdmi_enable_ddc_pin,
	.disable_pins = hdmi_disable_ddc_pin,
};

static struct fsl_mxc_hdmi_core_platform_data hdmi_core_data = {
	.ipu_id = 0,
	.disp_id = 1,
};

static struct fsl_mxc_ldb_platform_data ldb_data = {
	.ipu_id = 1,
	.disp_id = 0,
	.ext_ref = 1,
	.mode = LDB_SEP0,
	.sec_ipu_id = 1,
	.sec_disp_id = 1,
};

static struct imx_ipuv3_platform_data ipu_data[] = {
	{
	.rev = 4,
	.csi_clk[0] = "clko2_clk",
	}, {
	.rev = 4,
	.csi_clk[0] = "clko2_clk",
	},
};

static struct fsl_mxc_capture_platform_data capture_data = {
	.csi = 0,
	.ipu = 0,
	.mclk_source = 0,
	.is_mipi = 0,
};


static void oc_suspend_enter(void)
{
	/* suspend preparation */
}

static void oc_suspend_exit(void)
{
	/* resume restore */
}
static const struct pm_platform_data mx6_oc_pm_data __initconst = {
	.name = "imx_pm",
	.suspend_enter = oc_suspend_enter,
	.suspend_exit = oc_suspend_exit,
};

static struct regulator_consumer_supply mx6_oc_vwifi_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.1"),
};

static struct regulator_init_data mx6_oc_vwifi_init = {
	.constraints            = {
		.name           = "VDD_1.8V",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(mx6_oc_vwifi_consumers),
	.consumer_supplies = mx6_oc_vwifi_consumers,
};

static struct fixed_voltage_config mx6_oc_vwifi_reg_config = {
	.supply_name		= "vwifi",
	.microvolts		= 1800000, /* 1.80V */
	.gpio			= MX6_OC_WL_EN,
	.startup_delay		= 70000, /* 70ms */
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &mx6_oc_vwifi_init,
};

static struct platform_device mx6_oc_vwifi_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 4,
	.dev	= {
		.platform_data = &mx6_oc_vwifi_reg_config,
	},
};

static struct regulator_consumer_supply oc_vmmc_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.2"),
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.3"),
};

static struct regulator_init_data oc_vmmc_init = {
	.num_consumer_supplies = ARRAY_SIZE(oc_vmmc_consumers),
	.consumer_supplies = oc_vmmc_consumers,
};

static struct fixed_voltage_config oc_vmmc_reg_config = {
	.supply_name		= "vmmc",
	.microvolts		= 3300000,
	.gpio			= -1,
	.init_data		= &oc_vmmc_init,
};

static struct platform_device oc_vmmc_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 3,
	.dev	= {
		.platform_data = &oc_vmmc_reg_config,
	},
};

#ifdef CONFIG_SND_SOC_SGTL5000

static struct regulator_consumer_supply sgtl5000_oc_consumer_vdda = {
	.supply = "VDDA",
	.dev_name = "0-000a",
};

static struct regulator_consumer_supply sgtl5000_oc_consumer_vddio = {
	.supply = "VDDIO",
	.dev_name = "0-000a",
};

static struct regulator_consumer_supply sgtl5000_oc_consumer_vddd = {
	.supply = "VDDD",
	.dev_name = "0-000a",
};

static struct regulator_init_data sgtl5000_oc_vdda_reg_initdata = {
	.num_consumer_supplies = 1,
	.consumer_supplies = &sgtl5000_oc_consumer_vdda,
};

static struct regulator_init_data sgtl5000_oc_vddio_reg_initdata = {
	.num_consumer_supplies = 1,
	.consumer_supplies = &sgtl5000_oc_consumer_vddio,
};

static struct regulator_init_data sgtl5000_oc_vddd_reg_initdata = {
	.num_consumer_supplies = 1,
	.consumer_supplies = &sgtl5000_oc_consumer_vddd,
};

static struct fixed_voltage_config sgtl5000_oc_vdda_reg_config = {
	.supply_name		= "VDDA",
	.microvolts		= 2500000,
	.gpio			= -1,
	.init_data		= &sgtl5000_oc_vdda_reg_initdata,
};

static struct fixed_voltage_config sgtl5000_oc_vddio_reg_config = {
	.supply_name		= "VDDIO",
	.microvolts		= 3300000,
	.gpio			= -1,
	.init_data		= &sgtl5000_oc_vddio_reg_initdata,
};

static struct fixed_voltage_config sgtl5000_oc_vddd_reg_config = {
	.supply_name		= "VDDD",
	.microvolts		= 3300000,
	.gpio			= -1,
	.init_data		= &sgtl5000_oc_vddd_reg_initdata,
};

static struct platform_device sgtl5000_oc_vdda_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 0,
	.dev	= {
		.platform_data = &sgtl5000_oc_vdda_reg_config,
	},
};

static struct platform_device sgtl5000_oc_vddio_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 1,
	.dev	= {
		.platform_data = &sgtl5000_oc_vddio_reg_config,
	},
};

static struct platform_device sgtl5000_oc_vddd_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 2,
	.dev	= {
		.platform_data = &sgtl5000_oc_vddd_reg_config,
	},
};

#endif /* CONFIG_SND_SOC_SGTL5000 */

static int imx6_init_audio(void)
{
	mxc_register_device(&mx6_oc_audio_device,
			    &mx6_oc_audio_data);
	imx6q_add_imx_ssi(1, &mx6_oc_ssi_pdata);
#ifdef CONFIG_SND_SOC_SGTL5000
	platform_device_register(&sgtl5000_oc_vdda_reg_devices);
	platform_device_register(&sgtl5000_oc_vddio_reg_devices);
	platform_device_register(&sgtl5000_oc_vddd_reg_devices);
#endif
	return 0;
}

int mx6_oc_bl_notify(struct device *dev, int brightness)
{
	pr_info("%s: brightness=%d\n", __func__, brightness);
	gpio_set_value(MX6_OC_DISP_BACKLIGHT_12V_EN, brightness ? 1 : 0);
	gpio_set_value(MX6_OC_DISP_BACKLIGHT_EN, brightness ? 1 : 0);
	return brightness;
}

/* PWM4_PWMO: backlight control on LDB connector */
static struct platform_pwm_backlight_data mx6_oc_pwm4_backlight_data = {
	.pwm_id = 3,	/* pin SD1_CMD - PWM4 */
	.max_brightness = 256,
	.dft_brightness = 128,
	.pwm_period_ns = 50000,
	.notify = mx6_oc_bl_notify,
};

static struct mxc_dvfs_platform_data oc_dvfscore_data = {
	.reg_id = "cpu_vddgp",
	.soc_id = "cpu_vddsoc",
	.pu_id = "cpu_vddvpu",
	.clk1_id = "cpu_clk",
	.clk2_id = "gpc_dvfs_clk",
	.gpc_cntr_offset = MXC_GPC_CNTR_OFFSET,
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
	.delay_time = 80,
};

static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
}

static const struct imx_pcie_platform_data pcie_data  __initconst = {
	.pcie_pwr_en	= -EINVAL,
	.pcie_rst	= MX6_OC_PCIE_RESET,
	.pcie_wake_up	= -EINVAL,
	.pcie_dis	= -EINVAL,
};

/*!
 * Board specific initialization.
 */
static void __init mx6_oc_board_init(void)
{
	int i, j;
	struct clk *clko2;
	struct clk *new_parent;
	int rate;
	int ret = gpio_request_array(mx6_oc_init_gpios,
			ARRAY_SIZE(mx6_oc_init_gpios));

	if (ret) {
		printk(KERN_ERR "%s gpio_request_array failed("
				"%d) for mx6_oc_init_gpios\n", __func__, ret);
	}
	IOMUX_SETUP(common_pads);


	gp_reg_id = oc_dvfscore_data.reg_id;
	soc_reg_id = oc_dvfscore_data.soc_id;
	pu_reg_id = oc_dvfscore_data.pu_id;

	imx6q_add_imx_uart(0, NULL);
	imx6q_add_imx_uart(1, NULL);
	imx6q_add_imx_uart(2, &mx6_arm2_uart2_data);

	if (!cpu_is_mx6q()) {
		ldb_data.ipu_id = 0;
		ldb_data.sec_ipu_id = 0;
	}
	imx6q_add_mxc_hdmi_core(&hdmi_core_data);

	imx6q_add_ipuv3(0, &ipu_data[0]);
	if (cpu_is_mx6q()) {
		imx6q_add_ipuv3(1, &ipu_data[1]);
		j = ARRAY_SIZE(oc_fb_data);
	} else {
		j = (ARRAY_SIZE(oc_fb_data) + 1) / 2;
	}
	for (i = 0; i < j; i++)
		imx6q_add_ipuv3fb(i, &oc_fb_data[i]);

	imx6q_add_vdoa();
	imx6q_add_ldb(&ldb_data);
	imx6q_add_v4l2_output(0);
	imx6q_add_v4l2_capture(0, &capture_data);
	imx6q_add_imx_snvs_rtc();

	imx6q_add_imx_i2c(0, &mx6_oc_i2c_data);
	imx6q_add_imx_i2c(2, &mx6_oc_i2c_data);
	i2c_register_board_info(0, mxc_i2c0_board_info,
			ARRAY_SIZE(mxc_i2c0_board_info));
	i2c_register_board_info(2, mxc_i2c1_board_info,
			ARRAY_SIZE(mxc_i2c1_board_info));

	mxc_register_device(&i2c0_i2cmux, &i2c0_i2cmux_data);
	i2c_register_board_info(3, mxc_i2c3_board_info,
			ARRAY_SIZE(mxc_i2c3_board_info));
	i2c_register_board_info(4, mxc_i2c4_board_info,
			ARRAY_SIZE(mxc_i2c4_board_info));

	mxc_register_device(&i2c2_i2cmux, &i2c2_i2cmux_data);
	i2c_register_board_info(5, mxc_i2c5_board_info,
			ARRAY_SIZE(mxc_i2c5_board_info));

	/* SPI */
	imx6q_add_ecspi(0, &mx6_oc_spi_data);
	spi_device_init();

	imx6q_add_mxc_hdmi(&hdmi_data);

	imx6q_add_anatop_thermal_imx(1, &mx6_oc_anatop_thermal_data);
	imx6_init_fec(fec_data);
	imx6q_add_pm_imx(0, &mx6_oc_pm_data);
	imx6q_add_sdhci_usdhc_imx(2, &mx6_oc_sd3_data);
	imx6q_add_sdhci_usdhc_imx(3, &mx6_oc_sd4_data);
	imx_add_viv_gpu(&imx6_gpu_data, &imx6_gpu_pdata);
	imx6_oc_init_usb();
	imx6q_add_vpu();
	if (cpu_is_mx6q())
		imx6q_add_ahci(0, &mx6_oc_sata_data);
	imx6_init_audio();
	platform_device_register(&oc_vmmc_reg_devices);
	imx_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	imx_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	imx6q_add_asrc(&imx_asrc_data);

	/* release USB Hub reset */
	gpio_set_value(MX6_OC_USB_HUB_RESET, 1);

	imx6q_add_mxc_pwm(3);

	imx6q_add_mxc_pwm_backlight(3, &mx6_oc_pwm4_backlight_data);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&oc_dvfscore_data);

	imx6q_add_hdmi_soc();
	imx6q_add_hdmi_soc_dai();

	clko2 = clk_get(NULL, "clko2_clk");
	if (IS_ERR(clko2)) {
		pr_err("can't get CLKO2 clock.\n");
	} else {
		new_parent = clk_get(NULL, "osc_clk");
		if (!IS_ERR(new_parent)) {
			clk_set_parent(clko2, new_parent);
			clk_put(new_parent);
		}
		rate = clk_round_rate(clko2, 24000000);
		clk_set_rate(clko2, rate);
		clk_enable(clko2);
	}
	imx6q_add_busfreq();

	imx6q_add_sdhci_usdhc_imx(1, &mx6_oc_sd2_data);
	platform_device_register(&mx6_oc_vwifi_reg_devices);

	gpio_set_value(MX6_OC_WL_EN, 1);		/* momentarily enable */
	gpio_set_value(MX6_OC_WL_BT_REG_EN, 1);
	mdelay(2);
	gpio_set_value(MX6_OC_WL_EN, 0);
	gpio_set_value(MX6_OC_WL_BT_REG_EN, 0);

	gpio_free(MX6_OC_WL_EN);
	gpio_free(MX6_OC_WL_BT_REG_EN);
	mdelay(1);

	imx6q_add_pcie(&pcie_data);

	imx6q_add_perfmon(0);
	imx6q_add_perfmon(1);
	imx6q_add_perfmon(2);
//	regulator_has_full_constraints();
}

extern void __iomem *twd_base;
static void __init mx6_oc_timer_init(void)
{
	struct clk *uart_clk;
#ifdef CONFIG_LOCAL_TIMERS
	twd_base = ioremap(LOCAL_TWD_ADDR, SZ_256);
	BUG_ON(!twd_base);
#endif
	mx6_clocks_init(32768, 24000000, 0, 0);

	uart_clk = clk_get_sys("imx-uart.0", NULL);
	early_console_setup(UART2_BASE_ADDR, uart_clk);
}

static struct sys_timer mx6_oc_timer = {
	.init   = mx6_oc_timer_init,
};

static void __init mx6_oc_reserve(void)
{
#if defined(CONFIG_MXC_GPU_VIV) || defined(CONFIG_MXC_GPU_VIV_MODULE)
	phys_addr_t phys;

	if (imx6_gpu_pdata.reserved_mem_size) {
		phys = memblock_alloc_base(imx6_gpu_pdata.reserved_mem_size,
					   SZ_4K, SZ_1G);
		memblock_remove(phys, imx6_gpu_pdata.reserved_mem_size);
		imx6_gpu_pdata.reserved_mem_base = phys;
	}
#endif
}

/*
 * initialize __mach_desc_MX6Q_OC data structure.
 */
MACHINE_START(MX6_OC, "Freescale i.MX 6 OC Board")
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = mx6_oc_board_init,
	.timer = &mx6_oc_timer,
	.reserve = mx6_oc_reserve,
MACHINE_END
