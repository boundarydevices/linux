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
#include <linux/micrel_phy.h>
#include <linux/mutex.h>
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
#include <mach/imx_rfkill.h>
#include <mach/imx-uart.h>
#include <mach/viv_gpu.h>
#include <mach/ahci_sata.h>
#include <mach/ipu-v3.h>
#include <mach/mxc_hdmi.h>
#include <mach/system.h>
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

#include "pads-mx6_bt.h"
#define FOR_DL_SOLO
#include "pads-mx6_bt.h"

void __init early_console_setup(unsigned long base, struct clk *clk);

extern char *gp_reg_id;
extern char *soc_reg_id;
extern char *pu_reg_id;

extern struct regulator *(*get_cpu_regulator)(void);
extern void (*put_cpu_regulator)(void);

#define IOMUX_SETUP(pad_list)	iomux_v3_setup_pads(mx6q_##pad_list, \
		mx6dl_solo_##pad_list)

static int iomux_v3_setup_pads(iomux_v3_cfg_t *mx6q_pad_list,
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
	int i = index * SD_SPEED_CNT;

	if ((index < 0) || (index > 2)) {
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

static struct esdhc_platform_data sd1_data = {
	.cd_gpio = GP_SD1_CD,
	.wp_gpio = GP_SD1_WP,
//	.support_18v = 1;
	.keep_power_at_suspend = 0,
	.caps = MMC_CAP_POWER_OFF_CARD,
	.platform_pad_change = plt_sd_pad_change,
};

static struct esdhc_platform_data sd2_data = {
	.cd_gpio = GP_SD2_CD,
	.wp_gpio = -1,
	.platform_pad_change = plt_sd_pad_change,
};

static struct esdhc_platform_data sd3_data = {
	.cd_gpio = -1,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.always_present = 1,
	.support_8bit = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct anatop_thermal_platform_data
	anatop_thermal_data __initconst = {
		.name = "anatop_thermal",
};

static unsigned short ksz9031_por_cmds[] = {
	0x0204, 0x0,		/* RX_CTL/TX_CTL output pad skew */
	0x0205, 0x0,		/* RXDn pad skew */
	0x0206, 0x0,		/* TXDn pad skew */
	0x0208, 0x03ff,		/* TXC/RXC pad skew */
	0x0, 0x0
};

static int ksz9031_send_phy_cmds(struct phy_device *phydev, unsigned short* p)
{
	for (;;) {
		unsigned reg = *p++;
		unsigned val = *p++;
		if (reg == 0 && val == 0)
			break;
		if (reg < 32) {
			phy_write(phydev, reg, val);
		} else {
			unsigned dev_addr = (reg >> 8) & 0x7f;
			phy_write(phydev, 0x0d, dev_addr);
			phy_write(phydev, 0x0e, reg & 0xff);
			phy_write(phydev, 0x0d, dev_addr | 0x8000);
			phy_write(phydev, 0x0e, val);
		}
	}
	return 0;
}


static int fec_phy_init(struct phy_device *phydev)
{
	if ((phydev->phy_id & 0x00fffff0) == PHY_ID_KSZ9031) {
		ksz9031_send_phy_cmds(phydev, ksz9031_por_cmds);
		return 0;
	}
	/* KSZ9021 */
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
	.phy_irq = gpio_to_irq(GP_ENET_PHY_INT)
};

static int ecspi1_cs[] = {
	GP_ECSPI1_CS1,
};

static const struct spi_imx_master ecspi1_data __initconst = {
	.chipselect     = ecspi1_cs,
	.num_chipselect = ARRAY_SIZE(ecspi1_cs),
};

static int ecspi3_cs[] = {
	GP_ECSPI3_GS2971_CS,
};

static const struct spi_imx_master ecspi3_data __initconst = {
	.chipselect     = ecspi3_cs,
	.num_chipselect = ARRAY_SIZE(ecspi3_cs),
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

static struct flash_platform_data spi_flash_data = {
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
		.platform_data = &spi_flash_data,
	},
#endif
};

static struct fsl_mxc_camera_platform_data gs2971_data;

static void gs2971_io_init(void)
{

	pr_info("%s\n", __func__);
	if (gs2971_data.cea861)
		IOMUX_SETUP(gs2971_video_pads_cea861);
	else
		IOMUX_SETUP(gs2971_video_pads_no_cea861);
	gpio_set_value(GP_GS2971_RESET, 0);


	/* Enable parallel port to IPU1/CSI1 */
	if (cpu_is_mx6q())
		mxc_iomux_set_gpr_register(1, 20, 1, 1);
	else
		mxc_iomux_set_gpr_register(13, 3, 3, 4);

	/* Set control pin values */
	gpio_set_value(GP_GS2971_TIM_861, 0);
	gpio_set_value(GP_GS2971_IOPROC_EN, 1);
	gpio_set_value(GP_GS2971_SW_EN, 0);
	gpio_set_value(GP_GS2971_STANDBY, 1);
	msleep(1);
	gpio_set_value(GP_GS2971_RESET, 1);
}

static void gs2971_pwdn(int powerdown)
{
	printk(KERN_ERR "%s: reset=gp%d, standby=%d, powerdown=%d\n", __func__, GP_GS2971_RESET, GP_GS2971_STANDBY, powerdown);

	gpio_set_value(GP_GS2971_STANDBY, powerdown ? 1 : 0);
	if (!powerdown)
		msleep(1000);
}

static struct fsl_mxc_camera_platform_data gs2971_data = {
	.mclk = 27000000,
	.pwdn = gs2971_pwdn,
	.io_init = gs2971_io_init,
	.ipu = 1,
	.csi = 1,
	.cea861 = 0,
};

static struct spi_board_info spi_gs2971_device[] __initdata = {
	{
		.modalias = "gs2971",
		.max_speed_hz = 20000000, /* max spi clock (SCK) speed in HZ */
		.bus_num = 2,
		.chip_select = 0,
		.platform_data = &gs2971_data,
	},
};

static void spi_device_init(void)
{
	spi_register_board_info(spi_nor_device,
				ARRAY_SIZE(spi_nor_device));
	spi_register_board_info(spi_gs2971_device, ARRAY_SIZE(spi_gs2971_device));
}

static int gs2971_audio_init(void)
{
	return 0;
}

static struct imx_ssi_platform_data ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

static struct mxc_audio_platform_data audio_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 4,
	.init = gs2971_audio_init,
	.hp_gpio = -1,
	.sysclk = 48000 * 16,
};

static struct platform_device audio_device = {
	.name = "gs2971",
};

static struct imxi2c_platform_data i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info i2c0_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("rv4162", 0x68),
		.irq = gpio_to_irq(GP_RTC_RV4162_IRQ),
	},
};

static struct i2c_board_info i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("mxc_hdmi_i2c", 0x50),
	},
	{
		I2C_BOARD_INFO("sc16is7xx-uart", 0x49),
		.irq = gpio_to_irq(GP_SC16IS752_IRQ),
	},
};

static void adv7391_enable_pins(void)
{
	pr_info("%s\n", __func__);
	IOMUX_SETUP(lcd_pads_enable);
	gpio_set_value(GP_ADV7391_RESET, 1);
}

static void adv7391_disable_pins(void)
{
	pr_info("%s\n", __func__);
	gpio_set_value(GP_ADV7391_RESET, 0);
	IOMUX_SETUP(lcd_pads_disable);
}

static struct fsl_mxc_lcd_platform_data adv7391_data = {
	.ipu_id = 0,
	.disp_id = 0,
	.default_ifmt = IPU_PIX_FMT_BT656,
	.enable_pins = adv7391_enable_pins,
	.disable_pins = adv7391_disable_pins,
};

static struct i2c_board_info i2c2_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("mxc_adv739x", 0x2a),
		.platform_data = (void *)&adv7391_data,
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

static struct imx_asrc_platform_data imx_asrc_data = {
	.channel_bits = 4,
	.clk_map_ver = 2,
};

static struct ipuv3_fb_platform_data fb_data[] = {
	{ /*fb0*/
	.disp_dev = "hdmi",
	.interface_pix_fmt = IPU_PIX_FMT_RGB24,
	.mode_str = "1280x720M@60",
	.default_bpp = 16,
	.int_clk = false,
	},
	{ /*fb1*/
	.disp_dev = "adv739x",
	.interface_pix_fmt = IPU_PIX_FMT_BT656,
	.mode_str = "BT656-NTSC",
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

/* i2c2 iomux with hdmi ddc,
 * the pins default work at i2c2 function,
 * when hdcp enable, the pins should work at ddc function
 */

static void hdmi_enable_ddc_pin(void)
{
	IOMUX_SETUP(hdmi_ddc_pads);
}

static void hdmi_disable_ddc_pin(void)
{
	IOMUX_SETUP(i2c2_pads);
}

static struct fsl_mxc_hdmi_platform_data hdmi_data = {
	.init = hdmi_init,
	.enable_pins = hdmi_enable_ddc_pin,
	.disable_pins = hdmi_disable_ddc_pin,
};

static struct fsl_mxc_hdmi_core_platform_data hdmi_core_data = {
	.ipu_id = 1,
	.disp_id = 0,
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

static struct fsl_mxc_capture_platform_data capture_data[] = {
	{
		.ipu = 1,	/* GS2971 */
		.csi = 1,
		.mclk_source = 0,
	},
};


static void suspend_enter(void)
{
	/* suspend preparation */
}

static void suspend_exit(void)
{
	/* resume restore */
}
static const struct pm_platform_data pm_data __initconst = {
	.name = "imx_pm",
	.suspend_enter = suspend_enter,
	.suspend_exit = suspend_exit,
};



static struct regulator_consumer_supply vmmc_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.0"),
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.1"),
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.2"),
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

static int init_audio(void)
{
	mxc_register_device(&audio_device,
			    &audio_data);
	imx6q_add_imx_ssi(1, &ssi_pdata);
	return 0;
}

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

static void __init fixup_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
}

static struct imx_pcie_platform_data pcie_data = {
	.pcie_pwr_en	= -EINVAL,
	.pcie_rst	= GP_PCIE_RESET,
	.pcie_wake_up	= -EINVAL,
	.pcie_dis	= -EINVAL,
};

#define GPIOF_HIGH GPIOF_OUT_INIT_HIGH

static struct gpio initial_gpios[] __initdata = {
	{.label = "emmc_reset",		.gpio = GP_EMMC_RESET,		.flags = 0},
	{.label = "usb-pwr",		.gpio = GP_USB_OTG_PWR,		.flags = 0},
	{.label = "usb-hub_reset",	.gpio = GP_USB_HUB_RESET,	.flags = 0},
	{.label = "ax88772a_reset",	.gpio = GP_AX88772A_RESET,	.flags = 0},
	{.label = "sd1_power_sel",	.gpio = GP_SD1_POWER_SEL,	.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "gs-smpte",		.gpio = GP_GS2971_SMPTE_BYPASS,	.flags = GPIOF_DIR_IN},
	{.label = "gs-reset",		.gpio = GP_GS2971_RESET,	.flags = 0},
	{.label = "gs-dvi_lock",	.gpio = GP_GS2971_DVI_LOCK,	.flags = GPIOF_DIR_IN},
	{.label = "gs-data_err",	.gpio = GP_GS2971_DATA_ERR,	.flags = GPIOF_DIR_IN},
	{.label = "gs-lb_cont",		.gpio = GP_GS2971_LB_CONT,	.flags = GPIOF_DIR_IN},
	{.label = "gs-y_1anc",		.gpio = GP_GS2971_Y_1ANC,	.flags = GPIOF_DIR_IN},
	{.label = "gs-rc_bypas",	.gpio = GP_GS2971_RC_BYPASS,	.flags = 0},
	{.label = "gs-ioproc_en",	.gpio = GP_GS2971_IOPROC_EN,	.flags = 0},
	{.label = "gs-audio_en",	.gpio = GP_GS2971_AUDIO_EN,	.flags = 0},
	{.label = "gs-tim_861",		.gpio = GP_GS2971_TIM_861,	.flags = 0},
	{.label = "gs-sw_en",		.gpio = GP_GS2971_SW_EN,	.flags = 0},
	{.label = "gs-standby",		.gpio = GP_GS2971_STANDBY,	.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "gs-dvb_asi",		.gpio = GP_GS2971_DVB_ASI,	.flags = 0},
	{.label = "adv7391_reset",	.gpio = GP_ADV7391_RESET,	.flags = 0},
};

static struct gpio export_gpios[] __initdata = {
	{.label = "pwr-j1",		.gpio = GP_PWR_J1,		.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "pwr-j2",		.gpio = GP_PWR_J2,		.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "pwr-j3",		.gpio = GP_PWR_J3,		.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "pwr-j4",		.gpio = GP_PWR_J4,		.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "pwr-j6",		.gpio = GP_PWR_J6,		.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "pwr-j7",		.gpio = GP_PWR_J7,		.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "dry_contact",	.gpio = GP_DRY_CONTACT,		.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "bt_gpio1",		.gpio = GP_BT_GPIO1,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio2",		.gpio = GP_BT_GPIO2,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio3",		.gpio = GP_BT_GPIO3,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio4",		.gpio = GP_BT_GPIO4,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio5",		.gpio = GP_BT_GPIO5,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio6",		.gpio = GP_BT_GPIO6,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio7",		.gpio = GP_BT_GPIO7,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio8",		.gpio = GP_BT_GPIO8,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio9",		.gpio = GP_BT_GPIO9,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio10",		.gpio = GP_BT_GPIO10,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio11",		.gpio = GP_BT_GPIO11,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio12",		.gpio = GP_BT_GPIO12,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio13",		.gpio = GP_BT_GPIO13,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio14",		.gpio = GP_BT_GPIO14,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio15",		.gpio = GP_BT_GPIO15,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio16",		.gpio = GP_BT_GPIO16,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio17",		.gpio = GP_BT_GPIO17,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio18",		.gpio = GP_BT_GPIO18,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio19",		.gpio = GP_BT_GPIO19,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio20",		.gpio = GP_BT_GPIO20,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio21",		.gpio = GP_BT_GPIO21,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio22",		.gpio = GP_BT_GPIO22,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio23",		.gpio = GP_BT_GPIO23,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio24",		.gpio = GP_BT_GPIO24,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio25",		.gpio = GP_BT_GPIO25,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio26",		.gpio = GP_BT_GPIO26,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio27",		.gpio = GP_BT_GPIO27,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio28",		.gpio = GP_BT_GPIO28,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio29",		.gpio = GP_BT_GPIO29,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio30",		.gpio = GP_BT_GPIO30,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio31",		.gpio = GP_BT_GPIO31,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio32",		.gpio = GP_BT_GPIO32,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio33",		.gpio = GP_BT_GPIO33,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio34",		.gpio = GP_BT_GPIO34,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio35",		.gpio = GP_BT_GPIO35,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio36",		.gpio = GP_BT_GPIO36,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio37",		.gpio = GP_BT_GPIO37,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio38",		.gpio = GP_BT_GPIO38,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio39",		.gpio = GP_BT_GPIO39,		.flags = GPIOF_DIR_IN},
	{.label = "bt_gpio40",		.gpio = GP_BT_GPIO40,		.flags = GPIOF_DIR_IN},
};

static void poweroff(void)
{
	int i;
	for (i=0; i < num_registered_fb; i++) {
		if (registered_fb[i])
                        fb_blank(registered_fb[i],FB_BLANK_POWERDOWN);
	}

	for (i = 0; i < 5; i++) {
		msleep(1000);
	}
	arch_reset('h',"");
}

/*!
 * Board specific initialization.
 */
static void __init board_init(void)
{
	int i, j;
	int ret;
	unsigned mask;

	ret = gpio_request_array(initial_gpios, ARRAY_SIZE(initial_gpios));
	if (ret)
		printk(KERN_ERR "%s gpio_request_array failed("
			"%d) for initial_gpios\n", __func__, ret);
	ret = gpio_request_array(export_gpios, ARRAY_SIZE(export_gpios));
	if (ret) {
		pr_err("%s gpio_request_array failed(%d) for export_gpios\n",
				__func__, ret);
	}
	IOMUX_SETUP(board_pads);
	adv7391_disable_pins();

	printk(KERN_ERR "------------ Board type BT\n");
	for (i=0; i < ARRAY_SIZE(export_gpios);i++) {
		int gpio = export_gpios[i].gpio;

		pr_info("%s: exporting gpio %d\n", __func__, gpio);
		gpio_export(gpio,1);
	}

	gp_reg_id = dvfscore_data.reg_id;
	soc_reg_id = dvfscore_data.soc_id;
	pu_reg_id = dvfscore_data.pu_id;

	imx6q_add_imx_uart(0, NULL);
	imx6q_add_imx_uart(1, NULL);
	imx6q_add_imx_uart(2, NULL);
	imx6q_add_imx_uart(3, NULL);
	imx6q_add_imx_uart(4, NULL);

	imx6q_add_mxc_hdmi_core(&hdmi_core_data);

	imx6q_add_ipuv3(0, &ipu_data[0]);
	if (cpu_is_mx6q())
		imx6q_add_ipuv3(1, &ipu_data[1]);

	for (i = 0; i < ARRAY_SIZE(fb_data); i++)
		imx6q_add_ipuv3fb(i, &fb_data[i]);

	imx6q_add_vdoa();
	imx6q_add_v4l2_output(0);

	mask = 0;
	for (i = 0; i < ARRAY_SIZE(capture_data); i++) {
		if (!cpu_is_mx6q())
			capture_data[i].ipu = 0;
		j = (capture_data[i].ipu << 1) | capture_data[i].csi;
		if (!(mask & (1 << j))) {
			mask |= (1 << j);
			imx6q_add_v4l2_capture(i, &capture_data[i]);
			pr_info("%s: added capture for ipu%d:csi%d\n", __func__, capture_data[i].ipu, capture_data[i].csi);
		}
	}
	if (!cpu_is_mx6q())
		gs2971_data.ipu = 0;


	imx6q_add_imx_snvs_rtc();

	imx6q_add_imx_i2c(0, &i2c_data);
	imx6q_add_imx_i2c(1, &i2c_data);
	imx6q_add_imx_i2c(2, &i2c_data);
	i2c_register_board_info(0, i2c0_board_info,
		ARRAY_SIZE(i2c0_board_info));
	i2c_register_board_info(1, i2c1_board_info,
			ARRAY_SIZE(i2c1_board_info));
	i2c_register_board_info(2, i2c2_board_info,
			ARRAY_SIZE(i2c2_board_info));

	/* SPI */
	imx6q_add_ecspi(0, &ecspi1_data);
	imx6q_add_ecspi(2, &ecspi3_data);
	spi_device_init();

	imx6q_add_mxc_hdmi(&hdmi_data);

	imx6q_add_anatop_thermal_imx(1, &anatop_thermal_data);
	imx6_init_fec(fec_data);
	imx6q_add_pm_imx(0, &pm_data);
	imx6q_add_sdhci_usdhc_imx(0, &sd1_data);
	imx6q_add_sdhci_usdhc_imx(1, &sd2_data);
	imx6q_add_sdhci_usdhc_imx(2, &sd3_data);
	imx_add_viv_gpu(&imx6_gpu_data, &gpu_pdata);
	init_usb();
	imx6q_add_vpu();
	init_audio();
	platform_device_register(&vmmc_reg_devices);
	imx_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	imx_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	imx6q_add_asrc(&imx_asrc_data);

	/* release USB Hub reset */
	gpio_set_value(GP_USB_HUB_RESET, 1);
	gpio_set_value(GP_AX88772A_RESET, 1);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&dvfscore_data);

	imx6q_add_hdmi_soc();
	imx6q_add_hdmi_soc_dai();

	pm_power_off = poweroff;
	imx6q_add_busfreq();

	gpio_set_value(GP_EMMC_RESET, 1);

	imx6q_add_pcie(&pcie_data);

	imx6_add_armpmu();
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

static struct sys_timer timer __initdata = {
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
 * initialize __mach_desc_ data structure.
 */
MACHINE_START(MX6_BT, "Boundary Devices BT Board")
	/* Maintainer: Boundary Devices */
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = board_init,
	.timer = &timer,
	.reserve = reserve,
MACHINE_END
