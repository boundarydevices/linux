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

#define GP_USB_OTG_PWR	IMX_GPIO_NR(3, 22)

#define GP_ENET_PHY_RESET	IMX_GPIO_NR(1, 27)	/* ENET_RXD0 - active low */
#define GP_ENET_PHY_IRQ		IMX_GPIO_NR(1, 28)	/* ENET_TX_EN - active low */

#define GP_EMMC_RESET		IMX_GPIO_NR(2, 0)	/* NANDF_D0 - active low */
#define GP_EMMC_RESET_REV0	IMX_GPIO_NR(2, 24)	/* EIM_CS1 - active low */
#define GP_ECSPI1_CS1		IMX_GPIO_NR(3, 19)	/* EIM_D19 - active low */
#define GP_MODEM_ONOFF		IMX_GPIO_NR(2, 5)
#define GP_MODEM_RESET		IMX_GPIO_NR(2, 6)

#include "pads-mx6_a.h"
#define FOR_DL_SOLO
#include "pads-mx6_a.h"

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
	{.label = "phy_reset",		.gpio = GP_ENET_PHY_RESET,	.flags = GPIOF_HIGH},	/* GPIO1[27]: ENET_RXD0 - active low */
//	{.label = "phy_irq",		.gpio = GP_ENET_PHY_IRQ,	.flags = GPIOF_DIR_IN},	/* GPIO1[28]: ENET_TX_EN - active low */
	{.label = "eMMC_reset",		.gpio = GP_EMMC_RESET,		.flags = 0},
	{.label = "eMMC_reset_rev0",	.gpio = GP_EMMC_RESET_REV0,	.flags = 0},
	{.label = "Modem_reset",	.gpio = GP_MODEM_RESET,		.flags = GPIOF_DIR_IN},
	{.label = "Modem_onoff",	.gpio = GP_MODEM_ONOFF,		.flags = GPIOF_DIR_IN},
	{.label = "usb-pwr",		.gpio = GP_USB_OTG_PWR,		.flags = 0},
//	{.label = "factory_default",	.gpio = IMX_GPIO_NR(4, 6),	.flags = GPIOF_DIR_IN},

	{.label = "led1",		.gpio = IMX_GPIO_NR(2, 22),	.flags = 0},	/* high - on */
	{.label = "led2",		.gpio = IMX_GPIO_NR(2, 21),	.flags = 0},	/* high - on */
	{.label = "led3",		.gpio = IMX_GPIO_NR(2, 20),	.flags = 0},	/* high - on */
	{.label = "led4",		.gpio = IMX_GPIO_NR(2, 19),	.flags = 0},	/* high - on */
	{.label = "led5",		.gpio = IMX_GPIO_NR(6, 6),	.flags = 0},	/* high - on */
	{.label = "rxact",		.gpio = IMX_GPIO_NR(1, 3),	.flags = GPIOF_HIGH},	/* low - on */
	{.label = "txact",		.gpio = IMX_GPIO_NR(1, 4),	.flags = GPIOF_HIGH},	/* low - on */
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
	int i = (index - 3) * SD_SPEED_CNT;

//	pr_info("%s: index %d, clock %d\n", __func__, index, clock);
	if (index != 3) {
		pr_err("%s:no such SD host controller index %d\n", __func__, index);
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
	gpio_set_value(GP_EMMC_RESET, on);
	if (on)
		gpio_direction_input(GP_EMMC_RESET_REV0);
	else
		gpio_direction_output(GP_EMMC_RESET_REV0, on);

//	pr_info("%s:%s: %d\n", __FILE__, __func__, on);
}

static const struct esdhc_platform_data mx6_sd4_data __initconst = {
	.cd_gpio = -1,
	.wp_gpio = -1,
	.always_present = 1,
	.keep_power_at_suspend = 1,
	.support_8bit = 1,
	.platform_pad_change = plt_sd_pad_change,
	.set_power = sdio_set_power,
};


static const struct anatop_thermal_platform_data
	mx6_anatop_thermal_data __initconst = {
		.name = "anatop_thermal",
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
	.phy_irq = gpio_to_irq(GP_ENET_PHY_IRQ)
};

static int mx6_spi_cs[] = {
	GP_ECSPI1_CS1,
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

/*
 **********************************************************************
 */
static int vbus_on;
static int vbus_state;
static struct delayed_work usb_modem_power_work;

static void usb_modem_power_handler(struct work_struct *work)
{
	switch (vbus_state) {
	case 0:
		gpio_direction_output(GP_MODEM_ONOFF, 0);	/* Power up modem pulse low */
		/*
		 * on  - pulse low .5 to 1 second,
		 * off - pulse low for 3 seconds
		 */
		schedule_delayed_work(&usb_modem_power_work,
			usecs_to_jiffies(vbus_on ? 750000 : 3000000));
		vbus_state++;
		break;
	case 1:
		gpio_direction_input(GP_MODEM_ONOFF);		/* ON/OFF high */
		vbus_state++;
		pr_info("%s: %d\n", __func__, vbus_on);
		break;
	}
}

static void imx6_usbotg_vbus(bool on)
{
	vbus_on = on;
	vbus_state = 0;
	gpio_set_value(GP_USB_OTG_PWR, on ? 1 : 0);

	gpio_direction_input(GP_MODEM_ONOFF);		/* ON/OFF high */
	schedule_delayed_work(&usb_modem_power_work,
				usecs_to_jiffies(3000000));
}

static void __init imx6_init_usb(void)
{
	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);
	/* disable external charger detect,
	 * or it will affect signal quality at dp .
	 */
	mxc_iomux_set_gpr_register(1, 13, 1, 1);
	INIT_DELAYED_WORK(&usb_modem_power_work, usb_modem_power_handler);
	mx6_set_otghost_vbus_func(imx6_usbotg_vbus);

	gpio_direction_output(GP_MODEM_RESET, 0);	/* modem reset low */
	mdelay(50);
	gpio_direction_input(GP_MODEM_RESET);		/* modem reset high */
}

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

static struct regulator_consumer_supply mx6_vmmc_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.3"),
};

static struct regulator_init_data mx6_vmmc_init = {
	.num_consumer_supplies = ARRAY_SIZE(mx6_vmmc_consumers),
	.consumer_supplies = mx6_vmmc_consumers,
};

static struct fixed_voltage_config mx6_vmmc_reg_config = {
	.supply_name		= "vmmc",
	.microvolts		= 3300000,
	.gpio			= -1,
	.init_data		= &mx6_vmmc_init,
};

static struct platform_device mx6_vmmc_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 0,
	.dev	= {
		.platform_data = &mx6_vmmc_reg_config,
	},
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
	GPIO_BUTTON(IMX_GPIO_NR(4, 6), KEY_HOME, 1, "Factory default", 0),
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
	imx6q_add_imx_uart(2, NULL);
	imx6q_add_imx_uart(3, NULL);

	imx6q_add_imx_snvs_rtc();

	/* SPI */
	imx6q_add_ecspi(0, &mx6_spi_data);
	spi_device_init();

	imx6q_add_anatop_thermal_imx(1, &mx6_anatop_thermal_data);
	imx6_init_fec(fec_data);
	imx6q_add_pm_imx(0, &mx6_pm_data);
	imx6q_add_sdhci_usdhc_imx(3, &mx6_sd4_data);
	imx6_init_usb();
	platform_device_register(&mx6_vmmc_reg_devices);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&oc_dvfscore_data);

	add_device_buttons();

	imx6q_add_busfreq();

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

/*
 * initialize __mach_desc_MX6Q_OC data structure.
 */
MACHINE_START(MX6_A, "Boundary Devices A Board")
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = mx6_board_init,
	.timer = &mx6_timer,
MACHINE_END
