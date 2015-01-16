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
#include <mach/imx-uart.h>
#include <mach/viv_gpu.h>
#include <mach/ahci_sata.h>
#include <mach/ipu-v3.h>
#include <mach/mxc_hdmi.h>
#include <mach/mxc_asrc.h>
#include <mach/imx_rfkill.h>
#include <linux/i2c/tsc2007.h>
#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT) || \
	defined(CONFIG_TOUCHSCREEN_ATMEL_MXT_MODULE)
#include <linux/i2c/atmel_mxt_ts.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_ILI210X) \
	|| defined(CONFIG_TOUCHSCREEN_ILI210X_MODULE)
#include <linux/input/ili210x.h>
#endif
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

#define GP_SD3_CD		IMX_GPIO_NR(7, 0)
#define GP_SD3_WP		IMX_GPIO_NR(7, 1)
#define GP_SD4_CD		IMX_GPIO_NR(2, 6)
#define GP_SD4_WP		IMX_GPIO_NR(2, 7)
#define GP_ECSPI1_CS1		IMX_GPIO_NR(3, 19)
#define GP_USB_OTG_PWR		IMX_GPIO_NR(3, 22)
#define GP_CAP_TCH_INT1		IMX_GPIO_NR(1, 9)
#define GP_DRGB_IRQGPIO		IMX_GPIO_NR(4, 20)
#define GP_USB_HUB_RESET	IMX_GPIO_NR(7, 12)
#define GP_CAN1_STBY		IMX_GPIO_NR(1, 2)
#define GP_CAN1_EN		IMX_GPIO_NR(1, 4)
#define GP_CAN1_ERR		IMX_GPIO_NR(1, 7)
#define GP_MENU_KEY		IMX_GPIO_NR(2, 1)
#define GP_BACK_KEY		IMX_GPIO_NR(2, 2)
#define GP_ONOFF_KEY		IMX_GPIO_NR(2, 3)
#define GP_HOME_KEY		IMX_GPIO_NR(2, 4)
#define GP_VOL_UP_KEY		IMX_GPIO_NR(7, 13)
#define GP_VOL_DOWN_KEY		IMX_GPIO_NR(4, 5)

#define GP_CSI0_RST		IMX_GPIO_NR(1, 8)
#define GP_CSI0_PWN		IMX_GPIO_NR(1, 6)
#define GP_CSI1_PWN		IMX_GPIO_NR(3, 13)
#define GP_CSI1_RST		IMX_GPIO_NR(3, 14)
#define GP_MIPI_PWN		IMX_GPIO_NR(6, 9)
#define GP_MIPI_RST		IMX_GPIO_NR(2, 5)
#define GP_MIPI_RST2		IMX_GPIO_NR(6, 11)

#define GP_ENET_PHY_INT	IMX_GPIO_NR(1, 28)

#define N6_WL1271_WL_IRQ		IMX_GPIO_NR(6, 14)
#define N6_WL1271_WL_EN			IMX_GPIO_NR(6, 15)
#define N6_WL1271_BT_EN			IMX_GPIO_NR(6, 16)

#define CAN1_ERR_TEST_PADCFG	(PAD_CTL_PKE | PAD_CTL_PUE | \
		PAD_CTL_PUS_100K_DOWN | PAD_CTL_SPEED_MED | \
		PAD_CTL_DSE_40ohm | PAD_CTL_HYS)
#define CAN1_ERR_PADCFG		(PAD_CTL_PUE | \
		PAD_CTL_PUS_100K_DOWN | PAD_CTL_SPEED_MED | \
		PAD_CTL_DSE_40ohm | PAD_CTL_HYS)
#define SD3_WP_PADCFG	(PAD_CTL_PKE | PAD_CTL_PUE |	\
		PAD_CTL_PUS_22K_UP | PAD_CTL_SPEED_MED |	\
		PAD_CTL_DSE_40ohm | PAD_CTL_HYS)

#define WEAK_PULLUP	(PAD_CTL_HYS | PAD_CTL_PKE \
			 | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)

#define N6_IRQ_PADCFG		(PAD_CTL_PUE | PAD_CTL_PUS_100K_DOWN | PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_HYS)
#define N6_IRQ_TEST_PADCFG	(PAD_CTL_PKE | N6_IRQ_PADCFG)
#define N6_EN_PADCFG		(PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm)

#if defined(CONFIG_MXC_CAMERA_OV5642) || defined(CONFIG_MXC_CAMERA_OV5642_MODULE)
#define CSI0_CAMERA
#endif

#if defined(CONFIG_MXC_CAMERA_OV5640) || defined(CONFIG_MXC_CAMERA_OV5640_MODULE)
#define CSI1_CAMERA
#endif

#if defined(CONFIG_MXC_CAMERA_OV5640_MIPI) || defined(CONFIG_MXC_CAMERA_OV5640_MIPI_MODULE)
#define MIPI_CAMERA
#endif

#if defined(CONFIG_MXC_HDMI_CSI2_TC358743) || defined(CONFIG_MXC_HDMI_CSI2_TC358743_MODULE)
#define TC358743_MIPI_CAMERA
#endif

#if !defined(MIPI_CAMERA)
#define OV5640_MIPI_IPU -1
#define OV5640_MIPI_CSI -1
#elif !defined(CSI0_CAMERA) && !defined(TC358743_MIPI_CAMERA)
#define OV5640_MIPI_IPU 0
#define OV5640_MIPI_CSI 0
#elif !defined(CSI1_CAMERA)
#define OV5640_MIPI_IPU 1
#define OV5640_MIPI_CSI 1
#else
#define OV5640_MIPI_IPU 0
#define OV5640_MIPI_CSI 1
#endif


#include "pads-mx6_nitrogen6x.h"
#define FOR_DL_SOLO
#include "pads-mx6_nitrogen6x.h"

void __init early_console_setup(unsigned long base, struct clk *clk);
static struct clk *sata_clk;

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

static struct gpio n6w_wl1271_gpios[] __initdata = {
	{.label = "wl1271_int",		.gpio = N6_WL1271_WL_IRQ,	.flags = GPIOF_DIR_IN},
	{.label = "wl1271_bt_en",	.gpio = N6_WL1271_BT_EN,	.flags = 0},
	{.label = "wl1271_wl_en",	.gpio = N6_WL1271_WL_EN,	.flags = 0},
};

static int forcen6 = 0;
module_param(forcen6, int, S_IRUGO | S_IWUSR);

__init static int is_nitrogen6w(void)
{
	int ret = gpio_request_array(n6w_wl1271_gpios,
			ARRAY_SIZE(n6w_wl1271_gpios));
	if (ret) {
		printk(KERN_ERR "%s gpio_request_array failed("
				"%d) for n6w_wl1271_gpios\n", __func__, ret);
		return ret;
	}
	ret = gpio_get_value(N6_WL1271_WL_IRQ) || forcen6;
	if (ret <= 0) {
		/* Sabrelite, not nitrogen6w */
		gpio_free(N6_WL1271_WL_IRQ);
		gpio_free(N6_WL1271_WL_EN);
		gpio_free(N6_WL1271_BT_EN);
		ret = 0;
	}
	return ret;
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
	gpio_set_value(N6_WL1271_WL_EN,on);
}

#ifdef CONFIG_WL12XX_PLATFORM_DATA
static struct esdhc_platform_data sd2_data = {
	.always_present = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.keep_power_at_suspend = 0,
	.caps = MMC_CAP_POWER_OFF_CARD,
	.platform_pad_change = plt_sd_pad_change,
	.set_power = sdio_set_power,
};
#endif

static struct esdhc_platform_data sd3_data = {
	.cd_gpio = GP_SD3_CD,
	.wp_gpio = GP_SD3_WP,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct esdhc_platform_data sd4_data __initconst = {
	.cd_gpio = GP_SD4_CD,
	.wp_gpio = -1,
	.keep_power_at_suspend = 1,
	.platform_pad_change = plt_sd_pad_change,
};

static const struct anatop_thermal_platform_data
	anatop_thermal_data __initconst = {
		.name = "anatop_thermal",
};

static const struct imxuart_platform_data mx6_arm2_uart2_data __initconst = {
	.flags      = IMXUART_HAVE_RTSCTS,
};

#ifdef CONFIG_TI_ST
/* TI-ST for WL1271 BT */
#include "wl12xx-bt.h"

static struct ti_st_plat_data wilink_pdata = {
	.nshutdown_gpio = N6_WL1271_BT_EN,
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

#endif

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
	.phy_irq = gpio_to_irq(GP_ENET_PHY_INT)
};

static int spi_cs[] = {
	GP_ECSPI1_CS1,
};

static const struct spi_imx_master spi_data __initconst = {
	.chipselect     = spi_cs,
	.num_chipselect = ARRAY_SIZE(spi_cs),
};

#if defined(CONFIG_MXC_VIDEO_GS2971) || defined(CONFIG_MXC_VIDEO_GS2971_MODULE)
static int spi3_cs[] = {
	GP_ECSPI3_CS1,
};
static const struct spi_imx_master spi3_data __initconst = {
	.chipselect     = spi3_cs,
	.num_chipselect = ARRAY_SIZE(spi3_cs),
};
#endif

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

#if defined(CONFIG_MXC_VIDEO_GS2971) || defined(CONFIG_MXC_VIDEO_GS2971_MODULE)
static struct fsl_mxc_camera_platform_data gs2971_data;

static struct spi_board_info spi_gs2971_device[] __initdata = {
	{
		.modalias = "gs2971",
		.max_speed_hz = 20000000, /* max spi clock (SCK) speed in HZ */
		.bus_num = 2,
		.chip_select = 0,
		.platform_data = &gs2971_data,
	},
};
#endif

static void spi_device_init(void)
{
	spi_register_board_info(spi_nor_device,
				ARRAY_SIZE(spi_nor_device));
#if defined(CONFIG_MXC_VIDEO_GS2971) || defined(CONFIG_MXC_VIDEO_GS2971_MODULE)
	spi_register_board_info(spi_gs2971_device, ARRAY_SIZE(spi_gs2971_device));
#endif
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
		.irq = gpio_to_irq(IMX_GPIO_NR(6, 7)),	/* NANDF_CLE */
	},
};

static void camera_reset(int power_gp, int poweroff_level, int reset_gp, int reset_gp2)
{
	pr_info("%s: power_gp=0x%x, reset_gp=0x%x reset_gp2=0x%x\n",
			__func__, power_gp, reset_gp, reset_gp2);
	gpio_direction_output(power_gp, poweroff_level);
	/* Camera reset */
	gpio_direction_output(reset_gp, 0);
	if (reset_gp2 >= 0)
		gpio_direction_output(reset_gp2, 0);
	msleep(1);
}

#if defined(CSI0_CAMERA) || defined(MIPI_CAMERA)
/*
 * This lock is to ensure that only 1 camera powers up at a time.
 * The camera's i2c address is changed before the lock is released.
 */
DEFINE_MUTEX(i2c_3c_mutex);

static void lock_i2c_3c_address(void)
{
	mutex_lock(&i2c_3c_mutex);
}

static void unlock_i2c_3c_address(void)
{
	mutex_unlock(&i2c_3c_mutex);
}
#endif

static struct mxc_pwm_platform_data pwm3_data = {
	.clk_select = PWM_CLK_HIGHPERF,
};

#ifdef MIPI_CAMERA
/*
 * (ov5640 Mipi) - J16
 * NANDF_WP_B	GPIO[6]:9	Nitrogen6x - power down, SOM - NC
 * NANDF_D5 	GPIO[2]:5	Nitrogen6x/SOM - CSI0 reset
 * NANDF_CS0	GPIO[6]:11	reset, old rev SOM jumpered
 * SD1_DAT1	GPIO[1]:16	24 Mhz XCLK/XVCLK (pwm3)
 */
static struct pwm_device	*mipi_pwm;
static struct fsl_mxc_camera_platform_data ov5640_mipi_data;
static int ov5640_mipi_reset_active;

static void ov5640_mipi_camera_io_init(void)
{
	IOMUX_SETUP(mipi_pads);

	pr_info("%s\n", __func__);
	if (!mipi_pwm)
		mipi_pwm = pwm_request(2, "mipi_clock");
	if (IS_ERR(mipi_pwm)) {
		pr_err("unable to request PWM for mipi_clock\n");
	} else {
		unsigned period = 1000/22;
		pr_info("got pwm for mipi_clock\n");
		pwm_config(mipi_pwm, period >> 1, period);
		pwm_enable(mipi_pwm);
	}

	camera_reset(GP_MIPI_PWN, 1, GP_MIPI_RST, GP_MIPI_RST2);
	ov5640_mipi_reset_active = 1;
	if (cpu_is_mx6dl()) {
		/*
		 * for mx6dl, mipi virtual channel 0 connect to csi0
		 * virtual channel 1 connect to csi1
		 */
		mxc_iomux_set_gpr_register(13, ov5640_mipi_data.csi * 3, 3, ov5640_mipi_data.csi);
	} else {
		if (ov5640_mipi_data.csi == ov5640_mipi_data.ipu) {
			/* select mipi IPU1 CSI0/ IPU2/CSI1 */
			mxc_iomux_set_gpr_register(1, 19 + ov5640_mipi_data.csi, 1, 0);
		}
	}
}

static void ov5640_mipi_camera_powerdown(int powerdown)
{
	pr_info("%s: powerdown=%d, power_gp=0x%x\n",
			__func__, powerdown, GP_MIPI_PWN);
	if (!IS_ERR(mipi_pwm)) {
		if (powerdown) {
			pwm_disable(mipi_pwm);
		} else {
			unsigned period = 1000/22;
			pwm_config(mipi_pwm, period >> 1, period);
			pwm_enable(mipi_pwm);
		}
	}
	gpio_set_value(GP_MIPI_PWN, powerdown ? 1 : 0);
	if (!powerdown) {
		msleep(2);
		if (ov5640_mipi_reset_active) {
			ov5640_mipi_reset_active = 0;
			gpio_set_value(GP_MIPI_RST, 1);
			gpio_set_value(GP_MIPI_RST2, 1);
			msleep(20);
		}
	}
}

static struct fsl_mxc_camera_platform_data ov5640_mipi_data = {
	.mclk = 22000000,
	.ipu = OV5640_MIPI_IPU,
	.csi = OV5640_MIPI_CSI,
	.io_init = ov5640_mipi_camera_io_init,
	.pwdn = ov5640_mipi_camera_powerdown,
	.lock = lock_i2c_3c_address,
	.unlock = unlock_i2c_3c_address,
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
	.src_port = 3,
	.ext_port = 5,
	.hp_gpio = -1,
};
#endif
/*
 * (tc358743 Mipi-CSI2 bridge) - J16
 * NANDF_WP_B	GPIO[6]:9	Nitrogen6x - RESET
 * NANDF_D5	GPIO[2]:5	Nitrogen6x/SOM - TC358743 INT
 * NANDF_CS0	GPIO[6]:11	reset, old rev SOM jumpered
 * SD1_DAT1	GPIO[1]:16	24 Mhz XCLK/XVCLK (pwm3)
 */
static struct fsl_mxc_camera_platform_data tc358743_mipi_data;

static void tc358743_mipi_camera_io_init(void)
{
	IOMUX_SETUP(mipi_pads);
#ifdef CONFIG_TC358743_AUDIO
	IOMUX_SETUP(tc_audio_pads);
#endif
	pr_info("%s\n", __func__);

	if (cpu_is_mx6dl()) {
		/*
		 * for mx6dl, mipi virtual channel 0 connect to csi0
		 * virtual channel 1 connect to csi1
		 */
		mxc_iomux_set_gpr_register(13, tc358743_mipi_data.csi * 3, 3, tc358743_mipi_data.csi);
	} else {
		/* select mipi IPU1 CSI0/ IPU2/CSI1 */
		mxc_iomux_set_gpr_register(1, 19 + tc358743_mipi_data.csi, 1, 0);//MIPI sensor to IPU-1 mux control
								//0 Enable mipi to IPU1 CSI0 - virtual channel is fixed to 0.
								//1 Enable parallel interface to IPU1 CSI0.
	}
}

static void tc358743_mipi_camera_powerdown(int powerdown)
{
	pr_info("%s: powerdown=%d, power_gp=0x%x\n",
			__func__, powerdown, IMX_GPIO_NR(6, 9));
}

static struct fsl_mxc_camera_platform_data tc358743_mipi_data = {
	.mclk = 27000000,
	.ipu = 0,
	.csi = 0,
	.io_init = tc358743_mipi_camera_io_init,
	.pwdn = tc358743_mipi_camera_powerdown,
};
#endif

#if defined(CSI0_CAMERA)
static int ov564x_reset_active;

/*
 * GPIO_6	GPIO[1]:6	(ov564x) - J5 - CSI0 power down
 * GPIO_8	GPIO[1]:8	(ov564x) - J5 - CSI0 reset
 * NANDF_CS0	GPIO[6]:11	(ov564x) - J5 - reset
 * SD1_DAT0	GPIO[1]:16	(ov564x) - J5 - GP
 */
static void ov564x_io_init(void)
{
	IOMUX_SETUP(csi0_sensor_pads);

	camera_reset(GP_CSI0_PWN, 1, GP_CSI0_RST, IMX_GPIO_NR(6, 11));
	ov564x_reset_active = 1;
	/* For MX6Q GPR1 bit19 and bit20 meaning:
	 * Bit19:       0 - Enable mipi to IPU1 CSI0
	 *                      virtual channel is fixed to 0
	 *              1 - Enable parallel interface to IPU1 CSI0
	 * Bit20:       0 - Enable mipi to IPU2 CSI1
	 *                      virtual channel is fixed to 3
	 *              1 - Enable parallel interface to IPU2 CSI1
	 * IPU1 CSI1 directly connect to mipi csi2,
	 *      virtual channel is fixed to 1
	 * IPU2 CSI0 directly connect to mipi csi2,
	 *      virtual channel is fixed to 2
	 */
	if (cpu_is_mx6q())
		mxc_iomux_set_gpr_register(1, 19, 1, 1);
	else
		mxc_iomux_set_gpr_register(13, 0, 3, 4);
}

static void ov564x_powerdown(int powerdown)
{
	pr_info("%s: powerdown=%d, power_gp=0x%x\n",
			__func__, powerdown, GP_CSI0_PWN);
	gpio_set_value(GP_CSI0_PWN, powerdown ? 1 : 0);
	if (!powerdown) {
		msleep(2);
		if (ov564x_reset_active) {
			ov564x_reset_active = 0;
			gpio_set_value(GP_CSI0_RST, 1);
			gpio_set_value(IMX_GPIO_NR(6, 11), 1);
			msleep(20);
		}
	}
}

static struct fsl_mxc_camera_platform_data ov564x_data = {
	.mclk = 24000000,
	.mclk_source = 0,
	.csi = 0,
	.io_init = ov564x_io_init,
	.pwdn = ov564x_powerdown,
	.lock = lock_i2c_3c_address,
	.unlock = unlock_i2c_3c_address,
};

#endif

static void adv7180_pwdn(int powerdown)
{
	pr_info("%s: powerdown=%d, power_gp=0x%x\n",
			__func__, powerdown, GP_CSI1_PWN);
	gpio_set_value(GP_CSI1_PWN, powerdown ? 0 : 1);
}

static struct fsl_mxc_tvin_platform_data adv7180_data;

static void adv7180_io_init(void)
{
	if (adv7180_data.cea861)
		IOMUX_SETUP(adv7180_video_pads_cea861);
	else
		IOMUX_SETUP(adv7180_video_pads_no_cea861);
	camera_reset(GP_CSI1_PWN, 0, GP_CSI1_RST, -1);
	gpio_set_value(GP_CSI1_PWN, 1);
	msleep(2);
	gpio_set_value(GP_CSI1_RST, 1);

	if (cpu_is_mx6q())
		mxc_iomux_set_gpr_register(1, 20, 1, 1);
	else
		mxc_iomux_set_gpr_register(13, 3, 3, 4);
}

static struct fsl_mxc_tvin_platform_data adv7180_data = {
	.pwdn = adv7180_pwdn,
	.io_init = adv7180_io_init,
	.cvbs = true,
	.ipu = 1,
	.csi = 1,
	.cea861 = 0,
};

#if defined(CONFIG_MXC_VIDEO_GS2971) || defined(CONFIG_MXC_VIDEO_GS2971_MODULE)
/*
 * GS2971
 * EIM_LBA__GPIO_2_27 - power down
 * DISP0_DAT9__GPIO_4_30 - Reset
 */
static struct fsl_mxc_camera_platform_data gs2971_data;

static void gs2971_io_init(void)
{

	IOMUX_SETUP(gs2971_video_pads);

	if (gs2971_data.cea861)
		IOMUX_SETUP(gs2971_video_pads_cea861);

	pr_info("%s\n", __func__);

	camera_reset(GP_GS2971_PWN, 1, GP_GS2971_RST, -1);
	gpio_set_value(GP_GS2971_PWN, 0);
	msleep(2);
	gpio_set_value(GP_GS2971_RST, 1);

	/* Enable parallel port to IPU1/CSI1 */
	if (cpu_is_mx6q())
		mxc_iomux_set_gpr_register(1, 20, 1, 1);
	else
		mxc_iomux_set_gpr_register(13, 3, 3, 4);

	/* Set control pin values */
	gpio_request(IMX_GPIO_NR(4, 28), "TIM_861");
	gpio_request(IMX_GPIO_NR(5, 5), "IOPROC_EN");
	gpio_request(IMX_GPIO_NR(4, 31), "SW_EN");
	gpio_request(IMX_GPIO_NR(3, 13), "PWR_DN");
	gpio_request(IMX_GPIO_NR(5, 7), "LB_CONT");

	gpio_direction_output(IMX_GPIO_NR(4, 28), 0); // TIM_861 = 0
	gpio_direction_output(IMX_GPIO_NR(5, 5), 1); // Enable IOPROC
	gpio_direction_output(IMX_GPIO_NR(4, 31), 0); // sw_en =0
	gpio_direction_output(IMX_GPIO_NR(3, 13), 1); // Enable voltage regulator
	gpio_direction_output(IMX_GPIO_NR(5, 7), 0); // LB_CONT = 0
}

static void gs2971_pwdn(int powerdown)
{
	pr_debug("-> In function %s\n",__FUNCTION__);

	pr_info("%s: powerdown=%d, power_gp=0x%x\n",
			__func__, powerdown, IMX_GPIO_NR(2, 27));
	gpio_set_value(GP_GS2971_PWN, powerdown ? 1 : 0);
	if (!powerdown)
		msleep(2);
}

static struct fsl_mxc_camera_platform_data gs2971_data = {
	.mclk = 74250000,
	.pwdn = gs2971_pwdn,
	.io_init = gs2971_io_init,
	//.cvbs = true,
	.ipu = 1,
	.csi = 1,
	.cea861 = 0,
};
#endif

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("mxc_hdmi_i2c", 0x50),
	},
#ifdef CSI0_CAMERA
	{
		I2C_BOARD_INFO("ov5642", 0x3d),
		.platform_data = (void *)&ov564x_data,
	},
#endif
#ifdef MIPI_CAMERA
	{
		I2C_BOARD_INFO("ov5640_mipi", 0x3e),
		.platform_data = (void *)&ov5640_mipi_data,
	},
#endif
#ifdef TC358743_MIPI_CAMERA
	{
		I2C_BOARD_INFO("tc358743_mipi", 0x0f),
		.platform_data = (void *)&tc358743_mipi_data,
		.irq = gpio_to_irq(IMX_GPIO_NR(2, 5)),
	},
#endif
};

static struct tsc2007_platform_data tsc2007_info = {
	.model			= 2004,
	.x_plate_ohms		= 500,
};

#ifdef CSI1_CAMERA

static struct fsl_mxc_camera_platform_data ov5640_csi1_data;
static int ov5640_reset_active;

static void ov5640_csi1_camera_io_init(void)
{
	pr_info("%s\n", __func__);

	camera_reset(GP_CSI1_PWN, 1, GP_CSI1_RST, -1);
	ov5640_reset_active = 1;
	if (cpu_is_mx6dl()) {
		/*
		 * for mx6dl, parallel connect to csi0
		 * or parallel connect to csi1
		 */
		mxc_iomux_set_gpr_register(13, ov5640_csi1_data.csi * 3, 3, 4);
	} else {
		/* select mipi IPU1 CSI0/ IPU2/CSI1 */
		mxc_iomux_set_gpr_register(1, 19 + ov5640_csi1_data.csi, 1, 1);
	}
}

static void ov5640_csi1_camera_powerdown(int powerdown)
{
	pr_info("%s: powerdown=%d, reset_gp=0x%x, power_gp=0x%x\n",
			__func__, powerdown, GP_CSI1_RST, GP_CSI1_PWN);
	gpio_set_value(GP_CSI1_PWN, powerdown ? 1 : 0);
	if (!powerdown) {
		msleep(2);
		if (ov5640_reset_active) {
			ov5640_reset_active = 0;
			gpio_set_value(GP_CSI1_RST, 1);
			msleep(20);
		}
	}
}

static struct fsl_mxc_camera_platform_data ov5640_csi1_data = {
	.mclk = 24000000,
	.csi = 1,
	.ipu = 1,
	.io_init = ov5640_csi1_camera_io_init,
	.pwdn = ov5640_csi1_camera_powerdown,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT) || \
	defined(CONFIG_TOUCHSCREEN_ATMEL_MXT_MODULE)
static struct mxt_platform_data mxt_data = {
        .irqflags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_ILI210X) \
	|| defined(CONFIG_TOUCHSCREEN_ILI210X_MODULE)
static struct ili210x_platform_data ili_pdata = {
	.irq_flags = IRQF_TRIGGER_FALLING,
	.poll_period = 20,
	.gp = GP_CAP_TCH_INT1
};
#endif

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("egalax_ts", 0x4),
		.irq = gpio_to_irq(GP_CAP_TCH_INT1),
	},
	{
		I2C_BOARD_INFO("tsc2004", 0x48),
		.platform_data	= &tsc2007_info,
		.irq = gpio_to_irq(GP_DRGB_IRQGPIO),
	},
#if defined(CONFIG_TOUCHSCREEN_FT5X06) \
	|| defined(CONFIG_TOUCHSCREEN_FT5X06_MODULE)
	{
		I2C_BOARD_INFO("ft5x06-ts", 0x38),
		.irq = gpio_to_irq(GP_CAP_TCH_INT1),
	},
#endif
#if defined(CONFIG_TOUCHSCREEN_ILI210X) \
	|| defined(CONFIG_TOUCHSCREEN_ILI210X_MODULE)
	{
		I2C_BOARD_INFO("ili210x", 0x41),
		.irq = gpio_to_irq(GP_CAP_TCH_INT1),
		.platform_data = &ili_pdata
	},
#endif
#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT) || \
	defined(CONFIG_TOUCHSCREEN_ATMEL_MXT_MODULE)
	{
		I2C_BOARD_INFO("atmel_mxt_ts", 0x4c), /* i2c address */
		.irq = gpio_to_irq(GP_CAP_TCH_INT1),
                .platform_data = &mxt_data,
	},
#endif
	{
		I2C_BOARD_INFO("adv7180", 0x20),
		.platform_data = (void *)&adv7180_data,
		.irq = gpio_to_irq(IMX_GPIO_NR(5, 0)),  /* EIM_WAIT */
	},
#ifdef CSI1_CAMERA
	{
		I2C_BOARD_INFO("ov5640", 0x3c),
		.platform_data = (void *)&ov5640_csi1_data,
	},
#endif
#if defined(CONFIG_TOUCHSCREEN_FUSION_F0710A) \
       || defined(CONFIG_TOUCHSCREEN_FUSION_F0710A_MODULE)
       {
               I2C_BOARD_INFO("fusion_F0710A", 0x10),
               .irq = gpio_to_irq(GP_CAP_TCH_INT1),
       },
#endif
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
static int init_sata(struct device *dev, void __iomem *addr)
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
	 * one write for rest of parameters, mask of first write is 0x07FFFFFF,
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
	writel(((tmpdata & ~0x07FFFFFF) | 0x0593A044), IOMUXC_GPR13);

	/* enable SATA_PHY PLL */
	tmpdata = readl(IOMUXC_GPR13);
	writel((tmpdata | 0x2), IOMUXC_GPR13);

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

static void exit_sata(struct device *dev)
{
	clk_disable(sata_clk);
	clk_put(sata_clk);
}

static struct ahci_platform_data sata_data = {
	.init = init_sata,
	.exit = exit_sata,
};

static struct gpio flexcan_gpios[] = {
	{ GP_CAN1_ERR, GPIOF_DIR_IN, "flexcan1-err" },
	{ GP_CAN1_EN, GPIOF_OUT_INIT_LOW, "flexcan1-en" },
	{ GP_CAN1_STBY, GPIOF_OUT_INIT_LOW, "flexcan1-stby" },
};

static void flexcan0_mc33902_switch(int enable)
{
	gpio_set_value(GP_CAN1_EN, enable);
	gpio_set_value(GP_CAN1_STBY, enable);
}

static void flexcan0_tja1040_switch(int enable)
{
	gpio_set_value(GP_CAN1_STBY, enable ^ 1);
}

static const struct flexcan_platform_data
	flexcan0_mc33902_pdata __initconst = {
	.transceiver_switch = flexcan0_mc33902_switch,
};

static const struct flexcan_platform_data
	flexcan0_tja1040_pdata __initconst = {
	.transceiver_switch = flexcan0_tja1040_switch,
};

static struct viv_gpu_platform_data imx6_gpu_pdata __initdata = {
	.reserved_mem_size = SZ_128M + SZ_64M - SZ_16M,
};

static struct imx_asrc_platform_data imx_asrc_data = {
	.channel_bits = 4,
	.clk_map_ver = 2,
};

static struct ipuv3_fb_platform_data fb_data[] = {
	{ /*fb0*/
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "LDB-XGA",
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
	}, {
	.disp_dev = "ldb",
	.interface_pix_fmt = IPU_PIX_FMT_RGB666,
	.mode_str = "LDB-VGA",
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

/* On mx6x sbarelite board i2c2 iomux with hdmi ddc,
 * the pins default work at i2c2 function,
 when hdcp enable, the pins should work at ddc function */

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

static void lcd_disable_pins(void)
{
	pr_info("%s\n", __func__);
	IOMUX_SETUP(lcd_pads_disable);
}

#if !defined(CONFIG_MXC_VIDEO_GS2971) && !defined(CONFIG_MXC_VIDEO_GS2971_MODULE) /* We need the pads for GS2971 */
static void lcd_enable_pins(void)
{
	pr_info("%s\n", __func__);
	IOMUX_SETUP(lcd_pads_enable);
}

static struct fsl_mxc_lcd_platform_data lcdif_data = {
	.ipu_id = 0,
	.disp_id = 0,
	.default_ifmt = IPU_PIX_FMT_RGB565,
	.enable_pins = lcd_enable_pins,
	.disable_pins = lcd_disable_pins,
};
#endif

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

static struct fsl_mxc_capture_platform_data capture_data[] = {
#ifdef CSI0_CAMERA
	{
		.ipu = 0,
		.csi = 0,
		.mclk_source = 0,
	},
#endif
#ifdef TC358743_MIPI_CAMERA
	{
		.ipu = 0,
		.csi = 0,
		.is_mipi = 1,
		.mclk_source = 0,
	},
#endif
#ifdef MIPI_CAMERA
	{
		.ipu = OV5640_MIPI_IPU,
		.csi = OV5640_MIPI_CSI,
		.is_mipi = 1,
		.mclk_source = 0,
	},
#endif
#if defined(CSI1_CAMERA) || \
	defined(CONFIG_MXC_TVIN_ADV7180) || defined(CONFIG_MXC_TVIN_ADV7180_MODULE) || \
	defined(CONFIG_MXC_VIDEO_GS2971) || defined(CONFIG_MXC_VIDEO_GS2971_MODULE)
	{
		.ipu = 1,
		.csi = 1,
		.mclk_source = 0,
	},
#endif
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
	GPIO_BUTTON(GP_ONOFF_KEY, KEY_POWER, 1, "key-power", 1),
	GPIO_BUTTON(GP_MENU_KEY, KEY_MENU, 1, "key-memu", 0),
	GPIO_BUTTON(GP_HOME_KEY, KEY_HOME, 1, "key-home", 0),
	GPIO_BUTTON(GP_BACK_KEY, KEY_BACK, 1, "key-back", 0),
	GPIO_BUTTON(GP_VOL_UP_KEY, KEY_VOLUMEUP, 1, "volume-up", 0),
	GPIO_BUTTON(GP_VOL_DOWN_KEY, KEY_VOLUMEDOWN, 1, "volume-down", 0),
};

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
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
#else
static void __init add_device_buttons(void)
{
	int i;
	for (i=0; i < ARRAY_SIZE(buttons);i++) {
		int gpio = buttons[i].gpio;
		pr_debug("%s: exporting gpio %d\n", __func__, gpio);
		gpio_export(gpio,1);
	}
}
#endif

#ifdef CONFIG_WL12XX_PLATFORM_DATA
static struct wl12xx_platform_data n6q_wlan_data __initdata = {
	.irq = gpio_to_irq(N6_WL1271_WL_IRQ),
	.board_ref_clock = WL12XX_REFCLOCK_38, /* 38.4 MHz */
};
#endif

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

#ifdef CONFIG_SND_SOC_SGTL5000

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

#endif /* CONFIG_SND_SOC_SGTL5000 */

static int imx6_init_audio(void)
{
	mxc_register_device(&audio_device,
			    &audio_data);
	imx6q_add_imx_ssi(1, &ssi_pdata);
#ifdef CONFIG_SND_SOC_SGTL5000
	platform_device_register(&sgtl5000_vdda_reg_devices);
	platform_device_register(&sgtl5000_vddio_reg_devices);
	platform_device_register(&sgtl5000_vddd_reg_devices);
#endif
#ifdef CONFIG_TC358743_AUDIO
	mxc_register_device(&tc_audio_device,
			    &tc_audio_data);
	imx6q_add_imx_ssi(2, &tc_ssi_pdata);
#endif
	return 0;
}

/* PWM0_PWMO: backlight control on DRGB connector */
static struct platform_pwm_backlight_data pwm0_backlight_data = {
	.pwm_id = 0,
	.max_brightness = 255,
	.dft_brightness = 255,
	.pwm_period_ns = 1000000000/32768,
};

/* PWM3_PWMO: backlight control on LDB connector */
static struct platform_pwm_backlight_data pwm_backlight_data = {
	.pwm_id = 3,
	.max_brightness = 255,
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

static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
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
				pdata_fb[i++].res_size[0] = memparse(str, &str);
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

static const struct imx_pcie_platform_data pcie_data  __initconst = {
	.pcie_pwr_en	= -EINVAL,
	.pcie_rst	= -EINVAL, //GP_CAP_TCH_INT1,
	.pcie_wake_up	= -EINVAL,
	.pcie_dis	= -EINVAL,
};

#define GPIOF_HIGH GPIOF_OUT_INIT_HIGH

struct gpio initial_gpios[] __initdata = {
	{.label = "ov5642_csi0_pwdn",	.gpio = GP_CSI0_PWN,	.flags = GPIOF_HIGH},
	{.label = "ov5642_csi0_reset",	.gpio = GP_CSI0_RST,	.flags = 0},
	{.label = "ov5640_mipi_pwdn",	.gpio = GP_MIPI_PWN,	.flags = GPIOF_HIGH},
	{.label = "ov5640_mipi_reset",	.gpio = GP_MIPI_RST,	.flags = 0},
	{.label = "ov5640_mipi_reset2",	.gpio = GP_MIPI_RST2,	.flags = 0},
	{.label = "ov5640_csi1_pwdn",	.gpio = GP_CSI1_PWN,	.flags = GPIOF_HIGH},
	{.label = "ov5640_csi1_reset",	.gpio = GP_CSI1_RST,	.flags = 0},
#if defined(CONFIG_MXC_VIDEO_GS2971) || defined(CONFIG_MXC_VIDEO_GS2971_MODULE)
	{.label = "gs2971_csi1_pwdn",	.gpio = GP_GS2971_PWN,	.flags = GPIOF_HIGH},
	{.label = "gs2971_csi1_reset",	.gpio = GP_GS2971_RST,	.flags = 0},
#endif
};

void rcu_cpu_stall_reset(void);
static void poweroff(void)
{
	int i=0;
	pr_info("%s: %s\n", __FILE__, __func__);
	for (i=0; i < num_registered_fb; i++) {
		if (registered_fb[i])
                        fb_blank(registered_fb[i],FB_BLANK_POWERDOWN);
	}
	while (1) {
                touch_softlockup_watchdog_sync();
                touch_all_softlockup_watchdogs();
                rcu_cpu_stall_reset();
		__raw_writew(0x5555, IO_ADDRESS(MX6Q_WDOG2_BASE_ADDR+2));
		__raw_writew(0xaaaa, IO_ADDRESS(MX6Q_WDOG2_BASE_ADDR+2));
		__raw_writew(0x5555, IO_ADDRESS(MX6Q_WDOG1_BASE_ADDR+2));
		__raw_writew(0xaaaa, IO_ADDRESS(MX6Q_WDOG1_BASE_ADDR+2));
		mdelay(100);
		if (0 == (i &127)) {
			pr_info("%s\n", __func__);
		}
		i++;
	}
}

/*!
 * Board specific initialization.
 */
static void __init board_init(void)
{
	int i, j;
	int ret;
	struct clk *clko2;
	struct clk *new_parent;
	int rate;
	int isn6 ;
	unsigned mask;
	struct platform_device *voutdev;

	ret = gpio_request_array(initial_gpios, ARRAY_SIZE(initial_gpios));
	if (ret)
		printk(KERN_ERR "%s gpio_request_array failed("
			"%d) for initial_gpios\n", __func__, ret);
	IOMUX_SETUP(common_pads);
	lcd_disable_pins();

#if defined(CONFIG_MXC_VIDEO_GS2971) || defined(CONFIG_MXC_VIDEO_GS2971_MODULE)
	IOMUX_SETUP(ecspi3_pads);
#endif

	isn6 = is_nitrogen6w();
	if (isn6) {
		audio_data.ext_port = 3;
		sd3_data.wp_gpio = -1 ;
		IOMUX_SETUP(nitrogen6x_pads);
	} else {
		IOMUX_SETUP(sabrelite_pads);
	}
	printk(KERN_ERR "------------ Board type %s\n",
               isn6 ? "Nitrogen6X/W" : "Sabre Lite");

#ifdef CONFIG_FEC_1588
	/* Set GPIO_16 input for IEEE-1588 ts_clk and RMII reference clock
	 * For MX6 GPR1 bit21 meaning:
	 * Bit21:       0 - GPIO_16 pad output
	 *              1 - GPIO_16 pad input
	 */
	mxc_iomux_set_gpr_register(1, 21, 1, 1);
#endif

	gp_reg_id = dvfscore_data.reg_id;
	soc_reg_id = dvfscore_data.soc_id;
	pu_reg_id = dvfscore_data.pu_id;

	imx6q_add_imx_uart(0, NULL);
	imx6q_add_imx_uart(1, NULL);
	if (isn6)
		imx6q_add_imx_uart(2, &mx6_arm2_uart2_data);

	if (!cpu_is_mx6q()) {
		ldb_data.ipu_id = 0;
		ldb_data.sec_ipu_id = 0;
	}
	imx6q_add_mxc_hdmi_core(&hdmi_core_data);

	imx6q_add_ipuv3(0, &ipu_data[0]);
	if (cpu_is_mx6q()) {
		imx6q_add_ipuv3(1, &ipu_data[1]);
		j = ARRAY_SIZE(fb_data);
	} else {
		j = (ARRAY_SIZE(fb_data) + 1) / 2;
		adv7180_data.ipu = 0;
	}
	for (i = 0; i < j; i++)
		imx6q_add_ipuv3fb(i, &fb_data[i]);

	imx6q_add_vdoa();
#if ! defined(CONFIG_MXC_VIDEO_GS2971) && ! defined(CONFIG_MXC_VIDEO_GS2971_MODULE) /* We need the pads for GS2971 */
	imx6q_add_lcdif(&lcdif_data);
#endif
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

	mask = 0;
	for (i = 0; i < ARRAY_SIZE(capture_data); i++) {
		if (!cpu_is_mx6q())
			capture_data[i].ipu = 0;
		j = (capture_data[i].is_mipi << 2) | (capture_data[i].ipu << 1) | capture_data[i].csi;
		if (!(mask & (1 << j))) {
			mask |= (1 << j);
			imx6q_add_v4l2_capture(i, &capture_data[i]);
		}
	}
	if (!cpu_is_mx6q()) {
		mipi_csi2_pdata.ipu_id = 0;

#ifdef MIPI_CAMERA
		ov5640_mipi_data.ipu = 0;
#endif

#ifdef CSI1_CAMERA
		ov5640_csi1_data.ipu = 0;
#ifdef MIPI_CAMERA
		ov5640_mipi_data.csi = 0;
		mipi_csi2_pdata.csi_id = 0;
#endif
#endif
	}

	imx6q_add_mipi_csi2(&mipi_csi2_pdata);
	imx6q_add_imx_snvs_rtc();

	imx6q_add_imx_caam();

	imx6q_add_imx_i2c(0, &i2c_data);
	imx6q_add_imx_i2c(1, &i2c_data);
	imx6q_add_imx_i2c(2, &i2c_data);
	/*
	 * SABRE Lite does not have an ISL1208 RTC
	 */
	i2c_register_board_info(0, mxc_i2c0_board_info,
			isn6    ? ARRAY_SIZE(mxc_i2c0_board_info)
				: ARRAY_SIZE(mxc_i2c0_board_info)-1);
	i2c_register_board_info(1, mxc_i2c1_board_info,
			ARRAY_SIZE(mxc_i2c1_board_info));
	i2c_register_board_info(2, mxc_i2c2_board_info,
			ARRAY_SIZE(mxc_i2c2_board_info));

	/* SPI */
	imx6q_add_ecspi(0, &spi_data);
#if defined(CONFIG_MXC_VIDEO_GS2971) || defined(CONFIG_MXC_VIDEO_GS2971_MODULE)
	imx6q_add_ecspi(2, &spi3_data);
#endif
	spi_device_init();

	imx6q_add_mxc_hdmi(&hdmi_data);

	imx6q_add_anatop_thermal_imx(1, &anatop_thermal_data);
	imx6_init_fec(fec_data);
	imx6q_add_pm_imx(0, &pm_data);
	imx6q_add_sdhci_usdhc_imx(3, &sd4_data);
	imx6q_add_sdhci_usdhc_imx(2, &sd3_data);
	imx_add_viv_gpu(&imx6_gpu_data, &imx6_gpu_pdata);
	init_usb();
	if (cpu_is_mx6q())
		imx6q_add_ahci(0, &sata_data);
	imx6q_add_vpu();
	imx6_init_audio();
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
	imx6q_add_mxc_pwm_backlight(0, &pwm0_backlight_data);
	imx6q_add_mxc_pwm_backlight(3, &pwm_backlight_data);

	imx6q_add_otp();
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);
	imx6q_add_dma();

	imx6q_add_dvfs_core(&dvfscore_data);
	imx6q_add_ion(0, &imx_ion_data,
		sizeof(imx_ion_data) + sizeof(struct ion_platform_heap));

	add_device_buttons();

	imx6q_add_hdmi_soc();
	imx6q_add_hdmi_soc_dai();

	ret = gpio_request_array(flexcan_gpios,
			ARRAY_SIZE(flexcan_gpios));
	if (ret) {
		pr_err("failed to request flexcan1-gpios: %d\n", ret);
	} else {
		int ret = gpio_get_value(GP_CAN1_ERR);
		if (ret == 0) {
			imx6q_add_flexcan0(&flexcan0_tja1040_pdata);
			pr_info("Flexcan NXP tja1040\n");
		} else if (ret == 1) {
			IOMUX_SETUP(mc33902_flexcan_pads);
			imx6q_add_flexcan0(&flexcan0_mc33902_pdata);
			pr_info("Flexcan Freescale mc33902\n");
		} else {
			pr_info("Flexcan gpio_get_value CAN1_ERR failed\n");
		}
	}

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

#ifdef CONFIG_WL12XX_PLATFORM_DATA
	if (isn6) {
		imx6q_add_sdhci_usdhc_imx(1, &sd2_data);
		/* WL12xx WLAN Init */
		if (wl12xx_set_platform_data(&n6q_wlan_data))
			pr_err("error setting wl12xx data\n");

		gpio_set_value(N6_WL1271_WL_EN, 1);		/* momentarily enable */
		gpio_set_value(N6_WL1271_BT_EN, 1);
		mdelay(2);
		/* gpio_set_value(N6_WL1271_WL_EN, 0);		leave enabled for enumeration */
		gpio_set_value(N6_WL1271_BT_EN, 0);

		gpio_free(N6_WL1271_WL_EN);
		gpio_free(N6_WL1271_BT_EN);
		mdelay(1);
	}
#endif

	gpio_export(N6_WL1271_WL_EN,1);
	gpio_export(N6_WL1271_BT_EN,1);

	imx6q_add_pcie(&pcie_data);
#ifdef CONFIG_TI_ST
	if (isn6) {
		platform_device_register (&wl127x_bt_device);
		platform_device_register (&btwilink_device);
	}
#endif
	imx6_add_armpmu();
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
	phys_addr_t phys;
	int i;
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

	for (i = 0; i < ARRAY_SIZE(fb_data); i++)
		if (fb_data[i].res_size[0]) {
			/* reserve for background buffer */
			phys = memblock_alloc(fb_data[i].res_size[0],
						SZ_4K);
			memblock_remove(phys, fb_data[i].res_size[0]);
			fb_data[i].res_base[0] = phys;
		}
	if (vout_mem.res_msize) {
		phys = memblock_alloc_base(vout_mem.res_msize,
					   SZ_4K, SZ_1G);
		memblock_remove(phys, vout_mem.res_msize);
		vout_mem.res_mbase = phys;
	}
}

/*
 * initialize data structure.
 */
MACHINE_START(MX6_NITROGEN6X, "Boundary Devices Nitrogen6X/SABRE Lite Board")
	/* Maintainer: Boundary Devices */
	.boot_params = MX6_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mx6_map_io,
	.init_irq = mx6_init_irq,
	.init_machine = board_init,
	.timer = &timer,
	.reserve = reserve,
MACHINE_END
