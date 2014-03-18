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
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/regulator/consumer.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/pwm.h>
#include <linux/memblock.h>
#include <linux/gpio.h>
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
#include <mach/imx_rfkill.h>

#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "usb.h"
#include "devices-imx6q.h"
#include "crm_regs.h"
#include "cpu_op-mx6.h"

#if !defined(CONFIG_MACH_MX6_NW_REV2)
#define ST_SD3_CD		IMX_GPIO_NR(7, 0)	/* SD3_DAT5 - active low */
#endif

#define ST_ECSPI1_CS1		IMX_GPIO_NR(3, 19)	/* EIM_D19 - active low */

#define USB_HUB_RESET		IMX_GPIO_NR(4, 15)	/* KEY_ROW4 - active low */
#define USB_OTG_POWER		IMX_GPIO_NR(3, 22)	/* EIM_D22 - power enable for devices: active high */

#define WL_BT_RESET		IMX_GPIO_NR(6, 8)	/* NANDF_ALE - active low */
#define WL_BT_REG_EN		IMX_GPIO_NR(6, 15)	/* NANDF_CS2 - active high */
#define WL_BT_WAKE_IRQ		IMX_GPIO_NR(6, 16)	/* NANDF_CS3 - active low */

#define WL_EN			IMX_GPIO_NR(6, 7)	/* NANDF_CLE - active high */
#define WL_CLK_REQ_IRQ		IMX_GPIO_NR(6, 9)	/* NANDF_WP_B - active low */
#define WL_WAKE_IRQ		IMX_GPIO_NR(6, 14)	/* NANDF_CS1 - active low */

#define WEAK_PULLUP	(PAD_CTL_HYS | PAD_CTL_PKE \
			 | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)
#define WEAK_PULLUP240	(PAD_CTL_HYS | PAD_CTL_PKE \
			 | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP | PAD_CTL_DSE_240ohm)

#include "pads-mx6_nw.h"
#define FOR_DL_SOLO
#include "pads-mx6_nw.h"

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
	{.label = "usb_hub_reset",	.gpio = USB_HUB_RESET,	.flags = 0},		/* GPIO7[12]: GPIO_17 - active low */
	{.label = "usbotg_pwr_en",	.gpio = USB_OTG_POWER,	.flags = 0},		/* GPIO3[22]: EIM_D22 - active high */
	{.label = "wl_en",		.gpio = WL_EN,		.flags = 0},		/* GPIO6[7]: NANDF_CLE - active high */
	{.label = "wl_clk_req_irq",	.gpio = WL_CLK_REQ_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO6[9]: NANDF_WP_B - active low */
	{.label = "wl_wake_irq",	.gpio = WL_WAKE_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO6[14]: NANDF_CS1 - active low */
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
	pr_debug("%s:%s: set power(%d)\n",
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
static struct esdhc_platform_data mx6_sd_data = {
#if !defined(CONFIG_MACH_MX6_NW_REV2)
	.cd_gpio = ST_SD3_CD,
#else
	.always_present = 1,
	.support_18v = 1,
	.support_8bit = 1,
	.cd_gpio = -1, /* always present */
#endif
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct anatop_thermal_platform_data
	mx6_anatop_thermal_data __initconst = {
		.name = "anatop_thermal",
};

static const struct imxuart_platform_data mx6_arm2_uart2_data __initconst = {
	.flags      = IMXUART_HAVE_RTSCTS,
};

static int bt_enable(int enable)
{
	int do_enable= (0 != enable);
	gpio_set_value(WL_BT_REG_EN,do_enable);
	msleep(10*(10*do_enable)); 	/* 10ms for disable, 100 for enable */
	pr_debug("%s: %d\n",__func__,enable);
	return 0;
}

static struct platform_device bt_rfkill = {
	.name = "mxc_bt_rfkill",
};

static struct imx_bt_rfkill_platform_data rfkill_data = {
	.power_change = bt_enable,
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
		I2C_BOARD_INFO("rv4162", 0x68),
	},
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("mxc_hdmi_i2c", 0x50),
	},
};

/*
 **********************************************************************
 */

static void imx6_usbotg_vbus(bool on)
{
	gpio_set_value(USB_OTG_POWER, (0 != on));
}

static void __init imx6_init_usb(void)
{
	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);
	/*
	 * Select GPIO_1 as the OTG_ID pad
	 */
	mxc_iomux_set_gpr_register(1, 13, 1, 1);

	mx6_set_otghost_vbus_func(imx6_usbotg_vbus);
}

static struct viv_gpu_platform_data imx6_gpu_pdata __initdata = {
	.reserved_mem_size = SZ_128M,
};

static struct imx_asrc_platform_data imx_asrc_data = {
	.channel_bits = 4,
	.clk_map_ver = 2,
};

static struct ipuv3_fb_platform_data fb_data[] = {
	{/*fb0*/
	.disp_dev = "hdmi",
	.interface_pix_fmt = IPU_PIX_FMT_RGB24,
	.mode_str = "1920x1080M@60",
	.default_bpp = 32,
	.int_clk = false,
	},
	{/*fb0*/
	.disp_dev = "hdmi",
	.interface_pix_fmt = IPU_PIX_FMT_RGB24,
	.mode_str = "1280x720M@60",
	.default_bpp = 32,
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

static struct imx_ipuv3_platform_data ipu_data = {
	.rev = 4,
};

static void suspend_enter(void)
{
	/* suspend preparation */
}

static void suspend_exit(void)
{
	/* resume restore */
}
static const struct pm_platform_data mx6_pm_data __initconst = {
	.name = "imx_pm",
	.suspend_enter = suspend_enter,
	.suspend_exit = suspend_exit,
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

static struct platform_device vmmc_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 3,
	.dev	= {
		.platform_data = &vmmc_reg_config,
	},
};

static struct mxc_dvfs_platform_data dvfscore_data = {
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

/*!
 * Board specific initialization.
 */
static void __init mx6_board_init(void)
{
	int i, rate;
	struct clk *clko2;
	struct clk *new_parent;
	int ret = gpio_request_array(mx6_init_gpios,
			ARRAY_SIZE(mx6_init_gpios));

	if (ret) {
		printk(KERN_ERR "%s gpio_request_array failed("
				"%d) for mx6_init_gpios\n", __func__, ret);
	}
	IOMUX_SETUP(common_pads);

	gp_reg_id = dvfscore_data.reg_id;
	soc_reg_id = dvfscore_data.soc_id;
	pu_reg_id = dvfscore_data.pu_id;

	imx6q_add_imx_uart(0, NULL);
	imx6q_add_imx_uart(1, NULL);
	imx6q_add_imx_uart(2, &mx6_arm2_uart2_data);

	imx6q_add_mxc_hdmi_core(&hdmi_core_data);
	imx6q_add_ipuv3(0, &ipu_data);
	if (cpu_is_mx6q())
		imx6q_add_ipuv3(1, &ipu_data);
	for (i = 0; i < ARRAY_SIZE(fb_data); i++)
		imx6q_add_ipuv3fb(i, &fb_data[i]);

	imx6q_add_vdoa();
	imx6q_add_v4l2_output(0);
	imx6q_add_imx_snvs_rtc();

	imx6q_add_imx_i2c(0, &mx6_i2c_data);
	imx6q_add_imx_i2c(1, &mx6_i2c_data);

	pr_err ("%s: enabling three I2C devices\n", __func__);
	i2c_register_board_info(0, mxc_i2c0_board_info,
			ARRAY_SIZE(mxc_i2c0_board_info));

	i2c_register_board_info(1, mxc_i2c1_board_info,
			ARRAY_SIZE(mxc_i2c1_board_info));

	/* SPI */
	imx6q_add_ecspi(0, &mx6_spi_data);
	spi_device_init();

	imx6q_add_mxc_hdmi(&hdmi_data);

	imx6q_add_anatop_thermal_imx(1, &mx6_anatop_thermal_data);
	imx6q_add_pm_imx(0, &mx6_pm_data);
#if !defined(CONFIG_MACH_MX6_NW_REV2)
	imx6q_add_sdhci_usdhc_imx(2, &mx6_sd_data);
#else
	imx6q_add_sdhci_usdhc_imx(3, &mx6_sd_data);
#endif
	imx_add_viv_gpu(&imx6_gpu_data, &imx6_gpu_pdata);
	imx6_init_usb();
	imx6q_add_vpu();
	platform_device_register(&vmmc_reg_devices);
	imx_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	imx_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	imx6q_add_asrc(&imx_asrc_data);

	/* release USB Hub reset */
	gpio_set_value(USB_HUB_RESET, 1);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&dvfscore_data);

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

	gpio_request(WL_EN, "wl-en");
	gpio_request(WL_BT_REG_EN, "bt-reg-en");
	gpio_request(WL_BT_RESET, "bt-reset");

	gpio_direction_output(WL_EN, 1);		/* momentarily enable */
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
	mxc_register_device(&bt_rfkill, &rfkill_data);
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
MACHINE_START(MX6_NIT6XLITE, "Boundary Devices NW Board")
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = mx6_board_init,
	.timer = &mx6_timer,
	.reserve = mx6_reserve,
MACHINE_END
