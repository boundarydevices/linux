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
#include <mach/system.h>
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

#define ENET_PHY_RESET		IMX_GPIO_NR(1, 27)	/* ENET_RXD0 - active low */
#define ENET_PHY_IRQ		IMX_GPIO_NR(1, 28)	/* ENET_TX_EN - active low */

#define RTC_I2C_EN		IMX_GPIO_NR(2, 23)	/* EIM_CS0 - active high */
#define RTC_IRQ			IMX_GPIO_NR(2, 24)	/* EIM_CS1 - active low */

#define ST_EMMC_RESET		IMX_GPIO_NR(2, 5)	/* NANDF_D5 - active low */
#define ST_SD3_CD		IMX_GPIO_NR(7, 0)	/* SD3_DAT5 - active low */
#define ST_SD2_CD		IMX_GPIO_NR(7, 8)	/* SD3_RST - active low */
#define ST_ECSPI1_CS1		IMX_GPIO_NR(3, 19)	/* EIM_D19 - active low */

#define TOUCH_RESET		IMX_GPIO_NR(1, 4)	/* GPIO_4 - active low */
#define TOUCH_IRQ		IMX_GPIO_NR(1, 9)	/* GPIO_9 - active low */

#define USB_HUB_RESET		IMX_GPIO_NR(7, 12)	/* GPIO_17 - active low */

#define AR1021_5WIRE		IMX_GPIO_NR(7, 1)	/* SD3_DAT4 - low is 4-wire */

#define GP_KEY_ONOFF		IMX_GPIO_NR(4, 15)

#include "pads-mx6_mcs.h"
#define FOR_DL_SOLO
#include "pads-mx6_mcs.h"

extern char *soc_reg_id;
extern char *pu_reg_id;

extern struct regulator *(*get_cpu_regulator)(void);
extern void (*put_cpu_regulator)(void);

#define IOMUX_SETUP(pad_list)	mxc_iomux_v3_setup_pads(mx6q_##pad_list, \
		mx6dl_solo_##pad_list)

static int mxc_iomux_v3_setup_pads(iomux_v3_cfg_t *mx6q_pad_list,
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
struct gpio init_gpios[] __initdata = {
	{.label = "phy_reset",		.gpio = ENET_PHY_RESET,	.flags = GPIOF_HIGH},	/* GPIO1[27]: ENET_RXD0 - active low */
	{.label = "rtc_irq",		.gpio = RTC_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO2[24]:* EIM_CS1 - active low */
	{.label = "emmc_reset",		.gpio = ST_EMMC_RESET,	.flags = GPIOF_HIGH},	/* GPIO2[5]: NANDF_D5 - active low */
	{.label = "sd3_cd",		.gpio = ST_SD3_CD,	.flags = GPIOF_DIR_IN},	/* GPIO7[0]: SD3_DAT5 - active low */
	{.label = "touch_reset",	.gpio = TOUCH_RESET,	.flags = GPIOF_HIGH},	/* GPIO1[4]: GPIO_4 - active low */
	{.label = "touch_irq",		.gpio = TOUCH_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO1[9]: GPIO_9 - active low */
	{.label = "usb_hub_reset",	.gpio = USB_HUB_RESET,	.flags = 0},		/* GPIO7[12]: GPIO_17 - active low */
	{.label = "ar1021_4wire",	.gpio = AR1021_5WIRE,	.flags = GPIOF_HIGH},	/* GPIO7[1]: SD3_DAT4 - default to 5-wire */
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

/* Top SD card */
static struct esdhc_platform_data sd3_data = {
	.cd_gpio = ST_SD3_CD,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

/* Bottom SD card */
static struct esdhc_platform_data sd2_data = {
	.cd_gpio = ST_SD2_CD,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

/* EMMC */
static const struct esdhc_platform_data sd4_data __initconst = {
	.always_present = 1,
	.support_18v = 1,
	.support_8bit = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct anatop_thermal_platform_data
	anatop_thermal_data __initconst = {
		.name = "anatop_thermal",
};

#ifdef CONFIG_FEC
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

static struct fec_platform_data fec_data __initdata = {
	.init = fec_phy_init,
	.phy = PHY_INTERFACE_MODE_RGMII,
	.phy_irq = gpio_to_irq(ENET_PHY_IRQ)
};
#endif

static int spi_cs[] = {
	ST_ECSPI1_CS1,
};

static const struct spi_imx_master spi_data __initconst = {
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

static struct flash_platform_data _spi_flash_data = {
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
		.platform_data = &_spi_flash_data,
	},
#endif
};

static void spi_device_init(void)
{
	spi_register_board_info(spi_nor_device,
				ARRAY_SIZE(spi_nor_device));
}

static struct imxi2c_platform_data i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("rv4162", 0x68),
	},
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
};

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("ar1020_i2c", 0x4d),	/* Touchscreen */
		.irq = gpio_to_irq(TOUCH_IRQ),		/* GPIO_9 */
	},
};

static void usbotg_vbus(bool on)
{
}

static void __init init_usb(void)
{
	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);
	/* disable external charger detect,
	 * or it will affect signal quality at dp .
	 */
	mxc_iomux_set_gpr_register(1, 13, 1, 1);

	mx6_set_otghost_vbus_func(usbotg_vbus);
}

static struct viv_gpu_platform_data gpu_pdata __initdata = {
	.reserved_mem_size = SZ_128M,
};

static struct ipuv3_fb_platform_data oc_fb_data = {
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "LDB-WXGA",
	.default_bpp = 16,
	.int_clk = false,
};

static struct fsl_mxc_ldb_platform_data ldb_data = {
	.ipu_id = 0,
	.disp_id = 0,
	.ext_ref = 1,
	.mode = LDB_SEP0,
	.sec_ipu_id = 0,
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
};


static void oc_suspend_enter(void)
{
	/* suspend preparation */
}

static void oc_suspend_exit(void)
{
	/* resume restore */
}
static const struct pm_platform_data pm_data __initconst = {
	.name = "imx_pm",
	.suspend_enter = oc_suspend_enter,
	.suspend_exit = oc_suspend_exit,
};

static struct regulator_consumer_supply oc_vmmc_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.1"),
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

int bl_notify(struct device *dev, int brightness)
{
	pr_info("%s: brightness=%d\n", __func__, brightness);
	return brightness;
}

/* PWM4_PWMO: backlight control on LDB connector */
static struct platform_pwm_backlight_data pwm4_backlight_data = {
	.pwm_id = 3,	/* pin SD1_CMD - PWM4 */
	.max_brightness = 256,
	.dft_brightness = 128,
	.pwm_period_ns = 50000,
	.notify = bl_notify,
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
	.pcie_rst	= -EINVAL,
	.pcie_wake_up	= -EINVAL,
	.pcie_dis	= -EINVAL,
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

static struct gpio_keys_button buttons[] = {
	GPIO_BUTTON(IMX_GPIO_NR(4, 15), KEY_POWER, 0, "power", 1),
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

#define M_TX_EN		1

static const unsigned short uart3_gpios[] = {
	IMX_GPIO_NR(2, 26),		/* 1 - tx enable, active high */
	-1
};

static const struct imxuart_platform_data uart3_data __initconst = {
	.gpios = uart3_gpios,
	.flags = IMXUART_RS485_HALF_DUPLEX | IMXUART_RS485_MODE,
	.rs485_txen_mask = M_TX_EN,
	.rs485_txen_levels = M_TX_EN,
};

static const unsigned short uart4_gpios[] = {
	IMX_GPIO_NR(2, 25),		/* 1 - tx enable, active high */
	-1
};

static const struct imxuart_platform_data uart4_data __initconst = {
	.gpios = uart4_gpios,
	.flags = IMXUART_RS485_HALF_DUPLEX | IMXUART_RS485_MODE,
	.rs485_txen_mask = M_TX_EN,
	.rs485_txen_levels = M_TX_EN,
};

static const unsigned short uart5_gpios[] = {
	IMX_GPIO_NR(2, 27),		/* 1 - tx enable, active high */
	-1
};

static const struct imxuart_platform_data uart5_data __initconst = {
	.gpios = uart5_gpios,
	.flags = IMXUART_RS485_HALF_DUPLEX | IMXUART_RS485_MODE,
	.rs485_txen_mask = M_TX_EN,
	.rs485_txen_levels = M_TX_EN,
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
	struct clk *clko2;
	struct clk *new_parent;
	int rate;
	int ret = gpio_request_array(init_gpios,
			ARRAY_SIZE(init_gpios));

	if (ret) {
		printk(KERN_ERR "%s gpio_request_array failed("
				"%d) for init_gpios\n", __func__, ret);
	}
	IOMUX_SETUP(common_pads);


	gp_reg_id = oc_dvfscore_data.reg_id;
	soc_reg_id = oc_dvfscore_data.soc_id;
	pu_reg_id = oc_dvfscore_data.pu_id;

	imx6q_add_imx_uart(0, NULL);
	imx6q_add_imx_uart(1, NULL);
	imx6q_add_imx_uart(2, &uart3_data);
	imx6q_add_imx_uart(3, &uart4_data);
	imx6q_add_imx_uart(4, &uart5_data);

	imx6q_add_ipuv3(0, &ipu_data[0]);
	imx6q_add_ipuv3fb(0, &oc_fb_data);

	imx6q_add_vdoa();
	imx6q_add_ldb(&ldb_data);
	imx6q_add_v4l2_output(0);
	imx6q_add_v4l2_capture(0, &capture_data);
	imx6q_add_imx_snvs_rtc();

	imx6q_add_imx_i2c(0, &i2c_data);
	imx6q_add_imx_i2c(2, &i2c_data);
	i2c_register_board_info(0, mxc_i2c0_board_info,
			ARRAY_SIZE(mxc_i2c0_board_info));
	i2c_register_board_info(1, mxc_i2c1_board_info,
			ARRAY_SIZE(mxc_i2c1_board_info));
	i2c_register_board_info(2, mxc_i2c2_board_info,
			ARRAY_SIZE(mxc_i2c2_board_info));

	/* SPI */
	imx6q_add_ecspi(0, &spi_data);
	spi_device_init();

	imx6q_add_anatop_thermal_imx(1, &anatop_thermal_data);
#ifdef CONFIG_FEC
	imx6_init_fec(fec_data);
#endif

	imx6q_add_pm_imx(0, &pm_data);
	imx6q_add_sdhci_usdhc_imx(2, &sd3_data);
	imx6q_add_sdhci_usdhc_imx(1, &sd2_data);
	imx6q_add_sdhci_usdhc_imx(3, &sd4_data);
	imx_add_viv_gpu(&imx6_gpu_data, &gpu_pdata);
	init_usb();
	imx6q_add_vpu();
	platform_device_register(&oc_vmmc_reg_devices);

	/* release USB Hub reset */
	gpio_set_value(USB_HUB_RESET, 1);

	pr_err("AR1021: %d-wire\n",
	       4+gpio_get_value(AR1021_5WIRE));

	imx6q_add_mxc_pwm(3);

	imx6q_add_mxc_pwm_backlight(3, &pwm4_backlight_data);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&oc_dvfscore_data);

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
	pm_power_off = poweroff;
	imx6q_add_busfreq();
	imx6q_add_pcie(&pcie_data);

	platform_device_register(&button_device);

	imx6q_add_perfmon(0);
	imx6q_add_perfmon(1);
	imx6q_add_perfmon(2);
}

extern void __iomem *twd_base;
static void __init timer_init(void)
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

static struct sys_timer timer = {
	.init   = timer_init,
};

static void __init reserve(void)
{
#if defined(CONFIG_MXC_GPU_VIV) || defined(CONFIG_MXC_GPU_VIV_MODULE)
	phys_addr_t phys;

	if (gpu_pdata.reserved_mem_size) {
		phys = memblock_alloc_base(gpu_pdata.reserved_mem_size,
					   SZ_4K, SZ_1G);
		memblock_remove(phys, gpu_pdata.reserved_mem_size);
		gpu_pdata.reserved_mem_base = phys;
	}
#endif
}

/*
 * initialize __mach_desc_MX6Q_MCS data structure.
 */
MACHINE_START(MX6_MCS, "Freescale i.MX 6 MCS Board")
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = board_init,
	.timer = &timer,
	.reserve = reserve,
MACHINE_END
