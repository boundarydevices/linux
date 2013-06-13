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
#include <linux/leds_pwm.h>
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
#include <mach/mxc_asrc.h>
#include <mach/system.h>
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

#define GPIOF_HIGH	GPIOF_OUT_INIT_HIGH

struct gpio board_gpios[] __initdata = {
#define GP_LVDS0_12V_5V_BL_SELECT		IMX_GPIO_NR(4, 5)
	{.label = "lvds0_12_5_BL",	.gpio = IMX_GPIO_NR(4, 5),	.flags = 0},	/* GPIO_19 */
#define GP_RGB_LVDS1_12V_5V_BL_SELECT		IMX_GPIO_NR(1, 7)
	{.label = "rgb_lvds1_12_5_BL",	.gpio = IMX_GPIO_NR(1, 7),	.flags = 0},	/* GPIO_7 */
#define GP_12V_POWER_EN				IMX_GPIO_NR(4, 20)
	{.label = "12V_en",		.gpio = IMX_GPIO_NR(4, 20),	.flags = 0},	/* DI0_PIN4 */
	{.label = "gp_led",		.gpio = IMX_GPIO_NR(2, 7),	.flags = 0},	/* GPIO_3 */
	{.label = "sata_led",		.gpio = IMX_GPIO_NR(2, 17),	.flags = 0},	/* EIM_A21 */
	{.label = "powerfail",		.gpio = IMX_GPIO_NR(1, 6),	.flags = GPIOF_DIR_IN},	/* GPIO_6 */
	{.label = "rgb_mirror_vertical", .gpio = IMX_GPIO_NR(2, 27),	.flags = 0},	/* EIM_LBA */
	{.label = "rgb_mirror_horizontal", .gpio = IMX_GPIO_NR(2, 25),	.flags = GPIOF_HIGH},	/* EIM_OE */
#define GP_USB_HUB_RESET			IMX_GPIO_NR(7, 12)
	{.label = "usb_hub_reset",	.gpio = IMX_GPIO_NR(7, 12),	.flags = 0},	/* EIM_OE */
#define GP_AMP_ENABLE				IMX_GPIO_NR(1, 8)
	{.label = "speaker_enable",	.gpio = IMX_GPIO_NR(1, 8),	.flags = 0},	/* EIM_OE */

#define GP_WL_EN				IMX_GPIO_NR(6, 7)	/* NANDF_CLE - active high */
	{.label = "wl_en",		.gpio = IMX_GPIO_NR(6, 7),		.flags = 0},		/* GPIO6[7]: NANDF_CLE - active high */
#define GP_WL_WAKE_IRQ				IMX_GPIO_NR(6, 14)	/* NANDF_CS1 - active low */
	{.label = "wl_wake_irq",	.gpio = IMX_GPIO_NR(6, 14),	.flags = GPIOF_DIR_IN},	/* GPIO6[14]: NANDF_CS1 - active low */
#define GP_WL_BT_REG_EN				IMX_GPIO_NR(6, 15)	/* NANDF_CS2 - active high */
	{.label = "bt_reg_en",		.gpio = IMX_GPIO_NR(6, 15),	.flags = 0},		/* GPIO6[15]: NANDF_CS2 - active high */
#define GP_WL_BT_WAKE_IRQ			IMX_GPIO_NR(6, 16)	/* NANDF_CS3 - active low */
	{.label = "bt_wake_irq",	.gpio = IMX_GPIO_NR(6, 16),	.flags = GPIOF_DIR_IN},	/* GPIO6[16]: NANDF_CS3 - active low */
#define GP_WL_BT_RESET				IMX_GPIO_NR(6, 8)	/* NANDF_ALE - active low */
	{.label = "bt_reset",		.gpio = IMX_GPIO_NR(6, 8),	.flags = 0},		/* GPIO6[8]: NANDF_ALE - active low */
#define GP_WL_CLK_REQ_IRQ			IMX_GPIO_NR(6, 9)	/* NANDF_WP_B - active low */
	{.label = "wl_clk_req_irq",	.gpio = IMX_GPIO_NR(6, 9),	.flags = GPIOF_DIR_IN},	/* GPIO6[9]: NANDF_WP_B - active low */

	{.label = "usbp1", .gpio = IMX_GPIO_NR(2, 26),	.flags = GPIOF_HIGH},	/* EIM_OE */
	{.label = "usbp2", .gpio = IMX_GPIO_NR(3, 20),	.flags = GPIOF_HIGH},	/* EIM_OE */
	{.label = "usbp3", .gpio = IMX_GPIO_NR(5, 2),	.flags = GPIOF_HIGH},	/* EIM_OE */
};

#define GP_HP_DETECT		-1
#define GP_ENET_PHY_INT		IMX_GPIO_NR(1, 28)
#define GP_MIC_DETECT		IMX_GPIO_NR(1, 24)
#define GP_SD4_CD		IMX_GPIO_NR(2, 6)
#define GP_I2C_EN_MIPI		IMX_GPIO_NR(2, 16)
#define GP_I2C_EN_LVDS0		IMX_GPIO_NR(2, 21)
#define GP_I2C_EN_LVDS1		IMX_GPIO_NR(2, 22)
#define GP_I2C_EN_RTC		IMX_GPIO_NR(2, 23)
#define GP_RTC_IRQ		IMX_GPIO_NR(2, 24)
#define GP_ECSPI1_CS1		IMX_GPIO_NR(3, 19)
#define GP_USB_OTG_PWR		IMX_GPIO_NR(3, 22)
#define GP_KEY_ONOFF		IMX_GPIO_NR(3, 30)
#define GP_CAMERA_RESET		IMX_GPIO_NR(5, 4)
#define GP_SD3_CD		IMX_GPIO_NR(7, 0)
#define GP_AR1020_IRQ		IMX_GPIO_NR(7, 1)
#define GP_PCIE_RESET		IMX_GPIO_NR(7, 8)
#define GP_I2C_EN_AR1020	IMX_GPIO_NR(7, 13)


#define WEAK_PULLUP	(PAD_CTL_HYS | PAD_CTL_PKE \
			 | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)

#define WEAK_PULLDOWN	(PAD_CTL_HYS | PAD_CTL_PKE \
			 | PAD_CTL_PUE | PAD_CTL_PUS_100K_DOWN)

#define N6_IRQ_PADCFG		(PAD_CTL_PUE | PAD_CTL_PUS_100K_DOWN | PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_HYS)
#define N6_IRQ_TEST_PADCFG	(PAD_CTL_PKE | N6_IRQ_PADCFG)
#define N6_EN_PADCFG		(PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm)

#include "pads-mx6_h.h"
#define FOR_DL_SOLO
#include "pads-mx6_h.h"

void __init early_console_setup(unsigned long base, struct clk *clk);
static struct clk *sata_clk;

extern char *gp_reg_id;
extern char *soc_reg_id;
extern char *pu_reg_id;
static int caam_enabled;

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

static struct esdhc_platform_data plat_sd2 = {
	.always_present = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.keep_power_at_suspend = 0,
	.caps = MMC_CAP_POWER_OFF_CARD,
	.platform_pad_change = plt_sd_pad_change,
};

static struct esdhc_platform_data plat_sd3 = {
	.cd_gpio = GP_SD3_CD,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct esdhc_platform_data plat_sd4 __initconst = {
	.cd_gpio = GP_SD4_CD,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct anatop_thermal_platform_data
	plat_anatop_thermal __initconst = {
		.name = "anatop_thermal",
};

static const struct imxuart_platform_data plat_uart2 __initconst = {
	.flags      = IMXUART_HAVE_RTSCTS | IMXUART_SDMA,
	.dma_req_rx = MX6Q_DMA_REQ_UART3_RX,
	.dma_req_tx = MX6Q_DMA_REQ_UART3_TX,
};

static int fec_phy_init(struct phy_device *phydev)
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

static struct fec_platform_data plat_fec __initdata = {
	.init = fec_phy_init,
	.phy = PHY_INTERFACE_MODE_RGMII,
	.phy_irq = gpio_to_irq(GP_ENET_PHY_INT)
};

static int spi_cs[] = {
	GP_ECSPI1_CS1,
};

static const struct spi_imx_master plat_spi __initconst = {
	.chipselect     = spi_cs,
	.num_chipselect = ARRAY_SIZE(spi_cs),
};

#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
static struct mtd_partition spi_nor_partitions[] = {
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

static struct flash_platform_data plat_spi_flash = {
	.name = "m25p80",
	.parts = spi_nor_partitions,
	.nr_parts = ARRAY_SIZE(spi_nor_partitions),
	.type = "sst25vf016b",
};
#endif

static struct spi_board_info spi_nor_device[] __initdata = {
#if defined(CONFIG_MTD_M25P80)
	{
		.modalias = "m25p80",
		.max_speed_hz = 20000000, /* max spi clock (SCK) speed in HZ */
		.bus_num = 0,
		.chip_select = 0,
		.platform_data = &plat_spi_flash,
	},
#endif
};

static void spi_device_init(void)
{
	spi_register_board_info(spi_nor_device,
				ARRAY_SIZE(spi_nor_device));
}

static struct mxc_audio_platform_data plat_audio;

static int sgtl5000_init(void)
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

	plat_audio.sysclk = rate;
	clk_set_rate(clko, rate);
	clk_enable(clko);
	return 0;
}

static struct imx_ssi_platform_data plat_ssi = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

static int sgtl5000_amp_enable(int enable)
{
	if (enable)
		gpio_direction_input(GP_AMP_ENABLE);
	else
		gpio_direction_output(GP_AMP_ENABLE, 0);
        return 0;
}

static struct mxc_audio_platform_data plat_audio = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.init = sgtl5000_init,
	.amp_enable = sgtl5000_amp_enable,
	.hp_gpio = GP_HP_DETECT,
};

static struct platform_device platdev_audio_device = {
	.name = "imx-sgtl5000",
};

static struct imxi2c_platform_data plat_i2c = {
	.bitrate = 100000,
};

/*
 **********************************************************************
 */
/* I2C1 (i2c0) bus has a switch */
static const unsigned i2c0_gpiomux_gpios[] = {
	GP_I2C_EN_LVDS0,		/* EIM_A17, i2c3 - edid for LVDS0 */
	GP_I2C_EN_LVDS1,		/* EIM_A16, i2c4 - edid for LVDS1 */
	GP_I2C_EN_RTC,		/* EIM_CS0, i2c5 - RTC isl1208 */
};

static const unsigned i2c0_gpiomux_values[] = {
	1, 2, 4
};

static struct gpio_i2cmux_platform_data plat_i2c0mux = {
	.parent		= 0,
	.base_nr	= 3, /* optional */
	.values		= i2c0_gpiomux_values,
	.n_values	= ARRAY_SIZE(i2c0_gpiomux_values),
	.gpios		= i2c0_gpiomux_gpios,
	.n_gpios	= ARRAY_SIZE(i2c0_gpiomux_gpios),
	.idle		= 0,
};

static struct platform_device platdev_i2c0mux = {
        .name           = "gpio-i2cmux",
        .id             = 0,
        .dev            = {
                .platform_data  = &plat_i2c0mux,
        },
};
/*
 **********************************************************************
 */
/* I2C3 (i2c2) bus has a switch */
static const unsigned i2c2_gpiomux_gpios[] = {
	GP_I2C_EN_MIPI,	/* EIM_A22, i2c6 - mipi camera */
	GP_I2C_EN_AR1020,	/* GPIO_18, i2c7 - touch screen AR1020-1 */
};

static const unsigned i2c2_gpiomux_values[] = {
	1, 2
};

static struct gpio_i2cmux_platform_data plat_i2c2mux = {
	.parent		= 2,
	.base_nr	= 6, /* optional */
	.values		= i2c2_gpiomux_values,
	.n_values	= ARRAY_SIZE(i2c2_gpiomux_values),
	.gpios		= i2c2_gpiomux_gpios,
	.n_gpios	= ARRAY_SIZE(i2c2_gpiomux_gpios),
	.idle		= 0,
};

static struct platform_device platdev_i2c2mux = {
        .name           = "gpio-i2cmux",
        .id             = 1,
        .dev            = {
                .platform_data  = &plat_i2c2mux,
        },
};
/*
 **********************************************************************
 */
static void camera_reset(int reset_gp)
{
	pr_info("%s: reset_gp=0x%x\n", __func__, reset_gp);
	gpio_request(reset_gp, "cam-reset");
	/* Camera reset */
	gpio_direction_output(reset_gp, 0);
	msleep(1);
	gpio_set_value(reset_gp, 1);
}

/*
 * (ov5640 Mipi)
 * EIM_A24 	GPIO[5]:4	Camera reset
 */

static void mipi_camera_io_init(void)
{
	pr_info("%s\n", __func__);

	camera_reset(GP_CAMERA_RESET);
/* for mx6dl, mipi virtual channel 1 connect to csi 1*/
	if (cpu_is_mx6dl())
		mxc_iomux_set_gpr_register(13, 3, 3, 1);
}

static void mipi_camera_powerdown(int powerdown)
{
}

static struct fsl_mxc_camera_platform_data plat_ov5640_mipi = {
	.mclk = 22000000,
	.csi = 0,
	.io_init = mipi_camera_io_init,
	.pwdn = mipi_camera_powerdown,
};

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("sgtl5000", 0x0a),
	},
};

static struct i2c_board_info mxc_i2c3_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("lvds0_edid", 0x50),
	},
};

static struct i2c_board_info mxc_i2c4_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("lvds1_edid", 0x50),
	},
};

static struct i2c_board_info mxc_i2c5_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("isl1208", 0x6f),	/* Real time clock */
		.irq = gpio_to_irq(GP_RTC_IRQ),	/* EIM_CS1 */
	},
};

static struct i2c_board_info mxc_i2c6_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("ov5640_mipi", 0x3c),
		.platform_data = (void *)&plat_ov5640_mipi,
	},
};

static struct i2c_board_info mxc_i2c7_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("ar1020_i2c", 0x4d),		/* Touchscreen */
		.irq = gpio_to_irq(GP_AR1020_IRQ),	/* GPIO_9 */
	},
};

static void usbotg_vbus(bool on)
{
	if (on)
		gpio_set_value(GP_USB_OTG_PWR, 1);
	else
		gpio_set_value(GP_USB_OTG_PWR, 0);
}

static void __init init_usb(void)
{
	int ret = 0;

	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);
	/* disable external charger detect,
	 * or it will affect signal quality at dp .
	 */
	ret = gpio_request(GP_USB_OTG_PWR, "usb-pwr");
	if (ret) {
		pr_err("failed to get GPIO GP_USB_OTG_PWR: %d\n",
			ret);
		return;
	}
	gpio_direction_output(GP_USB_OTG_PWR, 0);
	mxc_iomux_set_gpr_register(1, 13, 1, 1);

	mx6_set_otghost_vbus_func(usbotg_vbus);
}

/* HW Initialization, if return 0, initialization is successful. */
static int board_sata_init(struct device *dev, void __iomem *addr)
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

static void board_sata_exit(struct device *dev)
{
	clk_disable(sata_clk);
	clk_put(sata_clk);
}

static struct ahci_platform_data plat_sata = {
	.init = board_sata_init,
	.exit = board_sata_exit,
};

static struct viv_gpu_platform_data plat_gpu __initdata = {
	.reserved_mem_size = SZ_128M,
};

static struct imx_asrc_platform_data plat_asrc = {
	.channel_bits = 4,
	.clk_map_ver = 2,
};

static struct ipuv3_fb_platform_data plat_fb[] = {
	/*fb0*/
	{
	 .disp_dev = "lcd",
	 .interface_pix_fmt = IPU_PIX_FMT_RGB666,
	 .mode_str = "CLAA-WVGA",
	 .default_bpp = 16,
	 .int_clk = false,
	}, {
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "LDB-XGA",
	.default_bpp = 16,
	.int_clk = false,
	}, {
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "LDB-SVGA",
	.default_bpp = 16,
	.int_clk = false,
	}, {
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "LDB-VGA",
	.default_bpp = 16,
	.int_clk = false,
	},
};

static struct fsl_mxc_lcd_platform_data plat_lcdif = {
	.ipu_id = 0,
	.disp_id = 0,
	.default_ifmt = IPU_PIX_FMT_RGB565,
};

static struct fsl_mxc_ldb_platform_data plat_ldb = {
	.ipu_id = 1,
	.disp_id = 0,
	.ext_ref = 1,
	.mode = LDB_SEP0,
	.sec_ipu_id = 1,
	.sec_disp_id = 1,
};

static struct imx_ipuv3_platform_data plat_ipu[] = {
	{
	.rev = 4,
	.csi_clk[0] = "clko2_clk",
	}, {
	.rev = 4,
	.csi_clk[0] = "clko2_clk",
	},
};

static struct fsl_mxc_capture_platform_data plat_capture = {
	.csi = 1,
	.ipu = 0,
	.mclk_source = 0,
	.is_mipi = 1,
};


static void pm_suspend_enter(void)
{
	/* suspend preparation */
}

static void pm_suspend_exit(void)
{
	/* resume restore */
}

static const struct pm_platform_data plat_pm __initconst = {
	.name = "imx_pm",
	.suspend_enter = pm_suspend_enter,
	.suspend_exit = pm_suspend_exit,
};

#define GPIO_BUTTON(gpio_num, ev_code, act_low, descr, wake)	\
{								\
	.gpio		= gpio_num,				\
	.type		= EV_KEY,				\
	.code		= ev_code,				\
	.active_low	= act_low,				\
	.desc		= "btn " descr,				\
	.wakeup		= wake,					\
}

static struct gpio_keys_button gpio_buttons[] = {
	GPIO_BUTTON(GP_KEY_ONOFF, KEY_POWER, 1, "key-power", 1),
};

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
static struct gpio_keys_platform_data plat_button = {
	.buttons	= gpio_buttons,
	.nbuttons	= ARRAY_SIZE(gpio_buttons),
};

static struct platform_device platdev_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources  = 0,
	.dev		= {
		.platform_data = &plat_button,
	}
};

static void __init add_device_buttons(void)
{
	platform_device_register(&platdev_button_device);
}
#else
static void __init add_device_buttons(void)
{
	int i;
	for (i=0; i < ARRAY_SIZE(gpio_buttons);i++) {
		int gpio = gpio_buttons[i].gpio;
		pr_debug("%s: exporting gpio %d\n", __func__, gpio);
		gpio_export(gpio,1);
	}
}
#endif

static struct regulator_consumer_supply vwifi_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.1"),
};

static struct regulator_init_data vwifi_init = {
	.constraints            = {
		.name           = "VDD_1.8V",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(vwifi_consumers),
	.consumer_supplies = vwifi_consumers,
};

static struct fixed_voltage_config vwifi_reg_config = {
	.supply_name		= "vwifi",
	.microvolts		= 1800000, /* 1.80V */
	.gpio			= GP_WL_EN,
	.startup_delay		= 70000, /* 70ms */
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &vwifi_init,
};

static struct platform_device platdev_vwifi_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 4,
	.dev	= {
		.platform_data = &vwifi_reg_config,
	},
};

static struct regulator_consumer_supply vmmc_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.2"),
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.3"),
};

static struct regulator_init_data vmmc_init = {
	.num_consumer_supplies = ARRAY_SIZE(vmmc_consumers),
	.consumer_supplies = vmmc_consumers,
};

static struct fixed_voltage_config vmmc_reg_config = {
	.supply_name		= "vmmc",
	.microvolts		= 3300000,
	.gpio			= -1,
	.init_data		= &vmmc_init,
};

static struct platform_device platdev_vmmc_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 3,
	.dev	= {
		.platform_data = &vmmc_reg_config,
	},
};

#ifdef CONFIG_SND_SOC_SGTL5000

static struct regulator_consumer_supply sgtl5000_consumer_vdda = {
	.supply = "VDDA",
	.dev_name = "0-000a",
};

static struct regulator_consumer_supply sgtl5000_consumer_vddio = {
	.supply = "VDDIO",
	.dev_name = "0-000a",
};

static struct regulator_init_data sgtl5000_vdda_reg_initdata = {
	.num_consumer_supplies = 1,
	.consumer_supplies = &sgtl5000_consumer_vdda,
};

static struct regulator_init_data sgtl5000_vddio_reg_initdata = {
	.num_consumer_supplies = 1,
	.consumer_supplies = &sgtl5000_consumer_vddio,
};

static struct fixed_voltage_config sgtl5000_vdda_reg_config = {
	.supply_name		= "VDDA",
	.microvolts		= 2500000,
	.gpio			= -1,
	.init_data		= &sgtl5000_vdda_reg_initdata,
};

static struct fixed_voltage_config sgtl5000_vddio_reg_config = {
	.supply_name		= "VDDIO",
	.microvolts		= 3300000,
	.gpio			= -1,
	.init_data		= &sgtl5000_vddio_reg_initdata,
};

static struct platform_device platdev_sgtl5000_vdda_reg = {
	.name	= "reg-fixed-voltage",
	.id	= 0,
	.dev	= {
		.platform_data = &sgtl5000_vdda_reg_config,
	},
};

static struct platform_device platdev_sgtl5000_vddio_reg = {
	.name	= "reg-fixed-voltage",
	.id	= 1,
	.dev	= {
		.platform_data = &sgtl5000_vddio_reg_config,
	},
};

#endif /* CONFIG_SND_SOC_SGTL5000 */

static int init_audio(void)
{
	mxc_register_device(&platdev_audio_device,
			    &plat_audio);
	imx6q_add_imx_ssi(1, &plat_ssi);
#ifdef CONFIG_SND_SOC_SGTL5000
	platform_device_register(&platdev_sgtl5000_vdda_reg);
	platform_device_register(&platdev_sgtl5000_vddio_reg);
#endif
	return 0;
}

struct platform_device *fb_dev[8];

int match_display(struct fb_info *info, const char* search_dev, int instance)
{
	int i;
	struct fb_info *fbi;
	struct ipuv3_fb_platform_data *pdata;
	struct platform_device *pdev;
	int ret = 0;

	if (0) pr_info("%s: %s%d %dx%d\n", __func__, search_dev, instance,
			info->var.xres, info->var.yres);
	for (i= 0 ; i < 8; i++) {
		pdev = fb_dev[i];
		if (!pdev)
			break;
		pdata = pdev->dev.platform_data;
		fbi = platform_get_drvdata(pdev);
		if (pdata) {
			if (0) pr_info("{%s} {%s}\n", pdata->disp_dev, search_dev);
			if (!strcmp(pdata->disp_dev, search_dev)) {
				if (0) pr_info("%p %p %d\n", fbi, info, instance);
				if (!instance) {
					if (!fbi ||(fbi == info))
						ret = 1;
					break;
				}
				instance--;
			}
		}
		if (fbi == info)
			break;
	}
	if (0) pr_info("%s: %s match found\n", __func__, ret ? "" : "no");
	return ret;
}

int bl_di0_notify(struct device *dev, int brightness)
{
	if (0) pr_info("%s: brightness=%d\n", __func__, brightness);
	return brightness;
}

int bl_lvds0_notify(struct device *dev, int brightness)
{
	if (0) pr_info("%s: brightness=%d\n", __func__, brightness);
	return brightness;
}

int bl_lvds1_notify(struct device *dev, int brightness)
{
	if (0) pr_info("%s: brightness=%d\n", __func__, brightness);
	return brightness;
}

int bl_di0_check_fb(struct device *dev, struct fb_info *info)
{
	return match_display(info, "lcd", 0);
}

int bl_lvds0_check_fb(struct device *dev, struct fb_info *info)
{
	return match_display(info, "ldb", 0);
}

int bl_lvds1_check_fb(struct device *dev, struct fb_info *info)
{
	return match_display(info, "ldb", 1);
}

/* PWM1_PWMO: backlight control on DRGB connector */
static struct platform_pwm_backlight_data plat_di0_backlight = {
	.pwm_id = 0,	/* pin SD1_DATA3 - PWM1 */
	.max_brightness = 256,
	.dft_brightness = 128,
	.pwm_period_ns = 50000,
	.notify = bl_di0_notify,
	.check_fb = bl_di0_check_fb,
};

/* PWM4_PWMO: backlight control on LDB connector */
static struct platform_pwm_backlight_data plat_lvds0_backlight = {
	.pwm_id = 3,	/* pin SD1_CMD - PWM4 */
	.max_brightness = 256,
	.dft_brightness = 128,
	.pwm_period_ns = 50000,
	.notify = bl_lvds0_notify,
	.check_fb = bl_lvds0_check_fb,
};

static struct platform_pwm_backlight_data plat_lvds1_backlight = {
	.pwm_id = 2,	/* pin SD1_DAT1 - PWM3 */
	.max_brightness = 256,
	.dft_brightness = 128,
	.pwm_period_ns = 50000,
	.notify = bl_lvds1_notify,
	.check_fb = bl_lvds1_check_fb,
};

static struct mxc_dvfs_platform_data plat_dvfscore = {
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

static struct mipi_csi2_platform_data plat_mipi_csi2 = {
	.ipu_id	 = 0,
	.csi_id = 0,
	.v_channel = 0,
	.lanes = 2,
	.dphy_clk = "mipi_pllref_clk",
	.pixel_clk = "emi_clk",
};

static int __init caam_setup(char *__unused)
{
	caam_enabled = 1;
	return 1;
}
early_param("caam", caam_setup);

static const struct imx_pcie_platform_data plat_pcie  __initconst = {
	.pcie_pwr_en	= -EINVAL,
	.pcie_rst	= GP_PCIE_RESET,
	.pcie_wake_up	= -EINVAL,
	.pcie_dis	= -EINVAL,
};

static struct led_pwm pwms[] = {
	[0] = {
		.name           = "led_pwm0",
		.default_trigger = "what_default_trigger",
		.pwm_id         = 1,
		.active_low     = 0,
		.max_brightness = 0x100,
		.pwm_period_ns  = 3822192,	/* middle "C" is 261.63 hz or */
	},
};

static struct led_pwm_platform_data plat_led = {
	.num_leds = 1,
	.leds = pwms,
};
static struct platform_device platdev_leds_pwd = {
	.name   = "leds_pwm",
	.dev    = {
			.platform_data  = &plat_led,
	},
};


static void poweroff(void)
{
#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
	int waspressed = 0;
	int i;
	for (i=0; i < num_registered_fb; i++) {
		if (registered_fb[i])
                        fb_blank(registered_fb[i],FB_BLANK_POWERDOWN);
	}

	while (1) {
		int pressed=(0 == gpio_get_value(GP_KEY_ONOFF));
		if (!pressed && waspressed) {
			break;
		}
		if (waspressed != pressed) {
			waspressed=pressed;
		} else {
			dsb();
			isb();
		}
	}
	arch_reset('h',"");
#endif
}

/*!
 * Board specific initialization.
 */
static void __init board_init(void)
{
	int i, j;
	struct clk *clko2;
	struct clk *new_parent;
	int rate;
	int ret = gpio_request_array(board_gpios,
			ARRAY_SIZE(board_gpios));

	IOMUX_SETUP(common_pads);

	if (ret) {
		printk(KERN_ERR "%s gpio_request_array failed("
				"%d) for board_gpios\n", __func__, ret);
	}
	printk(KERN_ERR "------------ Board type H\n");

	gp_reg_id = plat_dvfscore.reg_id;
	soc_reg_id = plat_dvfscore.soc_id;
	pu_reg_id = plat_dvfscore.pu_id;

	imx6q_add_imx_uart(0, NULL);
	imx6q_add_imx_uart(1, NULL);
	imx6q_add_imx_uart(2, &plat_uart2);

	imx6q_add_ipuv3(0, &plat_ipu[0]);
	if (cpu_is_mx6q()) {
		imx6q_add_ipuv3(1, &plat_ipu[1]);
		j = ARRAY_SIZE(plat_fb);
	} else {
		j = ARRAY_SIZE(plat_fb) / 2;
		plat_ldb.ipu_id = 0;
		plat_ldb.disp_id = 1;
		plat_ldb.sec_ipu_id = 0;
		plat_ldb.sec_disp_id = 0;
	}
	for (i = 0; i < j; i++)
		fb_dev[i] = imx6q_add_ipuv3fb(i, &plat_fb[i]);

	imx6q_add_vdoa();

	imx6q_add_lcdif(&plat_lcdif);
	imx6q_add_ldb(&plat_ldb);
	imx6q_add_v4l2_output(0);
	imx6q_add_v4l2_capture(1, &plat_capture);
	imx6q_add_mipi_csi2(&plat_mipi_csi2);
	imx6q_add_imx_snvs_rtc();

	if (1 == caam_enabled)
		imx6q_add_imx_caam();

	imx6q_add_imx_i2c(0, &plat_i2c);
	imx6q_add_imx_i2c(2, &plat_i2c);
	i2c_register_board_info(0, mxc_i2c0_board_info,
			ARRAY_SIZE(mxc_i2c0_board_info));

	mxc_register_device(&platdev_i2c0mux, &plat_i2c0mux);
	i2c_register_board_info(3, mxc_i2c3_board_info,
			ARRAY_SIZE(mxc_i2c3_board_info));
	i2c_register_board_info(4, mxc_i2c4_board_info,
			ARRAY_SIZE(mxc_i2c4_board_info));
	i2c_register_board_info(5, mxc_i2c5_board_info,
			ARRAY_SIZE(mxc_i2c5_board_info));

	mxc_register_device(&platdev_i2c2mux, &plat_i2c2mux);
	i2c_register_board_info(6, mxc_i2c6_board_info,
			ARRAY_SIZE(mxc_i2c6_board_info));
	i2c_register_board_info(7, mxc_i2c7_board_info,
			ARRAY_SIZE(mxc_i2c7_board_info));

	/* SPI */
	imx6q_add_ecspi(0, &plat_spi);
	spi_device_init();

	imx6q_add_anatop_thermal_imx(1, &plat_anatop_thermal);
	imx6_init_fec(plat_fec);
	imx6q_add_pm_imx(0, &plat_pm);
	imx6q_add_sdhci_usdhc_imx(2, &plat_sd3);
	imx6q_add_sdhci_usdhc_imx(3, &plat_sd4);
	imx_add_viv_gpu(&imx6_gpu_data, &plat_gpu);
	init_usb();
	if (cpu_is_mx6q())
		imx6q_add_ahci(0, &plat_sata);
	imx6q_add_vpu();
	init_audio();
	platform_device_register(&platdev_vmmc_reg_devices);
	plat_asrc.asrc_core_clk = clk_get(NULL, "asrc_clk");
	plat_asrc.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	imx6q_add_asrc(&plat_asrc);

	/* release USB Hub reset */
	gpio_set_value(GP_USB_HUB_RESET, 1);

	imx6q_add_mxc_pwm(0);	/* RGB backlight */
	imx6q_add_mxc_pwm(1);	/* Buzzer */
	imx6q_add_mxc_pwm(2);	/* LVDS1 baclight */
	imx6q_add_mxc_pwm(3);	/* LVDS0 baclight */

	imx6q_add_mxc_pwm_backlight(0, &plat_di0_backlight);
	imx6q_add_mxc_pwm_backlight(2, &plat_lvds1_backlight);
	imx6q_add_mxc_pwm_backlight(3, &plat_lvds0_backlight);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&plat_dvfscore);

	add_device_buttons();

	clko2 = clk_get(NULL, "clko2_clk");
	if (IS_ERR(clko2))
		pr_err("can't get CLKO2 clock.\n");

	new_parent = clk_get(NULL, "osc_clk");
	if (!IS_ERR(new_parent)) {
		clk_set_parent(clko2, new_parent);
		clk_put(new_parent);
	}
	rate = clk_round_rate(clko2, 24000000);
	clk_set_rate(clko2, rate);
	clk_enable(clko2);
	pm_power_off = poweroff;
	imx6q_add_busfreq();

	gpio_set_value(GP_WL_EN, 1);		/* momentarily enable */
	gpio_set_value(GP_WL_BT_REG_EN, 1);
	mdelay(2);
	gpio_set_value(GP_WL_EN, 0);
	gpio_set_value(GP_WL_BT_REG_EN, 0);

	gpio_free(GP_WL_BT_RESET);
	gpio_free(GP_WL_EN);
	gpio_free(GP_WL_BT_REG_EN);
	mdelay(1);

	imx6q_add_sdhci_usdhc_imx(1, &plat_sd2);

	platform_device_register(&platdev_vwifi_reg_devices);
	platform_device_register(&platdev_leds_pwd);

	imx6q_add_pcie(&plat_pcie);

	imx6q_add_perfmon(0);
	imx6q_add_perfmon(1);
	imx6q_add_perfmon(2);
}

extern void __iomem *twd_base;
static void __init plat_timer_init(void)
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

static struct sys_timer plat_timer = {
	.init   = plat_timer_init,
};

static void __init plat_reserve(void)
{
#if defined(CONFIG_MXC_GPU_VIV) || defined(CONFIG_MXC_GPU_VIV_MODULE)
	phys_addr_t phys;

	if (plat_gpu.reserved_mem_size) {
		phys = memblock_alloc_base(plat_gpu.reserved_mem_size,
					   SZ_4K, SZ_1G);
		memblock_remove(phys, plat_gpu.reserved_mem_size);
		plat_gpu.reserved_mem_base = phys;
	}
#endif
}

/*
 * initialize __mach_desc_MX6Q_H data structure.
 */
MACHINE_START(MX6_H, "Freescale i.MX 6 H Board")
	/* Maintainer: Freescale Semiconductor, Inc. */
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = board_init,
	.timer = &plat_timer,
	.reserve = plat_reserve,
MACHINE_END
