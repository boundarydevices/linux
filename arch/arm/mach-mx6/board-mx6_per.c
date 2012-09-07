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
#include <linux/ion.h>
#include <linux/etherdevice.h>
#include <linux/regulator/anatop-regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/arizona/pdata.h>

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

#if defined(CONFIG_MXC_HDMI_CSI2_TC358743) || defined(CONFIG_MXC_HDMI_CSI2_TC358743_MODULE)
#define TC358743_MIPI_CAMERA
#endif

#if defined(CONFIG_MXC_TVIN_ADV7180) || defined(CONFIG_MXC_TVIN_ADV7180_MODULE)
#define ADV7180_CAMERA
#endif

#include "pads-mx6_per.h"
#define FOR_DL_SOLO
#include "pads-mx6_per.h"

extern char *gp_reg_id;
extern char *soc_reg_id;
extern char *pu_reg_id;
static int caam_enabled;

extern struct regulator *(*get_cpu_regulator)(void);
extern void (*put_cpu_regulator)(void);

#define IOMUX_SETUP(pad_list)	iomux_v3_setup_pads(mx6q_##pad_list, \
		mx6dl_solo_##pad_list)

static int iomux_v3_setup_pads(const iomux_v3_cfg_t *mx6q_pad_list,
		const iomux_v3_cfg_t *mx6dl_solo_pad_list)
{
        const iomux_v3_cfg_t *p = cpu_is_mx6q() ? mx6q_pad_list : mx6dl_solo_pad_list;
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


#define CTRL1000_PREFER_MASTER          (1 << 10)
#define CTRL1000_CONFIG_MASTER          (1 << 11)
#define CTRL1000_MANUAL_CONFIG          (1 << 12)

static int fec_phy_init(struct phy_device *phydev)
{
	unsigned ctrl1000 = 0;
	const unsigned master = CTRL1000_PREFER_MASTER |
			CTRL1000_CONFIG_MASTER | CTRL1000_MANUAL_CONFIG;
	unsigned features = phydev->drv->features;

#define DISABLE_GIGA
#ifdef DISABLE_GIGA
	features &= ~(SUPPORTED_1000baseT_Half |
			SUPPORTED_1000baseT_Full);
#endif
	/* force master mode for 1000BaseT due to chip errata */
	if (features & SUPPORTED_1000baseT_Half)
		ctrl1000 |= ADVERTISE_1000HALF | master;
	if (features & SUPPORTED_1000baseT_Full)
		ctrl1000 |= ADVERTISE_1000FULL | master;
	phydev->advertising = phydev->supported = features;
	phy_write(phydev, MII_CTRL1000, ctrl1000);

	if ((phydev->phy_id & 0x00fffff0) == PHY_ID_KSZ9031) {
		ksz9031_send_phy_cmds(phydev, ksz9031_por_cmds);
		return 0;
	}
	/* KSZ9021 */
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
	GP_ECSPI3_WM5102_CS,
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

#if defined(CONFIG_SND_SOC_WM5102) || defined(CONFIG_SND_SOC_WM5102_MODULE)
static struct arizona_pdata wm5102_reva_pdata = {
	.reset = GP_WM5102_RESET,
	.ldoena = GP_WM5102_LDOENA,
	.clk32k_src = ARIZONA_32KZ_MCLK2,
	.inmode = {1, 1, 1, 1},
	.irq_base = MXC_BOARD_IRQ_START,
	.irq_gpio = GP_WM5102_IRQ,
};
#endif

static struct fsl_mxc_camera_platform_data gs2971_data;

#ifdef CONFIG_GS2971_AUDIO
static struct imx_ssi_platform_data gs2971_ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

static struct platform_device gs2971_audio_device = {
	.name = "imx-gs2971-audio",
};

static struct mxc_audio_platform_data gs2971_audio_data = {
	.ssi_num = 1,
	.src_port = 2,	/* ssi0 - 1, ssi1 - 2, ssi2 - 7 */
	.ext_port = 4,	/* AUD4_ */
	.hp_gpio = -1,
};
#endif
static void gs2971_io_init(void)
{

	pr_info("%s\n", __func__);
	if (gs2971_data.cea861)
		IOMUX_SETUP(gs2971_video_pads_cea861);
	else
		IOMUX_SETUP(gs2971_video_pads_no_cea861);
	gpio_set_value(GP_GS2971_RESET, 0);

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

static struct spi_board_info spi_devices[] __initdata = {
#if defined(CONFIG_MTD_M25P80)
	{
		.modalias = "m25p80",
		.max_speed_hz = 20000000, /* max spi clock (SCK) speed in HZ */
		.bus_num = 0,
		.chip_select = 0,
		.platform_data = &spi_flash_data,
	},
#endif
	{
		.modalias = "gs2971",
		.max_speed_hz = 20000000, /* max spi clock (SCK) speed in HZ */
		.bus_num = 2,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.platform_data = &gs2971_data,
	},
#if defined(CONFIG_SND_SOC_WM5102) || defined(CONFIG_SND_SOC_WM5102_MODULE)
	{
		.modalias       = "wm5102",
		.max_speed_hz   = 10000000,
		.bus_num        = 2,
		.chip_select    = 1,
		.mode           = SPI_MODE_0,
		.irq            = gpio_to_irq(GP_WM5102_IRQ),
		.platform_data = &wm5102_reva_pdata,
	},
#endif
};

static void spi_device_init(void)
{
	spi_register_board_info(spi_devices,
				ARRAY_SIZE(spi_devices));
}

static struct mxc_audio_platform_data wm5102_audio_data;

static int wm5102_init(void)
{
	struct clk *clko2;
	struct clk *new_parent;
	int rate;

	clko2 = clk_get(NULL, "clko2_clk");
	if (IS_ERR(clko2)) {
		pr_err("can't get clko2 clock.\n");
		return PTR_ERR(clko2);
	}
	new_parent = clk_get(NULL, "osc_clk");
	if (!IS_ERR(new_parent)) {
		clk_set_parent(clko2, new_parent);
		clk_put(new_parent);
	}
	rate = clk_round_rate(clko2, 24000000);
	if (rate < 8000000 || rate > 27000000) {
		pr_err("Error:wm5102 mclk freq %d out of range!\n", rate);
		clk_put(clko2);
		return -EINVAL;
	}
	wm5102_audio_data.sysclk = rate;
	clk_set_rate(clko2, rate);
	clk_enable(clko2);

	pr_info("%s:clko2 rate=%d\n", __func__, rate);
	return 0;
}

static struct imx_ssi_platform_data ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

static struct mxc_audio_platform_data wm5102_audio_data = {
	.ssi_num = 0,
	.src_port = 1,	/* ssi0 - 1, ssi1 - 2, ssi2 - 7 */
	.ext_port = 6,	/* AUD6_ */
	.init = wm5102_init,
	.hp_gpio = -1,
};

static struct platform_device wm5102_audio_device = {
	.name = "imx-wm5102",
};

static struct imxi2c_platform_data i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info i2c0_board_info[] __initdata = {
	/* microcontroller KL04Z32TFK4 */
};

#ifdef ADV7180_CAMERA
static void adv7180_pwdn(int powerdown)
{
	pr_info("%s: powerdown=%d, reset_gp=0x%x\n",
			__func__, powerdown, GP_ADV7180_RESET);
//	gpio_set_value(GP_ADV7180_CSI1_PWR, powerdown ? 0 : 1);
}

static struct fsl_mxc_tvin_platform_data adv7180_data;

static void adv7180_io_init(void)
{
	int reset_gp = GP_ADV7180_RESET;
	pr_info("%s: reset_gp=0x%x\n", __func__, reset_gp);
	if (adv7180_data.cea861)
		IOMUX_SETUP(adv7180_video_pads_cea861);
	else
		IOMUX_SETUP(adv7180_video_pads_no_cea861);
	/* Camera reset */
	gpio_set_value(reset_gp, 0);
	msleep(2);
	gpio_set_value(reset_gp, 1);
}

static struct fsl_mxc_tvin_platform_data adv7180_data = {
	.pwdn = adv7180_pwdn,
	.io_init = adv7180_io_init,
	.cvbs = true,
	.ipu = 0,
	.csi = 0,
	.cea861 = 0,
};
#endif

#ifdef TC358743_MIPI_CAMERA
#ifdef CONFIG_TC358743_AUDIO
static struct imx_ssi_platform_data tc_ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

static struct platform_device tc_audio_device = {
	.name = "imx-tc358743",
};

static struct mxc_audio_platform_data tc_audio_data = {
	.ssi_num = 2,
	.src_port = 7,	/* ssi0 - 1, ssi1 - 2, ssi2 - 7 */
	.ext_port = 5,	/* AUD5_ */
	.hp_gpio = -1,
};
#endif
/*
 * (tc358743 Mipi-CSI2 bridge) - J16
 * NANDF_WP_B	GPIO[6]:9	Nitrogen6x - RESET
 * NANDF_D5 	GPIO[2]:5	Nitrogen6x/SOM - TC358743 INT
 * NANDF_CS0	GPIO[6]:11	reset, old rev SOM jumpered
 * SD1_DAT1	GPIO[1]:16	24 Mhz XCLK/XVCLK (pwm3)
 */
static struct fsl_mxc_camera_platform_data tc358743_mipi_data;

static void tc358743_mipi_camera_io_init(void)
{
	pr_info("%s\n", __func__);
	gpio_set_value(GP_TC3587_RESET, 0);
	msleep(1);
	gpio_set_value(GP_TC3587_RESET, 1);	/* release reset */
	msleep(200);
}

static void tc358743_mipi_camera_powerdown(int powerdown)
{
	pr_info("%s: powerdown=%d, reset_gp=0x%x\n",
			__func__, powerdown, GP_TC3587_RESET);
}

static struct fsl_mxc_camera_platform_data tc358743_mipi_data = {
	.mclk = 27000000,
	.ipu = 0,
	.csi = 0,
	.io_init = tc358743_mipi_camera_io_init,
	.pwdn = tc358743_mipi_camera_powerdown,
};
#endif

static struct i2c_board_info i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("mxc_hdmi_i2c", 0x50),
	},
};

static struct i2c_board_info i2c2_board_info[] __initdata = {
#ifdef ADV7180_CAMERA
	{
		I2C_BOARD_INFO("adv7180", 0x20),
		.platform_data = (void *)&adv7180_data,
		.irq = gpio_to_irq(IMX_GPIO_NR(5, 0)),  /* EIM_WAIT */
	},
#endif
};

/*
 **********************************************************************
 */
/* i2C bus has a switch */
static const unsigned i2c1_gpiomux_gpios[] = {
	GP_TC3587_I2C_EN,	/* i2c3 */
};

static const unsigned i2c1_gpiomux_values[] = {
	1,
};

static struct gpio_i2cmux_platform_data i2c1_i2cmux_data = {
	.parent		= 1,
	.base_nr	= 3, /* optional */
	.values		= i2c1_gpiomux_values,
	.n_values	= ARRAY_SIZE(i2c1_gpiomux_values),
	.gpios		= i2c1_gpiomux_gpios,
	.n_gpios	= ARRAY_SIZE(i2c1_gpiomux_gpios),
	.idle		= 0,
};

static struct platform_device i2c1_i2cmux = {
        .name           = "gpio-i2cmux",
        .id             = 0,
        .dev            = {
                .platform_data  = &i2c1_i2cmux_data,
        },
};

static struct i2c_board_info i2c3_board_info[] __initdata = {
#ifdef TC358743_MIPI_CAMERA
	{
		I2C_BOARD_INFO("tc358743_mipi", 0x0f),
		.platform_data = (void *)&tc358743_mipi_data,
		.irq = gpio_to_irq(GP_TC3587_IRQ),
	},
#endif
};

static void usbotg_vbus(bool on)
{
	gpio_set_value(GP_USB_OTG_PWR, on ? 1 : 0);
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

static struct imx_asrc_platform_data imx_asrc_data = {
	.channel_bits = 4,
	.clk_map_ver = 2,
};

static struct ipuv3_fb_platform_data fb_data[] = {
	{ /*fb0*/
	.disp_dev = "hdmi",
	.interface_pix_fmt = IPU_PIX_FMT_RGB24,
	.mode_str = "1280x720M@60",
	.default_bpp = 32,
	.int_clk = false,
	}, {
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "1280x720M@60",
	.default_bpp = 16,
	.int_clk = false,
	},
};

static struct fsl_mxc_ldb_platform_data ldb_data = {
	.ipu_id = 1,
	.disp_id = 0,
	.ext_ref = 1,
	.mode = LDB_SEP0,
	.sec_ipu_id = 1,
	.sec_disp_id = 1,
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
	.ipu_id = 0,
	.disp_id = 1,
};

static struct imx_ipuv3_platform_data ipu_data[] = {
	{
	.rev = 4,
	}, {
	.rev = 4,
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

static struct fsl_mxc_capture_platform_data capture_data[] = {
	{	/* Adv7180 */
		.ipu = 0,
		.csi = 0,
	},
	{	/* TC3587 doesn't support virtual channel 1, must use csi0 */
		.ipu = 0,
		.csi = 0,
		.is_mipi = 1,
	},
	{	/* GS2971 */
		.ipu = 1,
		.csi = 1,
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


static struct regulator_consumer_supply vmmc_consumers[] = {
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

#if defined(CONFIG_SND_SOC_WM5102) || defined(CONFIG_SND_SOC_WM5102_MODULE)

static struct regulator_consumer_supply wm5102_consumer_5v[] = {
	REGULATOR_SUPPLY("SPKVDDL", "spi2.1"),
	REGULATOR_SUPPLY("SPKVDDR", "spi2.1"),
	REGULATOR_SUPPLY("SPKVDDL", "wm5102-codec"),
	REGULATOR_SUPPLY("SPKVDDR", "wm5102-codec"),
};

static struct regulator_consumer_supply wm5102_consumer_1p8v[] = {
	REGULATOR_SUPPLY("DBVDD1", "spi2.1"),
	REGULATOR_SUPPLY("DBVDD2", "spi2.1"),
	REGULATOR_SUPPLY("DBVDD3", "spi2.1"),
	REGULATOR_SUPPLY("AVDD", "spi2.1"),
	REGULATOR_SUPPLY("LDOVDD", "spi2.1"),
	REGULATOR_SUPPLY("CPVDD", "spi2.1"),

	REGULATOR_SUPPLY("DBVDD2", "wm5102-codec"),
	REGULATOR_SUPPLY("DBVDD3", "wm5102-codec"),
	REGULATOR_SUPPLY("CPVDD", "wm5102-codec"),
};

static struct regulator_init_data wm5102_5v_reg_initdata = {
	.num_consumer_supplies = ARRAY_SIZE(wm5102_consumer_5v),
	.consumer_supplies = wm5102_consumer_5v,
};

static struct regulator_init_data wm5102_1p8v_reg_initdata = {
	.num_consumer_supplies = ARRAY_SIZE(wm5102_consumer_1p8v),
	.consumer_supplies = wm5102_consumer_1p8v,
};

static struct fixed_voltage_config wm5102_5v_reg_config = {
	.supply_name		= "V_5",
	.microvolts		= 5000000,
	.gpio			= -1,
	.init_data		= &wm5102_5v_reg_initdata,
};

static struct fixed_voltage_config wm5102_1p8v_reg_config = {
	.supply_name		= "V_1P8",
	.microvolts		= 1800000,
	.gpio			= -1,
	.init_data		= &wm5102_1p8v_reg_initdata,
};

static struct platform_device wm5102_5v_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 1,
	.dev	= {
		.platform_data = &wm5102_5v_reg_config,
	},
};

static struct platform_device wm5102_1p8v_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id	= 2,
	.dev	= {
		.platform_data = &wm5102_1p8v_reg_config,
	},
};

#endif /* CONFIG_SND_SOC_WM5102 */

static int init_audio(void)
{
	mxc_register_device(&wm5102_audio_device,
			    &wm5102_audio_data);
	imx6q_add_imx_ssi(0, &ssi_pdata);
#if defined(CONFIG_SND_SOC_WM5102) || defined(CONFIG_SND_SOC_WM5102_MODULE)
	platform_device_register(&wm5102_1p8v_reg_devices);
	platform_device_register(&wm5102_5v_reg_devices);
#endif
#ifdef CONFIG_GS2971_AUDIO
	mxc_register_device(&gs2971_audio_device,
			    &gs2971_audio_data);
	imx6q_add_imx_ssi(1, &gs2971_ssi_pdata);
#endif
#ifdef CONFIG_TC358743_AUDIO
	mxc_register_device(&tc_audio_device,
			    &tc_audio_data);
	imx6q_add_imx_ssi(2, &tc_ssi_pdata);
#endif
	return 0;
}

/* PWM1_PWMO: backlight control on DRGB connector */
static struct platform_pwm_backlight_data pwm1_backlight_data = {
	.pwm_id = 0,	/* pin SD1_DATA3 - PWM1 */
	.max_brightness = 256,
	.dft_brightness = 256,
	.pwm_period_ns = 1000000000/32768,
};

/* PWM2_PWMO: backlight control on LVDS1 connector */
static struct platform_pwm_backlight_data pwm2_backlight_data = {
	.pwm_id = 1,	/* pin SD1_DAT2 - PWM2 */
	.max_brightness = 256,
	.dft_brightness = 128,
	.pwm_period_ns = 50000,
};

static struct mxc_pwm_platform_data pwm3_data = {
	.clk_select = PWM_CLK_HIGHPERF,
};

/* PWM4_PWMO: backlight control on LVDS0 connector */
static struct platform_pwm_backlight_data pwm4_backlight_data = {
	.pwm_id = 3,	/* pin SD1_CMD - PWM4 */
	.max_brightness = 256,
	.dft_brightness = 128,
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
	int i = 0;
	struct ipuv3_fb_platform_data *pdata_fb = fb_data;

	for_each_tag(t, tags) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			str = t->u.cmdline.cmdline;
			str = strstr(str, "fbmem=");
			if (str != NULL) {
				str += 6;
				pdata_fb[i].res_size[0] = memparse(str, &str);
				if (i == 0)
					pdata_fb[i].res_size[1] = pdata_fb[i].res_size[0];
				i++;
				while (*str == ',' &&
					i < ARRAY_SIZE(fb_data)) {
					str++;
					pdata_fb[i++].res_size[0] = memparse(str, &str);
				}
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

static struct mipi_csi2_platform_data mipi_csi2_pdata = {
	.dphy_clk = "mipi_pllref_clk",
	.pixel_clk = "emi_clk",
	.cfg_clk = "hdmi_isfr_clk",
};

static int __init caam_setup(char *__unused)
{
	caam_enabled = 1;
	return 1;
}
early_param("caam", caam_setup);

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
	{.label = "adv7180_reset",	.gpio = GP_ADV7180_RESET,	.flags = 0},
	{.label = "tc3587_reset",	.gpio = GP_TC3587_RESET,	.flags = 0},
	{.label = "tc3587_video_detect", .gpio = GP_TC3587_VIDEO_DETECT,	.flags = GPIOF_DIR_IN},
	{.label = "tc3587_hpd_in",	.gpio = GP_TC3587_HPD_IN,	.flags = GPIOF_DIR_IN},
	{.label = "gs-reset",		.gpio = GP_GS2971_RESET,	.flags = 0},
	{.label = "gs-standby",		.gpio = GP_GS2971_STANDBY,	.flags = GPIOF_OUT_INIT_HIGH},
	{.label = "gs-smpte",		.gpio = GP_GS2971_SMPTE_BYPASS,	.flags = GPIOF_DIR_IN},
	{.label = "gs-dvi_lock",	.gpio = GP_GS2971_DVI_LOCK,	.flags = GPIOF_DIR_IN},
	{.label = "gs-data_err",	.gpio = GP_GS2971_DATA_ERR,	.flags = GPIOF_DIR_IN},
	{.label = "gs-lb_cont",		.gpio = GP_GS2971_LB_CONT,	.flags = GPIOF_DIR_IN},
	{.label = "gs-y_1anc",		.gpio = GP_GS2971_Y_1ANC,	.flags = GPIOF_DIR_IN},
	{.label = "gs-rc_bypas",	.gpio = GP_GS2971_RC_BYPASS,	.flags = 0},
	{.label = "gs-ioproc_en",	.gpio = GP_GS2971_IOPROC_EN,	.flags = 0},
	{.label = "gs-audio_en",	.gpio = GP_GS2971_AUDIO_EN,	.flags = 0},
	{.label = "gs-tim_861",		.gpio = GP_GS2971_TIM_861,	.flags = 0},
	{.label = "gs-sw_en",		.gpio = GP_GS2971_SW_EN,	.flags = 0},
	{.label = "gs-dvb_asi",		.gpio = GP_GS2971_DVB_ASI,	.flags = 0},
};

static struct gpio export_gpios[] __initdata = {
	{.label = "radio-on",		.gpio = GP_PCIE_RADIO_ON,	.flags = 0},
	{.label = "kl04-reset",		.gpio = GP_KL04_RESET,		.flags = 0},	/* inverted before kl04 */
	{.label = "kl04-program",	.gpio = GP_KL04_PROGRAM,	.flags = 0},	/* inverted before kl04 */
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
		int pressed = 0;
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
	int i;
	int ret;
	struct platform_device *voutdev;

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

	printk(KERN_ERR "------------ Board type PER\n");
	for (i = 0; i < ARRAY_SIZE(export_gpios); i++) {
		int gpio = export_gpios[i].gpio;

		pr_info("%s: exporting gpio %d\n", __func__, gpio);
		gpio_export(gpio, 1);
	}

	gp_reg_id = dvfscore_data.reg_id;
	soc_reg_id = dvfscore_data.soc_id;
	pu_reg_id = dvfscore_data.pu_id;

	imx6q_add_imx_uart(0, NULL);

	imx6q_add_imx_uart(1, NULL);
	imx6q_add_imx_uart(2, NULL);

	imx6q_add_imx_uart(3, NULL);
	imx6q_add_imx_uart(4, NULL);

	if (!cpu_is_mx6q()) {
		ldb_data.ipu_id = 0;
		ldb_data.sec_ipu_id = 0;
	}
	imx6q_add_mxc_hdmi_core(&hdmi_core_data);

	imx6q_add_ipuv3(0, &ipu_data[0]);
	if (cpu_is_mx6q()) {
		imx6q_add_ipuv3(1, &ipu_data[1]);
	} else {
		gs2971_data.ipu = 0;
	}
	for (i = 0; i < ARRAY_SIZE(fb_data); i++)
		imx6q_add_ipuv3fb(i, &fb_data[i]);

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

	for (i = 0; i < ARRAY_SIZE(capture_data); i++) {
		if (!cpu_is_mx6q())
			capture_data[i].ipu = 0;
		imx6q_add_v4l2_capture(i, &capture_data[i]);
	}

	imx6q_add_mipi_csi2(&mipi_csi2_pdata);
	imx6q_add_imx_snvs_rtc();

	if (1 == caam_enabled)
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

	mxc_register_device(&i2c1_i2cmux, &i2c1_i2cmux_data);
	i2c_register_board_info(3, i2c3_board_info,
			ARRAY_SIZE(i2c3_board_info));
	/* SPI */
	imx6q_add_ecspi(0, &ecspi1_data);
	imx6q_add_ecspi(2, &ecspi3_data);
	spi_device_init();

	imx6q_add_mxc_hdmi(&hdmi_data);

	imx6q_add_anatop_thermal_imx(1, &anatop_thermal_data);
	imx6_init_fec(fec_data);
	imx6q_add_pm_imx(0, &pm_data);
	imx6q_add_sdhci_usdhc_imx(3, &sd4_data);
	imx_add_viv_gpu(&imx6_gpu_data, &imx6_gpu_pdata);
	init_usb();
	imx6q_add_vpu();
	init_audio();
	platform_device_register(&vmmc_reg_devices);
	imx_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	imx_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	imx6q_add_asrc(&imx_asrc_data);

	/* release USB Hub reset */
	gpio_set_value(GP_USB_HUB_RESET, 1);

	imx6q_add_mxc_pwm(0);
	imx6q_add_mxc_pwm(1);
	imx6q_add_mxc_pwm_pdata(2, &pwm3_data);
	imx6q_add_mxc_pwm(3);

	imx6q_add_mxc_pwm_backlight(0, &pwm1_backlight_data);
	imx6q_add_mxc_pwm_backlight(1, &pwm2_backlight_data);
	imx6q_add_mxc_pwm_backlight(3, &pwm4_backlight_data);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&dvfscore_data);
	imx6q_add_ion(0, &imx_ion_data,
		sizeof(imx_ion_data) + sizeof(struct ion_platform_heap));

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
	phys_addr_t phys;
	int i, j;

#if defined(CONFIG_MXC_GPU_VIV) || defined(CONFIG_MXC_GPU_VIV_MODULE)
	if (imx6_gpu_pdata.reserved_mem_size) {
		phys = memblock_alloc_base(imx6_gpu_pdata.reserved_mem_size,
					   SZ_4K, SZ_2G);
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

	for (i = 0; i < ARRAY_SIZE(fb_data); i++) {
		for (j = 0; j < ARRAY_SIZE(fb_data[i].res_size); j++) {
			resource_size_t sz = fb_data[i].res_size[j];

			if (!sz)
				continue;
			pr_info("%s: fb%d reserve %x\n", __func__, i, sz);
			/* reserve for background buffer */
			phys = memblock_alloc(sz, SZ_4K);
			memblock_remove(phys, sz);
			fb_data[i].res_base[j] = phys;
		}
	}
	if (vout_mem.res_msize) {
		phys = memblock_alloc_base(vout_mem.res_msize,
					   SZ_4K, SZ_2G);
		memblock_remove(phys, vout_mem.res_msize);
		vout_mem.res_mbase = phys;
	}
}

/*
 * initialize __mach_desc_ data structure.
 */
MACHINE_START(MX6_PER, "Boundary Devices PER Board")
	/* Maintainer: Boundary Devices */
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = board_init,
	.timer = &timer,
	.reserve = reserve,
MACHINE_END
