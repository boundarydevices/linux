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
#include <linux/gpio.h>
#include <linux/ion.h>
#include <linux/i2c/tsc2007.h>
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
#include <mach/mxc_asrc.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "devices-imx6q.h"
#include "crm_regs.h"
#include "cpu_op-mx6.h"
#include "usb.h"

#define ECSPI1_CS1	IMX_GPIO_NR(3, 19)
#define WEAK_PULLUP	(PAD_CTL_HYS | PAD_CTL_PKE \
			 | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)

#define N6_IRQ_PADCFG		(PAD_CTL_PUE | PAD_CTL_PUS_100K_DOWN | PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_HYS)
#define N6_IRQ_TEST_PADCFG	(PAD_CTL_PKE | N6_IRQ_PADCFG)
#define N6_EN_PADCFG		(PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm)
#define SD4_CD_GPIO		IMX_GPIO_NR(2,6)
#define SD3_CD_GPIO		IMX_GPIO_NR(7,0)
#define SD2_CD_GPIO		-1
#define TOUCH_IRQ		IMX_GPIO_NR(4, 20)	/* DI0_PIN4, active low */
#define RTC_IRQ			IMX_GPIO_NR(6, 7)	/* NANDF_CLE */
#define DISPLAY_SHUTDOWN	IMX_GPIO_NR(2, 4)	/* NANDF_D4 */
#define POWERDOWN_REPLY		IMX_GPIO_NR(1, 2)
#define POWERDOWN_NOTICE	IMX_GPIO_NR(1, 6)
#define POWERLOW		IMX_GPIO_NR(1, 3)
#define BUZZER			IMX_GPIO_NR(7, 13)
#define IR_STATUS		IMX_GPIO_NR(1, 9)
#define MIPI_PWN		IMX_GPIO_NR(6, 9)
#define MIPI_RST		IMX_GPIO_NR(2, 5)


#include "pads-mx6_cad.h"
#define FOR_DL_SOLO
#include "pads-mx6_cad.h"

extern char *gp_reg_id;
extern char *soc_reg_id;
extern char *pu_reg_id;

extern struct regulator *(*get_cpu_regulator)(void);
extern void (*put_cpu_regulator)(void);
extern void mx6_cpu_regulator_init(void);

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

/* SD cards */
static struct esdhc_platform_data sd4_data = {
	.cd_gpio = SD4_CD_GPIO,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static struct esdhc_platform_data sd3_data = {
	.cd_gpio = SD3_CD_GPIO,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static struct esdhc_platform_data sd2_data = {
	.cd_gpio = SD2_CD_GPIO,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct anatop_thermal_platform_data
	anatop_thermal_data __initconst = {
		.name = "anatop_thermal",
};

static const struct imxuart_platform_data uart3_data __initconst = {
	.flags      = IMXUART_HAVE_RTSCTS,
	.dma_req_rx = MX6Q_DMA_REQ_UART3_RX,
	.dma_req_tx = MX6Q_DMA_REQ_UART3_TX,
};

static int spi_cs[] = {
	ECSPI1_CS1,
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

static struct mxc_audio_platform_data audio_data;

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

	audio_data.sysclk = rate;
	clk_set_rate(clko, rate);
	clk_enable(clko);
	return 0;
}

static struct imx_ssi_platform_data ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

static struct mxc_audio_platform_data audio_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 4,
	.init = sgtl5000_init,
	.hp_gpio = -1,
};

static struct platform_device audio_device = {
	.name = "imx-sgtl5000",
};

static struct imxi2c_platform_data i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("sgtl5000", 0x0a),
	},
	{
		I2C_BOARD_INFO("isl1208", 0x6f),	/* Real time clock */
		.irq = gpio_to_irq(RTC_IRQ),		/* NANDF_CLE */
	},
};

static struct mxc_pwm_platform_data pwm3_data = {
	.clk_select = PWM_CLK_HIGHPERF,
};

static struct mipi_csi2_platform_data mipi_csi2_pdata = {
	.dphy_clk = "mipi_pllref_clk",
	.pixel_clk = "emi_clk",
	.cfg_clk = "hdmi_isfr_clk",
};

static struct pwm_device *mipi_pwm;
static struct fsl_mxc_camera_platform_data ov5640_mipi_data;
static int ov5640_mipi_reset_active;

static void camera_reset(int power_gp, int poweroff_level, int reset_gp)
{
	pr_info("%s: power_gp=0x%x, reset_gp=0x%x\n",
			__func__, power_gp, reset_gp);
	gpio_direction_output(power_gp, poweroff_level);
	/* Camera reset */
	gpio_direction_output(reset_gp, 0);
	msleep(1);
}

static void ov5640_mipi_camera_io_init(void)
{
	IOMUX_SETUP(mipi_pads);

	pr_info("%s\n", __func__);
	if (!mipi_pwm || IS_ERR(mipi_pwm))
		mipi_pwm = pwm_request(2, "mipi_clock");
	if (IS_ERR(mipi_pwm)) {
		pr_err("unable to request PWM for mipi_clock\n");
	} else {
		unsigned period = 1000/22;
		pr_info("got pwm for mipi_clock\n");
		pwm_config(mipi_pwm, period >> 1, period);
		pwm_enable(mipi_pwm);
		pr_info("pwm enabled\n");
	}

	camera_reset(MIPI_PWN, 1, MIPI_RST);
	ov5640_mipi_reset_active = 1;
	if (cpu_is_mx6dl()) {
		/*
		 * for mx6dl, mipi virtual channel 0 connect to csi0
		 * virtual channel 1 connect to csi1
		 */
		mxc_iomux_set_gpr_register(13, ov5640_mipi_data.csi * 3, 3,
					   ov5640_mipi_data.csi);
	} else {
		if (ov5640_mipi_data.csi == ov5640_mipi_data.ipu) {
			/* select mipi IPU1 CSI0/ IPU2/CSI1 */
			mxc_iomux_set_gpr_register
				(1, 19 + ov5640_mipi_data.csi, 1, 0);
		}
	}
}

static void ov5640_mipi_camera_powerdown(int powerdown)
{
	pr_info("%s: powerdown=%d, power_gp=0x%x\n",
			__func__, powerdown, MIPI_PWN);
	if (mipi_pwm && !IS_ERR(mipi_pwm)) {
		if (powerdown) {
			pwm_disable(mipi_pwm);
		} else {
			unsigned period = 1000/22;
			pwm_config(mipi_pwm, period >> 1, period);
			pwm_enable(mipi_pwm);
		}
	} else
		pr_err("%s: no MIPI PWM\n", __func__);
	gpio_set_value(MIPI_PWN, powerdown ? 1 : 0);
	if (!powerdown) {
		msleep(2);
		if (ov5640_mipi_reset_active) {
			ov5640_mipi_reset_active = 0;
			gpio_set_value(MIPI_RST, 1);
			msleep(20);
		}
	}
}

static struct fsl_mxc_camera_platform_data ov5640_mipi_data = {
	.mclk = 22000000,
	.ipu = 0,
	.csi = 0,
	.io_init = ov5640_mipi_camera_io_init,
	.pwdn = ov5640_mipi_camera_powerdown,
	.lock = 0,
	.unlock = 0,
};

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("ov5640_mipi", 0x3e),
		.platform_data = (void *)&ov5640_mipi_data,
	},
};

static struct tsc2007_platform_data tsc2007_info = {
	.model			= 2004,
	.x_plate_ohms		= 500,
};

/*
 * TODO: struct ad799x_platform_data needs to go into include/linux/iio
 */

struct ad799x_platform_data {
	u16				vref_mv;
};

static struct ad799x_platform_data ad799x_pdata = {
	.vref_mv = 3000,
};

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("tsc2004", 0x48),
		.platform_data	= &tsc2007_info,
		.irq = gpio_to_irq(TOUCH_IRQ),
	},
	{
		I2C_BOARD_INFO("ad7997", 0x21),
		.platform_data = &ad799x_pdata,
		.irq = -1,
	},
};

static void usbotg_vbus(bool on)
{
	pr_err("%s: %d\n", __func__, on);
}

static void __init init_usb(void)
{
	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);
	mxc_iomux_set_gpr_register(1, 13, 1, 1);

	mx6_set_otghost_vbus_func(usbotg_vbus);
}

static struct viv_gpu_platform_data imx6_gpu_pdata __initdata = {
	.reserved_mem_size = SZ_128M,
};

static struct imx_asrc_platform_data imx_asrc_data = {
	.channel_bits = 4,
	.clk_map_ver = 2,
};

static void lcd_enable_pins(void)
{
	pr_debug("%s\n", __func__);
	gpio_direction_output(DISPLAY_SHUTDOWN, 0);
	IOMUX_SETUP(lcd_pads_enable);
}

static void lcd_disable_pins(void)
{
	pr_debug("%s\n", __func__);
	gpio_direction_output(DISPLAY_SHUTDOWN, 1);
	IOMUX_SETUP(lcd_pads_disable);
}

static struct fsl_mxc_lcd_platform_data lcdif_data = {
	.ipu_id = 0,
	.disp_id = 0,
	.default_ifmt = IPU_PIX_FMT_RGB565,
	.enable_pins = lcd_enable_pins,
	.disable_pins = lcd_disable_pins,
};

static struct ipuv3_fb_platform_data fb_data = {
	.disp_dev = "lcd",
	.interface_pix_fmt = IPU_PIX_FMT_RGB24,
	.mode_str = "qvga60",
	.default_bpp = 32,
	.int_clk = false,
	.late_init = false,
};

static struct imx_ipuv3_platform_data ipu_data = {
	.rev = 4,
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

static struct fsl_mxc_capture_platform_data capture_data = {
	.ipu = 0,
	.csi = 0,
	.is_mipi = 1,
	.mclk_source = 0,
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

static struct regulator_consumer_supply vmmc_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.2"),
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.3"),
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.4"),
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

static struct regulator_consumer_supply sgtl5000_consumer_vdda = {
	.supply = "VDDA",
	.dev_name = "0-000a",
};

static struct regulator_consumer_supply sgtl5000_consumer_vddio = {
	.supply = "VDDIO",
	.dev_name = "0-000a",
};

static struct regulator_consumer_supply sgtl5000_consumer_vddd = {
	.supply = "VDDD",
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

static struct regulator_init_data sgtl5000_vddd_reg_initdata = {
	.num_consumer_supplies = 1,
	.consumer_supplies = &sgtl5000_consumer_vddd,
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

static struct fixed_voltage_config sgtl5000_vddd_reg_config = {
	.supply_name		= "VDDD",
	.microvolts		= 0,
	.gpio			= -1,
	.init_data		= &sgtl5000_vddd_reg_initdata,
};

static struct platform_device sgtl5000_vdda_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 0,
	.dev	= {
		.platform_data = &sgtl5000_vdda_reg_config,
	},
};

static struct platform_device sgtl5000_vddio_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 1,
	.dev	= {
		.platform_data = &sgtl5000_vddio_reg_config,
	},
};

static struct platform_device sgtl5000_vddd_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 2,
	.dev	= {
		.platform_data = &sgtl5000_vddd_reg_config,
	},
};

static int imx6_init_audio(void)
{
	mxc_register_device(&audio_device,
			    &audio_data);
	imx6q_add_imx_ssi(1, &ssi_pdata);
	platform_device_register(&sgtl5000_vdda_reg_devices);
	platform_device_register(&sgtl5000_vddio_reg_devices);
	platform_device_register(&sgtl5000_vddd_reg_devices);
	return 0;
}

/* PWM1_PWMO: backlight control on LDB connector */
static struct platform_pwm_backlight_data pwm_backlight_data = {
	.pwm_id = 0,
	.max_brightness = 255,
	.dft_brightness = 255,
	.pwm_period_ns = 50000,
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

pr_err("%s:%s\n", __FILE__, __func__);
	for_each_tag(t, tags) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			/* GPU reserved memory */
			str = t->u.cmdline.cmdline;
			str = strstr(str, "gpumem=");
			if (str != NULL) {
				str += 7;
				imx6_gpu_pdata.reserved_mem_size 
					= memparse(str, &str);
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

void rcu_cpu_stall_reset(void);

static void poweroff(void)
{
	int i = 0;
	pr_info("%s: %s\n", __FILE__, __func__);
	while (1) {
		gpio_direction_output(POWERDOWN_REPLY, 0);
		touch_softlockup_watchdog_sync();
		touch_all_softlockup_watchdogs();
		rcu_cpu_stall_reset();
		__raw_writew(0x5555, IO_ADDRESS(MX6Q_WDOG2_BASE_ADDR+2));
		__raw_writew(0xaaaa, IO_ADDRESS(MX6Q_WDOG2_BASE_ADDR+2));
		__raw_writew(0x5555, IO_ADDRESS(MX6Q_WDOG1_BASE_ADDR+2));
		__raw_writew(0xaaaa, IO_ADDRESS(MX6Q_WDOG1_BASE_ADDR+2));
		mdelay(100);
		if (0 == (i & 127)) {
			pr_info("%s\n", __func__);
		}
		i++;
	}
}

static struct gpio gpios[] __initdata = {
	{.label = "tsc2004-int",	.gpio = TOUCH_IRQ,		.flags = GPIOF_DIR_IN},
	{.label = "display-shut",	.gpio = DISPLAY_SHUTDOWN,	.flags = 0},
	{.label = "poweroff",		.gpio = POWERDOWN_REPLY,	.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "mipi-pwn",		.gpio = MIPI_PWN,		.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "mipi-rst",		.gpio = MIPI_RST,		.flags = 0},
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
	GPIO_BUTTON(POWERDOWN_NOTICE, KEY_POWER, 1, "key-power", 1),
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

static struct led_pwm pwm_leds[] = {
	{
		.name           = "buzzer",
		.default_trigger = "what_default_trigger",
		.pwm_id         = 3,
		.active_low     = 0,
		.max_brightness = 0x100,
		.pwm_period_ns  = 3822192/32,	/* middle "C" is 261.63 hz or */
	},
};

static struct led_pwm_platform_data plat_led = {
	.leds = pwm_leds,
	.num_leds = 1,
};

static struct platform_device platdev_leds_pwm = {
	.name   = "leds_pwm",
	.dev    = {
			.platform_data  = &plat_led,
	},
};

/*!
 * Board specific initialization.
 */
static void __init board_init(void)
{
	struct clk *clko2;
	struct clk *new_parent;
	int rate;
	struct platform_device *voutdev;
	int ret;

	ret = gpio_request_array(gpios, ARRAY_SIZE(gpios));
	if (ret)
		pr_err("%s gpio_request_array failed(%d)\n", __func__, ret);
	IOMUX_SETUP(common_pads);

	add_device_buttons();

	audio_data.ext_port = 3;

	printk(KERN_ERR "------------ Board type CAD\n");

	imx6q_add_mipi_csi2(&mipi_csi2_pdata);
	gp_reg_id = dvfscore_data.reg_id;
	soc_reg_id = dvfscore_data.soc_id;
	pu_reg_id = dvfscore_data.pu_id;

	imx6q_add_imx_uart(1, NULL);
	imx6q_add_imx_uart(2, &uart3_data);
	imx6q_add_ipuv3(0, &ipu_data);
	imx6q_add_ipuv3fb(0, &fb_data);
	imx6q_add_lcdif(&lcdif_data);
	imx6q_add_vdoa();
	init_usb();

	voutdev = imx6q_add_v4l2_output(0);
	if (vout_mem.res_msize && voutdev) {
		dma_declare_coherent_memory(&voutdev->dev,
					    vout_mem.res_mbase,
					    vout_mem.res_mbase,
					    vout_mem.res_msize,
					    (DMA_MEMORY_MAP |
					     DMA_MEMORY_EXCLUSIVE));
	}
	imx6q_add_v4l2_capture(0, &capture_data);

	imx6q_add_imx_i2c(0, &i2c_data);
	imx6q_add_imx_i2c(1, &i2c_data);
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
	imx6q_add_pm_imx(0, &pm_data);
	imx6q_add_sdhci_usdhc_imx(3, &sd4_data);
	imx6q_add_sdhci_usdhc_imx(2, &sd3_data);
	imx6q_add_sdhci_usdhc_imx(1, &sd2_data);
	imx_add_viv_gpu(&imx6_gpu_data, &imx6_gpu_pdata);
	imx6q_add_vpu();
	imx6_init_audio();
	platform_device_register(&vmmc_reg_devices);
	imx_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	imx_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	imx6q_add_asrc(&imx_asrc_data);

	imx6q_add_mxc_pwm(0);
	imx6q_add_mxc_pwm_backlight(0, &pwm_backlight_data);
	imx6q_add_mxc_pwm_pdata(2, &pwm3_data);
	imx6q_add_mxc_pwm(3);
	platform_device_register(&platdev_leds_pwm);

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
	imx6q_add_busfreq();
	pm_power_off = poweroff;

	imx6q_add_perfmon(0);
	imx6q_add_perfmon(1);
	imx6q_add_perfmon(2);

	imx6_add_armpmu();
}

extern void __iomem *twd_base;
static void __init timer_init(void)
{
	struct clk *uart_clk;
pr_err("%s:%s\n", __FILE__, __func__);
#ifdef CONFIG_LOCAL_TIMERS
	twd_base = ioremap(LOCAL_TWD_ADDR, SZ_256);
	BUG_ON(!twd_base);
#endif
	mx6_clocks_init(32768, 24000000, 0, 0);

	uart_clk = clk_get_sys("imx-uart.0", NULL);
	early_console_setup(UART2_BASE_ADDR, uart_clk);
}

static struct sys_timer mx6_timer = {
	.init   = timer_init,
};

static void __init reserve(void)
{
	phys_addr_t phys;
pr_err("%s:%s\n", __FILE__, __func__);
#if defined(CONFIG_MXC_GPU_VIV) || defined(CONFIG_MXC_GPU_VIV_MODULE)
	if (imx6_gpu_pdata.reserved_mem_size) {
		phys = memblock_alloc_base(imx6_gpu_pdata.reserved_mem_size,
					   SZ_4K, SZ_1G);
		memblock_remove(phys, imx6_gpu_pdata.reserved_mem_size);
		imx6_gpu_pdata.reserved_mem_base = phys;
	}
#endif

#if defined(CONFIG_ION)
	if (imx_ion_data.heaps[0].size) {
		phys = memblock_alloc(imx_ion_data.heaps[0].size, SZ_4K);
		memblock_remove(phys, imx_ion_data.heaps[0].size);
		imx_ion_data.heaps[0].base = phys;
	}
#endif
}

/*
 * initialize board data structure.
 */
MACHINE_START(MX6_CAD, "Boundary Devices CAD Board")
	/* Maintainer: Boundary Devices */
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = board_init,
	.timer = &mx6_timer,
	.reserve = reserve,
MACHINE_END
