/*
 * include/linux/amlogic/amlsd.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef AMLSD_H
#define AMLSD_H
#include <linux/of_gpio.h>

/* ptm or pxp simulation */
/* #define CONFIG_MESON_CPU_EMULATOR */
/* #define SD_EMMC_DEBUG_BOARD */
/* hardcode clock, for debug or bringup */
/* #define SD_EMMC_CLK_CTRL	(1) */
/* #define SD_EMMC_PIN_CTRL	(1) */

#define AML_MMC_MAJOR_VERSION   3
#define AML_MMC_MINOR_VERSION   02
#define AML_MMC_VERSION \
	((AML_MMC_MAJOR_VERSION << 8) | AML_MMC_MINOR_VERSION)
#define AML_MMC_VER_MESSAGE \
	"2017-05-15: New Emmc Host Controller"

extern unsigned long sdhc_debug;
extern unsigned long sdio_debug;
extern unsigned int sd_emmc_debug;
extern const u8 tuning_blk_pattern_4bit[64];
extern const u8 tuning_blk_pattern_8bit[128];

#define DEBUG_SD_OF		0
#define MODULE_NAME		"amlsd"
/* #define CARD_DETECT_IRQ    1 */
#define AML_MMC_TDMA 1
#define SD_EMMC_DEBUG_BOARD 1

#if 0
#define A0_GP_CFG0			(0xc8100240)
#define A0_GP_CFG2			(0xc8100248)
#define STORAGE_DEV_NOSET	(0)
#define STORAGE_DEV_EMMC	(1)
#define STORAGE_DEV_NAND	(2)
#define STORAGE_DEV_SPI		(3)
#define STORAGE_DEV_SDCARD	(4)
#define STORAGE_DEV_USB		(5)
#define LDO4DAC_REG_ADDR        0x4f
#define LDO4DAC_REG_1_8_V       0x24
#define LDO4DAC_REG_2_8_V       0x4c
#define LDO4DAC_REG_3_3_V       0x60
#endif

#define     DETECT_CARD_IN          1
#define     DETECT_CARD_OUT         2
#define     DETECT_CARD_JTAG_IN     3
#define     DETECT_CARD_JTAG_OUT    4

#define EMMC_DAT3_PINMUX_CLR    0
#define EMMC_DAT3_PINMUX_SET    1

#ifdef CONFIG_AMLOGIC_M8B_MMC
#define P_PERIPHS_PIN_MUX_2 (0xc1100000 + (0x202e << 2))
#define P_PREG_PAD_GPIO3_EN_N (0xc1100000 + (0x2015 << 2))
#define P_PREG_PAD_GPIO3_O (0xc1100000 + (0x2016 << 2))
#endif

#define CHECK_RET(ret) { \
	if (ret) \
	pr_info("[%s] gpio op failed(%d) at line %d\n",\
			__func__, ret, __LINE__); \
}

#define SD_PARSE_U32_PROP_HEX(node, prop_name, prop, value) do { \
	if (!of_property_read_u32(node, prop_name, &prop)) {\
		value = prop;\
		prop = 0;\
		if (DEBUG_SD_OF) {	\
			pr_info("get property:%25s, value:0x%08x\n", \
					prop_name, (unsigned int)value); \
		} \
	} \
} while (0)

#define SD_PARSE_U32_PROP_DEC(node, prop_name, prop, value) do { \
	if (!of_property_read_u32(node, prop_name, &prop)) {\
		value = prop;\
		prop = 0;\
		if (DEBUG_SD_OF) { \
			pr_info("get property:%25s, value:%d\n", \
					prop_name, (unsigned int)value); \
		} \
	} \
} while (0)

#define SD_PARSE_GPIO_NUM_PROP(node, prop_name, str, gpio_pin) {\
	if (!of_property_read_string(node, prop_name, &str)) {\
		gpio_pin = \
		of_get_named_gpio(node, \
				prop_name, 0);\
		if (DEBUG_SD_OF) {	\
			pr_info("get property:%25s, str:%s\n",\
					prop_name, str);\
		} \
	} \
}

#define SD_PARSE_STRING_PROP(node, prop_name, str, prop) {\
	if (!of_property_read_string(node, prop_name, &str)) {\
		strcpy(prop, str);\
		if (DEBUG_SD_OF) {\
			pr_info("get property:%25s, str:%s\n",\
					prop_name, prop);	\
		} \
	} \
}

#define SD_CAPS(a, b) { .caps = a, .name = b }

#define WAIT_UNTIL_REQ_DONE msecs_to_jiffies(10000)

struct sd_caps {
	unsigned int caps;
	const char *name;
};

void aml_mmc_ver_msg_show(void);
extern int sdio_reset_comm(struct mmc_card *card);
#if 0
extern int storage_flag;
extern void aml_debug_print_buf(char *buf, int size);
extern int aml_buf_verify(int *buf, int blocks, int lba);
extern void aml_sdhc_init_debugfs(struct mmc_host *mmc);
extern void aml_sdio_init_debugfs(struct mmc_host *mmc);
extern void aml_sd_emmc_init_debugfs(struct mmc_host *mmc);
extern void aml_sdio_print_reg(struct amlsd_host *host);
extern void aml_sd_emmc_print_reg(struct amlsd_host *host);
extern int add_part_table(struct mtd_partition *part, unsigned int nr_part);
extern int add_emmc_partition(struct gendisk *disk);
int amlsd_get_reg_base(struct platform_device *pdev,
		struct amlsd_host *host);

/* int of_amlsd_detect(struct amlsd_platform* pdata); */

int aml_sd_uart_detect(struct amlsd_platform *pdata);
void aml_sd_uart_detect_clr(struct amlsd_platform *pdata);
void aml_sduart_pre(struct amlsd_platform *pdata);

/* is eMMC/tSD exist */
bool is_emmc_exist(struct amlsd_host *host);
void aml_devm_pinctrl_put(struct amlsd_host *host);
/* void of_init_pins (struct amlsd_platform* pdata); */

#ifdef CONFIG_MMC_AML_DEBUG
void aml_dbg_verify_pull_up(struct amlsd_platform *pdata);
int aml_dbg_verify_pinmux(struct amlsd_platform *pdata);
#endif
#endif

#ifdef CONFIG_AMLOGIC_M8B_MMC
void aml_sdhc_print_reg_(u32 *buf);
extern void aml_sdhc_print_reg(struct amlsd_host *host);
void aml_dbg_print_pinmux(void);
extern size_t aml_sg_copy_buffer(struct scatterlist *sgl, unsigned int nents,
		void *buf, size_t buflen, int to_buffer);
#endif

#ifdef CARD_DETECT_IRQ
void of_amlsd_irq_init(struct amlsd_platform *pdata);
irqreturn_t aml_sd_irq_cd(int irq, void *dev_id);
irqreturn_t aml_irq_cd_thread(int irq, void *data);
#else
void meson_mmc_cd_detect(struct work_struct *work);
#endif

int amlsd_get_platform_data(struct platform_device *pdev,
		struct amlsd_platform *pdata,
		struct mmc_host *mmc, u32 index);
int of_amlsd_init(struct amlsd_platform *pdata);
void of_amlsd_pwr_prepare(struct amlsd_platform *pdata);
void of_amlsd_pwr_on(struct amlsd_platform *pdata);
void of_amlsd_pwr_off(struct amlsd_platform *pdata);
void of_amlsd_xfer_pre(struct amlsd_platform *pdata);
void of_amlsd_xfer_post(struct amlsd_platform *pdata);
/* chip select high */
void aml_cs_high(struct mmc_host *mmc);
/* chip select don't care */
void aml_cs_dont_care(struct mmc_host *mmc);
void aml_snprint (char **pp, int *left_size,  const char *fmt, ...);
int of_amlsd_ro(struct amlsd_platform *pdata);
int aml_sd_voltage_switch(struct mmc_host *mmc, char signal_voltage);
int aml_signal_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios);
int aml_check_unsupport_cmd(struct mmc_host *mmc, struct mmc_request *mrq);
extern void aml_emmc_hw_reset(struct mmc_host *mmc);
#endif


