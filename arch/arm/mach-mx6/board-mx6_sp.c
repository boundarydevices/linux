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
#include <mach/ipu-v3.h>

#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "usb.h"
#include "devices-imx6q.h"
#include "crm_regs.h"
#include "cpu_op-mx6.h"

#define GP_USB_OTG_PWR	IMX_GPIO_NR(3, 22)

#define DISP_I2C_EN		IMX_GPIO_NR(2, 17)	/* EIM_A21 - active high */
#define DISP_BACKLIGHT_12V_EN	IMX_GPIO_NR(4, 5)	/* GPIO_19 - active high */

#define ENET_PHY_RESET		IMX_GPIO_NR(1, 27)	/* ENET_RXD0 - active low */
#define ENET_PHY_IRQ		IMX_GPIO_NR(1, 28)	/* ENET_TX_EN - active low */

#define RTC_I2C_EN		IMX_GPIO_NR(2, 23)	/* EIM_CS0 - active high */
#define RTC_IRQ			IMX_GPIO_NR(2, 26)	/* EIM_RW - active low */

#define ST_SD3_CD		IMX_GPIO_NR(7, 0)	/* SD3_DAT5 - active low */
#define ST_ECSPI1_CS1		IMX_GPIO_NR(3, 19)	/* EIM_D19 - active low */

#define TOUCH_RESET		IMX_GPIO_NR(1, 4)	/* GPIO_4 - active low */
#define TOUCH_IRQ		IMX_GPIO_NR(7, 1)	/* SD3_DAT4 - active low */

#define CAP_TCH_INT		IMX_GPIO_NR(1, 9)	/* GPIO_9 - J7: pin 4: active low */

#define USB_HUB_RESET		IMX_GPIO_NR(7, 12)	/* GPIO_17 - active low */

#define EMMC_RESET		IMX_GPIO_NR(2, 7)	/* NANDF_D7 - active low */

#define WL_BT_RESET		IMX_GPIO_NR(6, 8)	/* NANDF_ALE - active low */
#define WL_BT_REG_EN		IMX_GPIO_NR(6, 15)	/* NANDF_CS2 - active high */
#define WL_BT_WAKE_IRQ		IMX_GPIO_NR(6, 16)	/* NANDF_CS3 - active low */

#define WL_EN			IMX_GPIO_NR(6, 7)	/* NANDF_CLE - active high */
#define WL_CLK_REQ_IRQ		IMX_GPIO_NR(6, 9)	/* NANDF_WP_B - active low */
#define WL_WAKE_IRQ		IMX_GPIO_NR(6, 14)	/* NANDF_CS1 - active low */

#define MX6_N6L_GLED			IMX_GPIO_NR(1, 2)	/* J14 pin1: GPIO2 */
#define MX6_N6L_RLED			IMX_GPIO_NR(1, 3)	/* J14 pin3: GPIO3 */
#define MX6_N6L_DRYCONTACT		IMX_GPIO_NR(1, 6)	/* J14 pins 8 and 9 - dry contact */
#define MX6_N6L_DRYCONTACT2		IMX_GPIO_NR(1, 7)	/* J46 pins 2 and 3 - dry contact */
#define MX6_N6L_VOLUP			IMX_GPIO_NR(7, 13)	/* J14 pin5: GPIO_18 */
#define MX6_N6L_VOLDOWN			IMX_GPIO_NR(4, 5)	/* J14 pin7: GPIO_19 */

#include "pads-mx6_sp.h"
#define FOR_DL_SOLO
#include "pads-mx6_sp.h"

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
//	{.label = "edid_i2c_en",	.gpio = DISP_I2C_EN,	.flags = 0},		/* GPIO2[17]: EIM_A21 - active high */
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

	{.label = "emmc_reset"	,	.gpio = EMMC_RESET,		.flags = 0},
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

static const struct esdhc_platform_data mx6_sd4_data __initconst = {
	.cd_gpio = -1,
	.wp_gpio = -1,
	.always_present = 1,
	.keep_power_at_suspend = 1,
	.support_8bit = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct anatop_thermal_platform_data
	mx6_anatop_thermal_data __initconst = {
		.name = "anatop_thermal",
};

static const struct imxuart_platform_data mx6_arm2_uart2_data __initconst = {
	.flags      = IMXUART_HAVE_RTSCTS,
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

static struct imxi2c_platform_data mx6_i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("ar1020_i2c", 0x4d),	/* Touchscreen */
		.irq = gpio_to_irq(TOUCH_IRQ),		/* GPIO_7 */
	},
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
};

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("isl1208", 0x6f),	/* Real time clock */
	},
};

/*
 **********************************************************************
 */

static void imx6_usbotg_vbus(bool on)
{
	gpio_set_value(GP_USB_OTG_PWR, on ? 1 : 0);
}

static void __init imx6_init_usb(void)
{
	int ret = 0;

	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);
	/* disable external charger detect,
	 * or it will affect signal quality at dp .
	 */
	ret = gpio_request(GP_USB_OTG_PWR, "usb-pwr");
	if (ret) {
		pr_err("failed to get GPIO USB_OTG_PWR: %d\n",
			ret);
		return;
	}
	gpio_direction_output(GP_USB_OTG_PWR, 0);
	mxc_iomux_set_gpr_register(1, 13, 1, 1);

	mx6_set_otghost_vbus_func(imx6_usbotg_vbus);
}

static struct viv_gpu_platform_data imx6_gpu_pdata __initdata = {
	.reserved_mem_size = SZ_128M,
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

int mx6_bl_notify(struct device *dev, int brightness)
{
	pr_info("%s: brightness=%d\n", __func__, brightness);
	gpio_set_value(DISP_BACKLIGHT_12V_EN, brightness ? 1 : 0);
	return brightness;
}

/* PWM3_PWMO: backlight control on RGB connector */
static struct platform_pwm_backlight_data mx6_pwm3_backlight_data = {
	.pwm_id = 2,	/* pin SD1_DAT1 - PWM3 */
	.max_brightness = 256,
	.dft_brightness = 256,
	.pwm_period_ns = 50000,
	.notify = mx6_bl_notify,
};

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

static struct led_pwm pwms[] = {
	[0] = {
		.name           = "buzzer",
		.default_trigger = "what_default_trigger",
		.pwm_id         = 0,
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
	imx6q_add_imx_uart(3, NULL);

	if (!cpu_is_mx6q()) {
		ldb_data.ipu_id = 0;
		ldb_data.sec_ipu_id = 0;
	}

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

	imx6q_add_anatop_thermal_imx(1, &mx6_anatop_thermal_data);
	imx6q_add_pm_imx(0, &mx6_pm_data);
	imx6q_add_sdhci_usdhc_imx(2, &mx6_sd3_data);
	imx6q_add_sdhci_usdhc_imx(3, &mx6_sd4_data);
	imx_add_viv_gpu(&imx6_gpu_data, &imx6_gpu_pdata);
	imx6_init_usb();
	imx6q_add_vpu();
	platform_device_register(&oc_vmmc_reg_devices);

	/* release USB Hub reset */
	gpio_set_value(USB_HUB_RESET, 1);

	imx6q_add_mxc_pwm(0);
	platform_device_register(&platdev_leds_pwd);

	imx6q_add_mxc_pwm(3);
	imx6q_add_mxc_pwm_backlight(3, &mx6_pwm4_backlight_data);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&oc_dvfscore_data);

	add_device_buttons();

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

	gpio_request(WL_EN, "wl-en");
	gpio_request(WL_BT_REG_EN, "bt-reg-en");
	gpio_request(WL_BT_RESET, "bt-reset");

	gpio_direction_output(WL_EN, 1);	/* momentarily enable */
	gpio_direction_output(WL_BT_REG_EN, 1);
	gpio_direction_output(WL_BT_RESET, 1);

	mdelay(2);
	gpio_set_value(WL_EN, 0);

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
MACHINE_START(MX6_SP, "Boundary Devices SP Board")
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = mx6_board_init,
	.timer = &mx6_timer,
	.reserve = mx6_reserve,
MACHINE_END
