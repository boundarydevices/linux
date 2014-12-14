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
#include <linux/i2c/mpr121_touchkey.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/regulator/consumer.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/fec.h>
#include <linux/memblock.h>
#include <linux/micrel_phy.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/ion.h>
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
#include <mach/ipu-v3.h>
#include <mach/system.h>
#include <linux/wl12xx.h>
#include <linux/ti_wilink_st.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "usb.h"
#include "devices-imx6q.h"
#include "crm_regs.h"
#include "cpu_op-mx6.h"

#include "pads-mx6_eo.h"
#define FOR_DL_SOLO
#include "pads-mx6_eo.h"
#include "wl12xx-bt.h"

void __init early_console_setup(unsigned long base, struct clk *clk);

extern char *gp_reg_id;
extern char *soc_reg_id;
extern char *pu_reg_id;
static int caam_enabled;

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
	gpio_set_value(GP_WL1271_WL_EN, on);
}

static struct esdhc_platform_data sd2_data = {
	.always_present = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.keep_power_at_suspend = 0,
	.caps = MMC_CAP_POWER_OFF_CARD,
	.platform_pad_change = plt_sd_pad_change,
	.set_power = sdio_set_power,
};

static struct esdhc_platform_data sd3_data = {
	.cd_gpio = GP_SD3_CD,
	.wp_gpio = -1,
//	.support_18v = 1;
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static struct esdhc_platform_data sd4_data = {
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

static struct ti_st_plat_data wilink_pdata = {
	.nshutdown_gpio = GP_WL1271_BT_EN,
	.dev_name = "/dev/ttymxc2",
	.flow_cntrl = 1,
	.baud_rate = 3000000,
	.suspend = plat_kim_suspend,
	.resume = plat_kim_resume,
	.chip_enable = plat_kim_chip_enable,
	.chip_disable = plat_kim_chip_disable
};

static struct platform_device wl127x_bt_device = {
	.name		= "kim",
	.id		= -1,
	.dev.platform_data = &wilink_pdata,
};

static struct platform_device btwilink_device = {
	.name = "btwilink",
	.id = -1,
};

static int fec_phy_init(struct phy_device *phydev)
{
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

static int spi_cs[] = {
	GP_ECSPI1_CS1,
};

static const struct spi_imx_master spi_data __initconst = {
	.chipselect     = spi_cs,
	.num_chipselect = ARRAY_SIZE(spi_cs),
};

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

static struct spi_board_info spi_nor_device[] __initdata = {
	{
		.modalias = "m25p80",
		.max_speed_hz = 20000000, /* max spi clock (SCK) speed in HZ */
		.bus_num = 0,
		.chip_select = 0,
		.platform_data = &spi_flash_data,
	},
};

static void spi_device_init(void)
{
	spi_register_board_info(spi_nor_device,
				ARRAY_SIZE(spi_nor_device));
}

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
		I2C_BOARD_INFO("apds9300", 0x29),
	},
};

static unsigned short touchkeys[] = {
	KEY_F1,		/* EL0: J57 pin 2	*/
	KEY_F2,		/* EL1: J57 pin 1	*/
	KEY_F3,		/* EL2: N/C 		*/
	KEY_F4,		/* EL3: J57 pin 4	*/
	KEY_F5,		/* EL4: N/C 		*/
	KEY_F6,		/* EL5: N/C 		*/
	KEY_F7,		/* EL6: N/C 		*/
	KEY_F8,		/* EL7: J57 pin 5	*/
	KEY_F9,		/* EL8: N/C 		*/
	KEY_F10,	/* EL9: N/C 		*/
	KEY_F11,	/* EL10: N/C 		*/
	KEY_F12,	/* EL11: J57 pin 3 	*/
};

static struct mpr121_platform_data mpr121_platdata = {
	.keymap_size = ARRAY_SIZE(touchkeys),
	.vdd_uv = 3300000,
	.keymap = touchkeys,
};

static int mma8x5x_position = 0;
static struct i2c_board_info i2c2_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("mma8x5x", 0x1d),
		.platform_data = (void *)&mma8x5x_position,
	},
	{
		I2C_BOARD_INFO("mpr121_touchkey", 0x5a),
		.irq = gpio_to_irq(GP_CAP_TOUCH_IRQ),
		.platform_data = &mpr121_platdata,
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

static struct viv_gpu_platform_data imx6_gpu_pdata __initdata = {
	.reserved_mem_size = SZ_128M + SZ_64M - SZ_16M,
};

static struct ipuv3_fb_platform_data fb_data = {
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "1920x1080MR@60,if=RGB24",
	.default_bpp = 32,
	.int_clk = false,
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

static struct ion_platform_data imx_ion_data = {
	.nr = 1,
	.heaps = {
		{
		.type = ION_HEAP_TYPE_CARVEOUT,
		.name = "vpu_ion",
		.size = SZ_64M,
		},
	},
};

struct imx_vout_mem {
       resource_size_t res_mbase;
       resource_size_t res_msize;
};

static struct imx_vout_mem vout_mem __initdata = {
       .res_msize = 0,
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

static void wl1271_set_power(bool enable)
{
	if (0 == enable) {
		gpio_set_value(GP_WL1271_WL_EN, 0);		/* momentarily disable */
		mdelay(2);
		gpio_set_value(GP_WL1271_WL_EN, 1);
	}
}

static struct wl12xx_platform_data wl1271_data __initdata = {
	.irq = gpio_to_irq(GP_WL1271_WL_IRQ),
	.board_ref_clock = WL12XX_REFCLOCK_38, /* 38.4 MHz */
	.set_power = wl1271_set_power,
};

static struct regulator_consumer_supply vwl1271_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.1"),
};

static struct regulator_init_data vwl1271_init = {
	.constraints            = {
		.name           = "VDD_1.8V",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(vwl1271_consumers),
	.consumer_supplies = vwl1271_consumers,
};

static struct fixed_voltage_config vwl1271_reg_config = {
	.supply_name		= "vwl1271",
	.microvolts		= 1800000, /* 1.80V */
	.gpio			= GP_WL1271_WL_EN,
	.startup_delay		= 70000, /* 70ms */
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &vwl1271_init,
};

static struct platform_device vwl1271_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 4,
	.dev	= {
		.platform_data = &vwl1271_reg_config,
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

/* PWM1_PWMO: backlight control on connector J58 */
static struct platform_pwm_backlight_data pwm1_backlight_data = {
	.pwm_id = 0,	/* pin SD1_DATA3 - PWM1 */
	.max_brightness = 256,
	.dft_brightness = 256,
	.pwm_period_ns = 1000000000/32768,
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

static void __init fixup_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	char *str;
	struct tag *t;

	for_each_tag(t, tags) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			str = t->u.cmdline.cmdline;
			str = strstr(str, "fbmem=");
			if (str != NULL) {
				str += 6;
				fb_data.res_size[0] = memparse(str, &str);
			}
			/* GPU reserved memory */
			str = t->u.cmdline.cmdline;
			str = strstr(str, "gpumem=");
			if (str != NULL) {
				str += 7;
				imx6_gpu_pdata.reserved_mem_size = memparse(str, &str);
			}
			/* VPU reserved memory */
			str = t->u.cmdline.cmdline;
			str = strstr(str, "vpumem=");
			if (str != NULL) {
				str += 7;
				vout_mem.res_msize = memparse(str, &str);
			}
			break;
		}
	}
}

static int __init caam_setup(char *__unused)
{
	caam_enabled = 1;
	return 1;
}
early_param("caam", caam_setup);

#define GPIOF_HIGH GPIOF_OUT_INIT_HIGH
static struct gpio initial_gpios[] __initdata = {
	{.label = "emmc_reset",		.gpio = GP_EMMC_RESET,		.flags = 0},
	{.label = "wl1271_int",		.gpio = GP_WL1271_WL_IRQ,	.flags = GPIOF_DIR_IN},
	{.label = "wl1271_bt_en",	.gpio = GP_WL1271_BT_EN,	.flags = 0},
	{.label = "wl1271_wl_en",	.gpio = GP_WL1271_WL_EN,	.flags = 0},
	{.label = "usb-pwr",		.gpio = GP_USB_OTG_PWR,		.flags = 0},
};

static int bt_power_change(int status)
{
	gpio_set_value(GP_WL1271_BT_EN, status ? 1 : 0);
	return 0;
}

static struct platform_device mxc_bt_rfkill = {
	.name = "mxc_bt_rfkill",
};

static struct imx_bt_rfkill_platform_data mxc_bt_rfkill_data = {
	.power_change = bt_power_change,
};

static void poweroff(void)
{
	int waspressed = 0;
	int i;
	for (i=0; i < num_registered_fb; i++) {
		if (registered_fb[i])
                        fb_blank(registered_fb[i],FB_BLANK_POWERDOWN);
	}

	while (1) {
		int pressed=(0 == gpio_get_value(GP_BUTTON1));
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
}

/* UART3 - ttymxc2 */
static const struct imxuart_platform_data ttymxc2_data __initconst = {
	.flags      = IMXUART_HAVE_RTSCTS,
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
	GPIO_BUTTON(GP_BUTTON1, KEY_POWER, 1, "key-power", 1),
	GPIO_BUTTON(GP_BUTTON2, KEY_MENU, 1, "key-memu", 0),
	GPIO_BUTTON(GP_BUTTON3, KEY_HOME, 1, "key-home", 0),
	GPIO_BUTTON(GP_BUTTON4, KEY_BACK, 1, "key-back", 0),
	GPIO_BUTTON(GP_BUTTON5, KEY_SEARCH, 1, "key-search", 0),
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

/*!
 * Board specific initialization.
 */
static void __init board_init(void)
{
	int ret;
	struct clk *clko2;
	struct clk *new_parent;
	struct platform_device *voutdev;
	int rate;

	ret = gpio_request_array(initial_gpios, ARRAY_SIZE(initial_gpios));
	if (ret)
		printk(KERN_ERR "%s gpio_request_array failed("
			"%d) for initial_gpios\n", __func__, ret);
	IOMUX_SETUP(board_pads);
	printk(KERN_ERR "------------ Board type EO\n");

	gp_reg_id = dvfscore_data.reg_id;
	soc_reg_id = dvfscore_data.soc_id;
	pu_reg_id = dvfscore_data.pu_id;

	imx6q_add_imx_uart(0, NULL);
	imx6q_add_imx_uart(1, NULL);
	imx6q_add_imx_uart(2, &ttymxc2_data);

	imx6q_add_ipuv3(0, &ipu_data[0]);
        imx6q_add_ipuv3(1, &ipu_data[1]);
	imx6q_add_ipuv3fb(0, &fb_data);
	imx6q_add_ipuv3fb(1, &fb_data);

	imx6q_add_vdoa();
	imx6q_add_ldb(&ldb_data);
	voutdev = imx6q_add_v4l2_output(0);
	if (vout_mem.res_msize && voutdev) {
		dma_declare_coherent_memory(&voutdev->dev,
                                            vout_mem.res_mbase,
                                            vout_mem.res_mbase,
                                            vout_mem.res_msize,
                                            (DMA_MEMORY_MAP |
                                             DMA_MEMORY_EXCLUSIVE));
	}
	imx6q_add_imx_caam();

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
	imx6q_add_ecspi(0, &spi_data);
	spi_device_init();

	imx6q_add_anatop_thermal_imx(1, &anatop_thermal_data);
	imx6_init_fec(fec_data);
	imx6q_add_pm_imx(0, &pm_data);
	imx6q_add_sdhci_usdhc_imx(2, &sd3_data);
	imx6q_add_sdhci_usdhc_imx(3, &sd4_data);
	imx_add_viv_gpu(&imx6_gpu_data, &imx6_gpu_pdata);
	init_usb();
	imx6q_add_vpu();
	platform_device_register(&vmmc_reg_devices);

	/* release USB Hub reset */
	gpio_set_value(GP_USB_HUB_RESET, 1);

	imx6q_add_mxc_pwm(0);
	imx6q_add_mxc_pwm_backlight(0, &pwm1_backlight_data);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&dvfscore_data);
	imx6q_add_ion(0, &imx_ion_data,
		sizeof(imx_ion_data) + sizeof(struct ion_platform_heap));

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

	mxc_register_device(&mxc_bt_rfkill, &mxc_bt_rfkill_data);
	gpio_set_value(GP_EMMC_RESET, 1);

	imx6q_add_sdhci_usdhc_imx(1, &sd2_data);
	/* WL12xx WLAN Init */
	if (wl12xx_set_platform_data(&wl1271_data))
		pr_err("error setting wl12xx data\n");
	platform_device_register(&vwl1271_reg_devices);

	gpio_set_value(GP_WL1271_WL_EN, 1);		/* momentarily enable */
	gpio_set_value(GP_WL1271_BT_EN, 1);
	mdelay(2);
	gpio_set_value(GP_WL1271_WL_EN, 0);
	gpio_set_value(GP_WL1271_BT_EN, 0);

	gpio_free(GP_WL1271_WL_EN);
	gpio_free(GP_WL1271_BT_EN);
	mdelay(1);
	gpio_export(GP_WL1271_BT_EN,1);

	platform_device_register (&wl127x_bt_device);
	platform_device_register (&btwilink_device);

	imx6_add_armpmu();
	imx6q_add_perfmon(0);
	imx6q_add_perfmon(1);
	imx6q_add_perfmon(2);
	add_device_buttons();
}

extern void __iomem *twd_base;
static void __init timer_init(void)
{
	struct clk *uart_clk;
	twd_base = ioremap(LOCAL_TWD_ADDR, SZ_256);
	BUG_ON(!twd_base);
	mx6_clocks_init(32768, 24000000, 0, 0);

	uart_clk = clk_get_sys("imx-uart.0", NULL);
	early_console_setup(UART2_BASE_ADDR, uart_clk);
}

static struct sys_timer timer __initdata = {
	.init   = timer_init,
};

static void __init reserve(void)
{
	phys_addr_t phys;
	resource_size_t sz;

	pr_err("%s: fbmem=%u\n", __func__, fb_data.res_size[0]);
	pr_err("%s: gpumem=%u\n", __func__, imx6_gpu_pdata.reserved_mem_size);
	pr_err("%s: vpumem=%u\n", __func__, vout_mem.res_msize);
	if (imx6_gpu_pdata.reserved_mem_size) {
		pr_info("%s: gpu reserve %x\n", __func__, imx6_gpu_pdata.reserved_mem_size);
		phys = memblock_alloc_base(imx6_gpu_pdata.reserved_mem_size,
					   SZ_4K, SZ_2G);
		memblock_remove(phys, imx6_gpu_pdata.reserved_mem_size);
		imx6_gpu_pdata.reserved_mem_base = phys;
	}

	if (imx_ion_data.heaps[0].size) {
		pr_info("%s: ion reserve %x\n", __func__, imx_ion_data.heaps[0].size);
		phys = memblock_alloc(imx_ion_data.heaps[0].size, SZ_4K);
		memblock_remove(phys, imx_ion_data.heaps[0].size);
		imx_ion_data.heaps[0].base = phys;
	}

	sz = fb_data.res_size[0];
	if (sz){
		pr_info("%s: fb0 reserve %x\n", __func__, sz);
		/* reserve for background buffer */
		phys = memblock_alloc(sz, SZ_4K);
		memblock_remove(phys, sz);
		fb_data.res_base[0] = phys;
	}
	if (vout_mem.res_msize) {
		pr_info("%s: vout reserve %x\n", __func__, vout_mem.res_msize);
		phys = memblock_alloc_base(vout_mem.res_msize,
					   SZ_4K, SZ_2G);
		memblock_remove(phys, vout_mem.res_msize);
		vout_mem.res_mbase = phys;
	}
}

/*
 * initialize __mach_desc_ data structure.
 */
MACHINE_START(MX6_EO, "Boundary Devices EO Board")
	/* Maintainer: Boundary Devices */
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = board_init,
	.timer = &timer,
	.reserve = reserve,
MACHINE_END
