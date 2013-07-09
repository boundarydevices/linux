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

#define AUDIO_MUTE		IMX_GPIO_NR(5, 2)	/* EIM_A25 - active low */

#define DISP_I2C_EN		IMX_GPIO_NR(2, 17)	/* EIM_A21 - active high */
#define DISP_BACKLIGHT_EN	IMX_GPIO_NR(1, 17)	/* SD1_DAT1 - active high */
#define DISP_BACKLIGHT_12V_EN	IMX_GPIO_NR(4, 5)	/* GPIO_19 - active high */

#define ENET_PHY_RESET		IMX_GPIO_NR(1, 27)	/* ENET_RXD0 - active low */
#define ENET_PHY_IRQ		IMX_GPIO_NR(1, 28)	/* ENET_TX_EN - active low */

#define RTC_I2C_EN		IMX_GPIO_NR(2, 23)	/* EIM_CS0 - active high */
#define RTC_IRQ			IMX_GPIO_NR(2, 26)	/* EIM_RW - active low */

#define ST_EMMC_RESET		IMX_GPIO_NR(2, 5)	/* NANDF_D5 - active low */
#define ST_SD3_CD		IMX_GPIO_NR(7, 0)	/* SD3_DAT5 - active low */
#define ST_ECSPI1_CS1		IMX_GPIO_NR(3, 19)	/* EIM_D19 - active low */

#define TOUCH_RESET		IMX_GPIO_NR(1, 4)	/* GPIO_4 - active low */
#define TOUCH_IRQ		IMX_GPIO_NR(2, 27)	/* EIM_LBA - active low */

#define CAP_TCH_INT		IMX_GPIO_NR(1, 9)	/* GPIO_9 - J7: pin 4: active low */

#define USB_HUB_RESET		IMX_GPIO_NR(7, 12)	/* GPIO_17 - active low */

#define WL_BT_RESET		IMX_GPIO_NR(6, 8)	/* NANDF_ALE - active low */
#define WL_BT_REG_EN		IMX_GPIO_NR(6, 15)	/* NANDF_CS2 - active high */
#define WL_BT_WAKE_IRQ		IMX_GPIO_NR(6, 16)	/* NANDF_CS3 - active low */

#define WL_EN			IMX_GPIO_NR(6, 7)	/* NANDF_CLE - active high */
#define WL_CLK_REQ_IRQ		IMX_GPIO_NR(6, 9)	/* NANDF_WP_B - active low */
#define WL_WAKE_IRQ		IMX_GPIO_NR(6, 14)	/* NANDF_CS1 - active low */

#define MX6_N6L_DRYCONTACT		IMX_GPIO_NR(1, 6)	/* J14 pins 8 and 9 - dry contact */
#define MX6_N6L_GLED			IMX_GPIO_NR(1, 2)	/* J14 pin1: GPIO2 */
#define MX6_N6L_RLED			IMX_GPIO_NR(1, 3)	/* J14 pin3: GPIO3 */
#define MX6_N6L_VOLUP			IMX_GPIO_NR(7, 13)	/* J14 pin5: GPIO_18 */
#define MX6_N6L_VOLDOWN			IMX_GPIO_NR(4, 5)	/* J14 pin7: GPIO_19 */

#include "pads-mx6_nit6xlite.h"
#define FOR_DL_SOLO
#include "pads-mx6_nit6xlite.h"

void __init early_console_setup(unsigned long base, struct clk *clk);

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
struct gpio mx6_init_gpios[] __initdata = {
	{.label = "audio_mute",		.gpio = AUDIO_MUTE,	.flags = 0},		/* GPIO5[2]: EIM_A25 - active low */

//	{.label = "edid_i2c_en",	.gpio = DISP_I2C_EN,	.flags = 0},		/* GPIO2[17]: EIM_A21 - active high */
	{.label = "backlight_en",	.gpio = DISP_BACKLIGHT_EN, .flags = 0},		/* GPIO1[17]: SD1_DAT1 - active high */
	{.label = "backlight_12V_en",	.gpio = DISP_BACKLIGHT_12V_EN, .flags = 0},	/* GPIO4[5]: GPIO_19 - active high */

	{.label = "phy_reset",		.gpio = ENET_PHY_RESET,	.flags = GPIOF_HIGH},	/* GPIO1[27]: ENET_RXD0 - active low */
//	{.label = "phy_irq",		.gpio = ENET_PHY_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO1[28]: ENET_TX_EN - active low */

//	{.label = "rtc_i2c_en",		.gpio = RTC_I2C_EN,	.flags = 0},		/* GPIO2[23]: EIM_CS0 - active high */
	{.label = "rtc_irq",		.gpio = RTC_IRQ,		.flags = GPIOF_DIR_IN},	/* GPIO2[24]:* EIM_CS1 - active low */

	{.label = "touch_reset",	.gpio = TOUCH_RESET,	.flags = GPIOF_HIGH},	/* GPIO1[4]: GPIO_4 - active low */
	{.label = "touch_irq",		.gpio = TOUCH_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO2[27]: EIM_LBA - active low */

	{.label = "usb_hub_reset",	.gpio = USB_HUB_RESET,	.flags = 0},		/* GPIO7[12]: GPIO_17 - active low */

	{.label = "bt_reset",		.gpio = WL_BT_RESET,	.flags = 0},		/* GPIO6[8]: NANDF_ALE - active low */
	{.label = "bt_reg_en",		.gpio = WL_BT_REG_EN,	.flags = 0},		/* GPIO6[15]: NANDF_CS2 - active high */
	{.label = "bt_wake_irq",	.gpio = WL_BT_WAKE_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO6[16]: NANDF_CS3 - active low */

	{.label = "wl_en",		.gpio = WL_EN,		.flags = 0},		/* GPIO6[7]: NANDF_CLE - active high */
	{.label = "wl_clk_req_irq",	.gpio = WL_CLK_REQ_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO6[9]: NANDF_WP_B - active low */
	{.label = "wl_wake_irq",	.gpio = WL_WAKE_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO6[14]: NANDF_CS1 - active low */

	{.label = "drycontact",		.gpio = MX6_N6L_DRYCONTACT,	.flags = GPIOF_HIGH},	/* J14 pins 8/9: GPIO6 */
	{.label = "gled",		.gpio = MX6_N6L_GLED,		.flags = GPIOF_HIGH},	/* J14 pin1: GPIO2 */
	{.label = "rled",		.gpio = MX6_N6L_RLED,		.flags = GPIOF_HIGH},	/* J14 pin3: GPIO3 */
	{.label = "volup",		.gpio = MX6_N6L_VOLUP,		.flags = GPIOF_DIR_IN},	/* J14 pin5: GPIO_18 */
	{.label = "voldown",		.gpio = MX6_N6L_VOLDOWN,	.flags = GPIOF_DIR_IN},	/* J14 pin7: GPIO_19 */
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

static void sdio_set_power(int on)
{
	pr_err("%s:%s: set power(%d)\n",
		 __FILE__, __func__, on);
        gpio_set_value(WL_EN,on);
}

/* Broadcom wifi */
static struct esdhc_platform_data mx6_sd2_data = {
	.always_present = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.keep_power_at_suspend = 0,
	.caps = MMC_CAP_POWER_OFF_CARD,
	.platform_pad_change = plt_sd_pad_change,
	.set_power = sdio_set_power,
};

/* SD card */
static struct esdhc_platform_data mx6_sd3_data = {
	.cd_gpio = ST_SD3_CD,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct anatop_thermal_platform_data
	mx6_anatop_thermal_data __initconst = {
		.name = "anatop_thermal",
};

static const struct imxuart_platform_data mx6_arm2_uart2_data __initconst = {
	.flags      = IMXUART_HAVE_RTSCTS | IMXUART_SDMA,
	.dma_req_rx = MX6Q_DMA_REQ_UART3_RX,
	.dma_req_tx = MX6Q_DMA_REQ_UART3_TX,
};

static int mx6_fec_phy_init(struct phy_device *phydev)
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
	.init = mx6_fec_phy_init,
	.phy = PHY_INTERFACE_MODE_RGMII,
	.phy_irq = gpio_to_irq(ENET_PHY_IRQ)
};

static int mx6_spi_cs[] = {
	ST_ECSPI1_CS1,
};

static const struct spi_imx_master mx6_spi_data __initconst = {
	.chipselect     = mx6_spi_cs,
	.num_chipselect = ARRAY_SIZE(mx6_spi_cs),
};

#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
static struct mtd_partition imx6_spi_nor_partitions[] = {
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

static struct flash_platform_data imx6__spi_flash_data = {
	.name = "m25p80",
	.parts = imx6_spi_nor_partitions,
	.nr_parts = ARRAY_SIZE(imx6_spi_nor_partitions),
	.type = "sst25vf016b",
};
#endif

static struct spi_board_info imx6_spi_nor_device[] __initdata = {
#if defined(CONFIG_MTD_M25P80)
	{
		.modalias = "m25p80",
		.max_speed_hz = 20000000, /* max spi clock (SCK) speed in HZ */
		.bus_num = 0,
		.chip_select = 0,
		.platform_data = &imx6__spi_flash_data,
	},
#endif
};

static void spi_device_init(void)
{
	spi_register_board_info(imx6_spi_nor_device,
				ARRAY_SIZE(imx6_spi_nor_device));
}

static struct mxc_audio_platform_data mx6_audio_data;

static int mx6_sgtl5000_init(void)
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
	mx6_audio_data.sysclk = rate;
	clk_set_rate(clko, rate);
	clk_enable(clko);
	return 0;
}

static int mx6_amp_enable(int enable)
{
	gpio_set_value(AUDIO_MUTE, enable ? 1 : 0);
	return 0;
}

static struct imx_ssi_platform_data mx6_ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

static struct mxc_audio_platform_data mx6_audio_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.init = mx6_sgtl5000_init,
	.hp_gpio = -1,
	.mic_gpio = -1,
	.amp_enable = mx6_amp_enable,
};

static struct platform_device mx6_audio_device = {
	.name = "imx-sgtl5000",
};

static struct imxi2c_platform_data mx6_i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("sgtl5000", 0x0a),
	},
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("mxc_hdmi_i2c", 0x50),
	},
};

static struct tsc2007_platform_data tsc2007_info = {
	.model			= 2004,
	.x_plate_ohms		= 500,
};

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
#if defined(CONFIG_TOUCHSCREEN_EGALAX) \
	|| defined(CONFIG_TOUCHSCREEN_EGALAX_MODULE)
	{
		I2C_BOARD_INFO("egalax_ts", 0x4),
		.irq = gpio_to_irq(CAP_TCH_INT),
	},
#endif
#if defined(CONFIG_TOUCHSCREEN_TSC2004) \
	|| defined(CONFIG_TOUCHSCREEN_TSC2004_MODULE)
	{
		I2C_BOARD_INFO("tsc2004", 0x48),
		.platform_data	= &tsc2007_info,
		.irq = gpio_to_irq(TOUCH_IRQ),
	},
#endif
	{
		I2C_BOARD_INFO("isl1208", 0x6f),	/* Real time clock */
	},
#if defined(CONFIG_TOUCHSCREEN_FT5X06) \
	|| defined(CONFIG_TOUCHSCREEN_FT5X06_MODULE)
	{
		I2C_BOARD_INFO("ft5x06-ts", 0x38),
		.irq = gpio_to_irq(CAP_TCH_INT),
	},
#endif
};

/*
 **********************************************************************
 */

static void imx6_usbotg_vbus(bool on)
{
}

static void __init imx6_init_usb(void)
{
	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);
	/* disable external charger detect,
	 * or it will affect signal quality at dp .
	 */
	mxc_iomux_set_gpr_register(1, 13, 1, 0);

	mx6_set_otghost_vbus_func(imx6_usbotg_vbus);
}

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
	.mode_str = "wqvga-lcd",
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

static struct fsl_mxc_lcd_platform_data lcdif_data = {
	.ipu_id = 0,
	.disp_id = 0,
	.default_ifmt = IPU_PIX_FMT_RGB565,
};

static struct fsl_mxc_ldb_platform_data ldb_data = {
	.ipu_id = 0,
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

static void oc_suspend_enter(void)
{
	/* suspend preparation */
}

static void oc_suspend_exit(void)
{
	/* resume restore */
}
static const struct pm_platform_data mx6_pm_data __initconst = {
	.name = "imx_pm",
	.suspend_enter = oc_suspend_enter,
	.suspend_exit = oc_suspend_exit,
};

static struct regulator_consumer_supply mx6_vwifi_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.1"),
};

static struct regulator_init_data mx6_vwifi_init = {
	.constraints            = {
		.name           = "VDD_1.8V",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(mx6_vwifi_consumers),
	.consumer_supplies = mx6_vwifi_consumers,
};

static struct fixed_voltage_config mx6_vwifi_reg_config = {
	.supply_name		= "vwifi",
	.microvolts		= 1800000, /* 1.80V */
	.gpio			= WL_EN,
	.startup_delay		= 70000, /* 70ms */
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &mx6_vwifi_init,
};

static struct platform_device mx6_vwifi_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 4,
	.dev	= {
		.platform_data = &mx6_vwifi_reg_config,
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
	mxc_register_device(&mx6_audio_device,
			    &mx6_audio_data);
	imx6q_add_imx_ssi(1, &mx6_ssi_pdata);
#ifdef CONFIG_SND_SOC_SGTL5000
	platform_device_register(&sgtl5000_oc_vdda_reg_devices);
	platform_device_register(&sgtl5000_oc_vddio_reg_devices);
	platform_device_register(&sgtl5000_oc_vddd_reg_devices);
#endif
	return 0;
}

int mx6_bl_notify(struct device *dev, int brightness)
{
	pr_info("%s: brightness=%d\n", __func__, brightness);
	gpio_set_value(DISP_BACKLIGHT_12V_EN, brightness ? 1 : 0);
	gpio_set_value(DISP_BACKLIGHT_EN, brightness ? 1 : 0);
	return brightness;
}

/* PWM4_PWMO: backlight control on LDB connector */
static struct platform_pwm_backlight_data mx6_pwm4_backlight_data = {
	.pwm_id = 3,	/* pin SD1_CMD - PWM4 */
	.max_brightness = 256,
	.dft_brightness = 128,
	.pwm_period_ns = 50000,
	.notify = mx6_bl_notify,
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

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
#define GPIO_BUTTON(gpio_num, ev_code, act_low, descr, wake)	\
{								\
	.gpio		= gpio_num,				\
	.type		= EV_KEY,				\
	.code		= ev_code,				\
	.active_low	= act_low,				\
	.desc		= "btn " descr,				\
	.wakeup		= wake,					\
	.debounce_interval = 1	\
}

static struct gpio_keys_button buttons[] = {
	GPIO_BUTTON(MX6_N6L_VOLUP, KEY_HOME, 1, "volume-up", 0),
	GPIO_BUTTON(MX6_N6L_VOLDOWN, KEY_BACK, 1, "volume-down", 0),
};

static struct gpio_keys_platform_data button_data = {
	.buttons	= buttons,
	.nbuttons	= ARRAY_SIZE(buttons),
};

static struct platform_device button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources  = 0,
	.dev		= {
		.platform_data = &button_data,
	}
};

static void __init add_device_buttons(void)
{
	platform_device_register(&button_device);
}
#endif

static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
}

/*!
 * Board specific initialization.
 */
static void __init mx6_board_init(void)
{
	int i, j;
	struct clk *clko2;
	struct clk *new_parent;
	int rate;
	int ret = gpio_request_array(mx6_init_gpios,
			ARRAY_SIZE(mx6_init_gpios));

	if (ret) {
		printk(KERN_ERR "%s gpio_request_array failed("
				"%d) for mx6_init_gpios\n", __func__, ret);
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
	imx6q_add_lcdif(&lcdif_data);
	imx6q_add_ldb(&ldb_data);
	imx6q_add_v4l2_output(0);
	imx6q_add_imx_snvs_rtc();

	imx6q_add_imx_i2c(0, &mx6_i2c_data);
	imx6q_add_imx_i2c(1, &mx6_i2c_data);
	imx6q_add_imx_i2c(2, &mx6_i2c_data);
	i2c_register_board_info(0, mxc_i2c0_board_info,
			ARRAY_SIZE(mxc_i2c0_board_info));
	i2c_register_board_info(1, mxc_i2c1_board_info,
			ARRAY_SIZE(mxc_i2c1_board_info));
	i2c_register_board_info(2, mxc_i2c2_board_info,
			ARRAY_SIZE(mxc_i2c2_board_info));

	/* SPI */
	imx6q_add_ecspi(0, &mx6_spi_data);
	spi_device_init();

	imx6q_add_mxc_hdmi(&hdmi_data);

	imx6q_add_anatop_thermal_imx(1, &mx6_anatop_thermal_data);
	imx6_init_fec(fec_data);
	imx6q_add_pm_imx(0, &mx6_pm_data);
	imx6q_add_sdhci_usdhc_imx(2, &mx6_sd3_data);
	imx_add_viv_gpu(&imx6_gpu_data, &imx6_gpu_pdata);
	imx6_init_usb();
	imx6q_add_vpu();
	imx6_init_audio();
	platform_device_register(&oc_vmmc_reg_devices);
	imx_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	imx_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	imx6q_add_asrc(&imx_asrc_data);

	/* release USB Hub reset */
	gpio_set_value(USB_HUB_RESET, 1);

	imx6q_add_mxc_pwm(3);

	imx6q_add_mxc_pwm_backlight(3, &mx6_pwm4_backlight_data);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&oc_dvfscore_data);

	add_device_buttons();

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

	imx6q_add_sdhci_usdhc_imx(1, &mx6_sd2_data);
	platform_device_register(&mx6_vwifi_reg_devices);

	gpio_set_value(WL_EN, 1);		/* momentarily enable */
	gpio_set_value(WL_BT_REG_EN, 1);
	mdelay(2);
	gpio_set_value(WL_EN, 0);
	gpio_set_value(WL_BT_REG_EN, 0);

	gpio_free(WL_EN);
	gpio_free(WL_BT_REG_EN);
	gpio_free(WL_BT_RESET);
	mdelay(1);

	imx6q_add_perfmon(0);
	imx6q_add_perfmon(1);
	imx6q_add_perfmon(2);
//	regulator_has_full_constraints();
}

extern void __iomem *twd_base;
static void __init mx6_timer_init(void)
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

static struct sys_timer mx6_timer = {
	.init   = mx6_timer_init,
};

static void __init mx6_reserve(void)
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
MACHINE_START(MX6_NIT6XLITE, "Boundary Devices Nit6xLite Board")
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = mx6_board_init,
	.timer = &mx6_timer,
	.reserve = mx6_reserve,
MACHINE_END
