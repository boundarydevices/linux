/*
 * drivers/amlogic/mmc/amlsd.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/card.h>
#include <linux/slab.h>
#include <linux/amlogic/sd.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/jtag.h>
#include <linux/highmem.h>
#include <linux/of.h>
#include <linux/arm-smccc.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/amlsd.h>
#ifdef CONFIG_AMLOGIC_M8B_MMC
#include <linux/amlogic/gpio-amlogic.h>
#endif

const u8 tuning_blk_pattern_4bit[64] = {
	0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
	0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
	0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
	0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
	0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
	0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
	0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
	0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};
const u8 tuning_blk_pattern_8bit[128] = {
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
	0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
	0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
	0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
	0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
	0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
	0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
	0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
	0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
	0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
	0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
	0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
	0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

void aml_mmc_ver_msg_show(void)
{
	static bool one_time_flag;

	if (!one_time_flag) {
		pr_info("mmc driver version: %d.%02d, %s\n",
			AML_MMC_MAJOR_VERSION, AML_MMC_MINOR_VERSION,
				AML_MMC_VER_MESSAGE);

	one_time_flag = true;
	}
}

static int aml_cmd_invalid(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	unsigned long flags;

	spin_lock_irqsave(&host->mrq_lock, flags);
	mrq->cmd->error = -EINVAL;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
	mmc_request_done(mmc, mrq);

	return -EINVAL;
}

#if 0
static int aml_rpmb_cmd_invalid(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	unsigned long flags;

	spin_lock_irqsave(&host->mrq_lock, flags);
	host->xfer_step = XFER_FINISHED;
	host->mrq = NULL;
	host->status = HOST_INVALID;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
	mrq->data->bytes_xfered = mrq->data->blksz*mrq->data->blocks;
	mmc_request_done(mmc, mrq);
	return -EINVAL;
}
#endif

int aml_check_unsupport_cmd(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	u32 opcode, arg;

	opcode = mrq->cmd->opcode;
	arg = mrq->cmd->arg;
	/* CMD3 means the first time initialized flow is running */
	if (opcode == 3)
		mmc->first_init_flag = false;

	if (mmc->caps & MMC_CAP_NONREMOVABLE) { /* nonremovable device */
		if (mmc->first_init_flag) { /* init for the first time */
			/* for 8189ETV needs ssdio reset when starts */
			if (aml_card_type_sdio(pdata)) {
				/* if (opcode == SD_IO_RW_DIRECT
				 * || opcode == SD_IO_RW_EXTENDED
				 * || opcode == SD_SEND_IF_COND)
				 * return aml_cmd_invalid(mmc, mrq);
				 */
				return 0;
			} else if (aml_card_type_mmc(pdata)) {
				if (opcode == SD_IO_SEND_OP_COND
					|| opcode == SD_IO_RW_DIRECT
					|| opcode == SD_IO_RW_EXTENDED
					|| opcode == SD_SEND_IF_COND
					|| opcode == MMC_APP_CMD)
					return aml_cmd_invalid(mmc, mrq);
			} else if (aml_card_type_sd(pdata)
					|| aml_card_type_non_sdio(pdata)) {
				if (opcode == SD_IO_SEND_OP_COND
					|| opcode == SD_IO_RW_DIRECT
					|| opcode == SD_IO_RW_EXTENDED)
					return aml_cmd_invalid(mmc, mrq);
			}
		}
	} else { /* removable device */
		/* filter cmd 5/52/53 for a non-sdio device */
		if (!aml_card_type_sdio(pdata)
			&& !aml_card_type_unknown(pdata)) {
			if (opcode == SD_IO_SEND_OP_COND
				|| opcode == SD_IO_RW_DIRECT
				|| opcode == SD_IO_RW_EXTENDED)
				return aml_cmd_invalid(mmc, mrq);
		}
	}
	return 0;
}

int aml_sd_voltage_switch(struct mmc_host *mmc, char signal_voltage)
{
#ifndef CONFIG_AMLOGIC_M8B_MMC
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	int ret = 0;

	/* voltage is the same, return directly */
	if (!aml_card_type_non_sdio(pdata)
		|| (pdata->signal_voltage == signal_voltage)) {
		if (aml_card_type_sdio(pdata))
			host->sd_sdio_switch_volat_done = 1;
		return 0;
	}
	if (pdata->vol_switch) {
		if (pdata->signal_voltage == 0xff) {
			gpio_free(pdata->vol_switch);
			ret = gpio_request_one(pdata->vol_switch,
					GPIOF_OUT_INIT_HIGH, MODULE_NAME);
			if (ret) {
				pr_err("%s [%d] request error\n",
						__func__, __LINE__);
				return -EINVAL;
			}
		}
		if (signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			ret = gpio_direction_output(pdata->vol_switch,
						pdata->vol_switch_18);
		else
			ret = gpio_direction_output(pdata->vol_switch,
					(!pdata->vol_switch_18));
		CHECK_RET(ret);
		if (!ret)
			pdata->signal_voltage = signal_voltage;
	} else
		return -EINVAL;

	host->sd_sdio_switch_volat_done = 1;
#endif
	return 0;
}

/* boot9 here */
void aml_emmc_hw_reset(struct mmc_host *mmc)
{
#ifdef CONFIG_AMLOGIC_M8B_MMC
	struct amlsd_platform *pdata = mmc_priv(mmc);
	void __iomem *hw_ctrl;

	if (!aml_card_type_mmc(pdata))
		return;
	hw_ctrl = ioremap(P_PREG_PAD_GPIO3_EN_N, 0x200);

	//boot_9 used as eMMC hw_rst pin here.

	//clr nand ce1 pinmux
	aml_clr_reg32_mask((hw_ctrl + (0x19 << 2)), (1<<24));

	//set out
	aml_clr_reg32_mask(hw_ctrl, (1<<9));

	//high
	aml_set_reg32_mask((hw_ctrl + (0x1 << 2)), (1<<9));
	mdelay(1);

	//low
	aml_clr_reg32_mask((hw_ctrl + (0x1 << 2)), (1<<9));
	mdelay(2);

	//high
	aml_set_reg32_mask((hw_ctrl + (0x1 << 2)), (1<<9));
	mdelay(1);
#else
	struct amlsd_platform *pdata = mmc_priv(mmc);
	u32 ret;

	if (!aml_card_type_mmc(pdata) || !pdata->hw_reset)
		return;

	/* boot_9 used as eMMC hw_rst pin here. */
	gpio_free(pdata->hw_reset);
	ret = gpio_request_one(pdata->hw_reset,
			GPIOF_OUT_INIT_HIGH, MODULE_NAME);
	CHECK_RET(ret);
	if (ret) {
		pr_err("%s [%d] request error\n",
				__func__, __LINE__);
		return;
	}
	ret = gpio_direction_output(pdata->hw_reset, 0);
	CHECK_RET(ret);
	if (ret) {
		pr_err("%s [%d] output high error\n",
			__func__, __LINE__);
		return;
	}
	mdelay(2);
	ret = gpio_direction_output(pdata->hw_reset, 1);
	CHECK_RET(ret);
	if (ret) {
		pr_err("%s [%d] output high error\n",
			__func__, __LINE__);
		return;
	}
	mdelay(2);
#endif
}

static void sdio_rescan(struct mmc_host *mmc)
{
	int ret;

	mmc->rescan_entered = 0;
/*	mmc->host_rescan_disable = false;*/
	mmc_detect_change(mmc, 0);
	/* start the delayed_work */
	ret = flush_work(&(mmc->detect.work));
	/* wait for the delayed_work to finish */
	if (!ret)
		pr_info("Error: delayed_work mmc_rescan() already idle!\n");
}

void sdio_reinit(void)
{
	if (sdio_host) {
		if (sdio_host->card)
			sdio_reset_comm(sdio_host->card);
		else
			sdio_rescan(sdio_host);
	} else {
		pr_info("Error: sdio_host is NULL\n");
	}

	pr_info("[%s] finish\n", __func__);
}
EXPORT_SYMBOL(sdio_reinit);

void of_amlsd_pwr_prepare(struct amlsd_platform *pdata)
{
}

void of_amlsd_pwr_on(struct amlsd_platform *pdata)
{
	if (pdata->gpio_power)
		gpio_set_value(pdata->gpio_power, pdata->power_level);
}

void of_amlsd_pwr_off(struct amlsd_platform *pdata)
{
	if (pdata->gpio_power)
		gpio_set_value(pdata->gpio_power, !pdata->power_level);
}

#ifdef CARD_DETECT_IRQ
void of_amlsd_irq_init(struct amlsd_platform *pdata)
{
	if (aml_card_type_non_sdio(pdata))
		pdata->irq_cd = gpio_to_irq(pdata->gpio_cd);
	pr_info("sd irq num = %d\n", pdata->irq_cd);
}
#endif

int of_amlsd_init(struct amlsd_platform *pdata)
{
	int ret;

	WARN_ON(!pdata);

	if (pdata->gpio_cd) {
		pr_info("gpio_cd = %x\n", pdata->gpio_cd);
		ret = gpio_request_one(pdata->gpio_cd,
				GPIOF_IN, MODULE_NAME);
		CHECK_RET(ret);
	}
#if 0
	if (pdata->gpio_ro) {
		ret = amlogic_gpio_request_one(pdata->gpio_ro,
				GPIOF_IN, MODULE_NAME);
		if (!ret) { // ok
			/* 0:pull down, 1:pull up */
			ret = amlogic_set_pull_up_down(pdata->gpio_ro,
			  1, MODULE_NAME);
			CHECK_RET(ret);
		} else {
			pr_err("request gpio_ro pin fail!\n");
		}
	}
#endif
	if (pdata->gpio_power) {
		if (pdata->power_level) {
			ret = gpio_request_one(pdata->gpio_power,
					GPIOF_OUT_INIT_LOW, MODULE_NAME);
			CHECK_RET(ret);
		} else {
			ret = gpio_request_one(pdata->gpio_power,
					GPIOF_OUT_INIT_HIGH, MODULE_NAME);
			CHECK_RET(ret);
		}
	}

	/* if(pdata->port == MESON_SDIO_PORT_A) */
	/* wifi_setup_dt(); */
	return 0;
}

void aml_devm_pinctrl_put(struct amlsd_host *host)
{
	if (host->pinctrl) {
		devm_pinctrl_put(host->pinctrl);
		host->pinctrl = NULL;

		host->pinctrl_name[0] = '\0';
		/* pr_err("Put Pinctrl\n"); */
	}
}

#ifndef SD_EMMC_PIN_CTRL
static struct pinctrl * __must_check aml_devm_pinctrl_get_select(
				struct amlsd_host *host, const char *name)
{
	struct pinctrl *p = host->pinctrl;
	struct pinctrl_state *s;
	int ret;

	if (!p) {
		p = devm_pinctrl_get(&host->pdev->dev);

		if (IS_ERR(p))
			return p;

		host->pinctrl = p;
		/* pr_err("switch %s\n", name); */
	}

	s = pinctrl_lookup_state(p, name);
	if (IS_ERR(s)) {
		pr_err("lookup %s fail\n", name);
		devm_pinctrl_put(p);
		return ERR_CAST(s);
	}

	ret = pinctrl_select_state(p, s);
	if (ret < 0) {
		pr_err("select %s fail\n", name);
		devm_pinctrl_put(p);
		return ERR_PTR(ret);
	}
	if ((host->mem->start == host->data->port_b_base)
			&& (host->data->chip_type == MMC_CHIP_G12A))
		strlcpy(host->pinctrl_name, name, sizeof(host->pinctrl_name));
	return p;
}
#else /* SD_EMMC_PIN_CTRL */
static struct pinctrl * __must_check aml_devm_pinctrl_get_select(
				struct amlsd_host *host, const char *name)
{
	u32 val;

	if (!strcmp("sd_clk_cmd_pins", name)) {
		val = readl(host->pinmux_base + PIN_MUX_REG9);
		val &= ~0xFFFFFF;
		val |= 0x110000;
		writel(val, host->pinmux_base + PIN_MUX_REG9);
		/* pullup status */
		pr_info("name %s -> pinmux 9 0x%x\n", name, val);

	} else if (!strcmp("sd_all_pins", name)) {
		val = readl(host->pinmux_base + PIN_MUX_REG9);
		val &= ~0xFFFFFF;
		val |= 0x111111;
		writel(val, host->pinmux_base + PIN_MUX_REG9);
		/* pullup status */

		pr_info("name %s -> pinmux 9 0x%x\n", name, val);
	} else
		pr_err("E: name %s do nothing.\n", name);

	return NULL;
}
#endif /* SD_EMMC_PIN_CTRL */

void of_amlsd_xfer_pre(struct amlsd_platform *pdata)
{
	struct amlsd_host *host = pdata->host;
	struct mmc_host *mmc = pdata->mmc;
	char pinctrl[30];
	char *p = pinctrl;
	int i, size = 0;
	struct pinctrl *ppin;

	size = sizeof(pinctrl);
#ifdef CONFIG_AMLOGIC_M8B_MMC
	if (pdata->port > PORT_SDIO_C) // so it should be PORT_SDHC_X
		aml_snprint(&p, &size, "sdhc_");
#endif

	if (mmc->ios.chip_select == MMC_CS_DONTCARE) {
		if ((mmc->caps & MMC_CAP_4_BIT_DATA)
		|| (strcmp(pdata->pinname, "sd"))
		|| (mmc->caps & MMC_CAP_8_BIT_DATA)) {
			if (aml_card_type_sdio(pdata)
				&& (host->data->chip_type == MMC_CHIP_G12A)
				&& host->is_sduart)
				aml_snprint(&p, &size,
					"%s_noclr_all_pins", pdata->pinname);
			else
				aml_snprint(&p, &size,
						"%s_all_pins", pdata->pinname);
		} else {
			if (pdata->is_sduart && (!strcmp(pdata->pinname, "sd")))
				aml_snprint(&p, &size,
						"%s_1bit_uart_pins",
						pdata->pinname);
			else
				aml_snprint(&p, &size,
						"%s_1bit_pins", pdata->pinname);
		}
	} else { /* MMC_CS_HIGH */
		if (pdata->is_sduart && (!strcmp(pdata->pinname, "sd"))) {
			aml_snprint(&p, &size,
					"%s_clk_cmd_uart_pins", pdata->pinname);
		} else {
			if (aml_card_type_sdio(pdata)
				&& (host->data->chip_type == MMC_CHIP_G12A)
				&& host->is_sduart)
				aml_snprint(&p, &size,
						"%s_noclr_clk_cmd_pins",
						pdata->pinname);
			else
				aml_snprint(&p, &size,
					"%s_clk_cmd_pins", pdata->pinname);
		}
	}

	/* if pinmux setting is changed (pinctrl_name is different) */
	if (strncmp(host->pinctrl_name, pinctrl,
				sizeof(host->pinctrl_name))) {
		if (strlcpy(host->pinctrl_name, pinctrl,
					sizeof(host->pinctrl_name))
				>= sizeof(host->pinctrl_name)) {

			pr_err("Pinctrl name is too long!\n");
			return;
		}

		for (i = 0; i < 100; i++) {
			mutex_lock(&host->pinmux_lock);
			ppin = aml_devm_pinctrl_get_select(host, pinctrl);
			mutex_unlock(&host->pinmux_lock);
			/* bringup on ptm for g12 */
		#ifndef SD_EMMC_PIN_CTRL
			if (!IS_ERR(ppin)) {
				/* pdata->host->pinctrl = ppin; */
				break;
			}
		#else
			break;
		#endif
			/* else -> aml_irq_cdin_thread()
			 *should be using one of the GPIO of card,
			 * then we should wait here until the GPIO is free,
			 * otherwise something must be wrong.
			 */
			mdelay(1);
		}
		if (i == 100)
			pr_err("CMD%d: get pinctrl %s fail.\n",
					host->opcode, pinctrl);
	}
}

void of_amlsd_xfer_post(struct amlsd_platform *pdata)
{
}

int of_amlsd_ro(struct amlsd_platform *pdata)
{
	int ret = 0; /* 0--read&write, 1--read only */

	if (pdata->gpio_ro)
		ret = gpio_get_value(pdata->gpio_ro);
	/* pr_err("read-only?--%s\n", ret?"YES":"NO"); */
	return ret;
}

void aml_snprint (char **pp, int *left_size,  const char *fmt, ...)
{
	va_list args;
	char *p = *pp;
	int size;

	if (*left_size <= 1) {
		pr_err("buf is full\n");
		return;
	}

	va_start(args, fmt);
	size = vsnprintf(p, *left_size, fmt, args);
	va_end(args);
	*pp += size;
	*left_size -= size;
}

void aml_cs_high(struct mmc_host *mmc) /* chip select high */
{
	int ret;
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;

	if ((mmc->ios.chip_select == MMC_CS_HIGH)
			&& (pdata->gpio_dat3 != 0)) {
		aml_devm_pinctrl_put(host);
		ret = gpio_request_one(pdata->gpio_dat3,
				GPIOF_OUT_INIT_HIGH, MODULE_NAME);
		CHECK_RET(ret);
		if (ret == 0) {
			ret = gpio_direction_output(pdata->gpio_dat3, 1);
			CHECK_RET(ret);
		}
		gpio_free(pdata->gpio_dat3);
	}
}

void aml_cs_dont_care(struct mmc_host *mmc)
{
#if 0
	struct amlsd_platform *pdata = mmc_priv(mmc);

	if ((mmc->ios.chip_select == MMC_CS_DONTCARE)
			&& (pdata->gpio_dat3 != 0)
			&& (gpio_get_value(pdata->gpio_dat3) >= 0))
		gpio_free(pdata->gpio_dat3);
#endif
}

static int aml_is_card_insert(struct amlsd_platform *pdata)
{
	int ret = 0, in_count = 0, out_count = 0, i;

	if (pdata->gpio_cd) {
		mdelay(pdata->card_in_delay);
		for (i = 0; i < 200; i++) {
			ret = gpio_get_value(pdata->gpio_cd);
			if (ret)
				out_count++;
			in_count++;
			if ((out_count > 100) || (in_count > 100))
				break;
		}
		if (out_count > 100)
			ret = 1;
		else if (in_count > 100)
			ret = 0;
	}
	pr_err("card %s\n", ret?"OUT":"IN");
	if (!pdata->gpio_cd_level)
		ret = !ret; /* reverse, so ---- 0: no inserted  1: inserted */

	return ret;
}

#ifndef CONFIG_AMLOGIC_M8B_MMC
static int aml_is_sdjtag(struct amlsd_platform *pdata)
{
	int in = 0, i;
	int high_cnt = 0, low_cnt = 0;
	u32 vstat = 0;
	struct pinctrl *pc;
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_status *ista = (struct sd_emmc_status *)&vstat;

	mutex_lock(&host->pinmux_lock);
	pc = aml_devm_pinctrl_get_select(host, "sd_to_ao_uart_pins");

	for (i = 0; ; i++) {
		mdelay(1);
		vstat = readl(host->base + SD_EMMC_STATUS) & 0xffffffff;
		if (ista->dat_i & 0x2) {
			high_cnt++;
			low_cnt = 0;
		} else {
			low_cnt++;
			high_cnt = 0;
		}
		if ((high_cnt > 50) || (low_cnt > 50))
			break;
	}

	if (low_cnt > 50)
		in = 1;
	mutex_unlock(&host->pinmux_lock);
	return !in;
}

static int aml_is_sduart(struct amlsd_platform *pdata)
{
#ifndef SD_EMMC_DEBUG_BOARD
	return 0;
#else
	int in = 0, i;
	int high_cnt = 0, low_cnt = 0;
	struct pinctrl *pc;
	u32 vstat = 0;
	struct amlsd_host *host = pdata->host;
	struct sd_emmc_status *ista = (struct sd_emmc_status *)&vstat;

	mutex_lock(&host->pinmux_lock);
	pc = aml_devm_pinctrl_get_select(host, "sd_to_ao_uart_pins");

	mdelay(1);
	for (i = 0; ; i++) {
		mdelay(1);
		vstat = readl(host->base + SD_EMMC_STATUS) & 0xffffffff;
		if (ista->dat_i & 0x8) {
			high_cnt++;
			low_cnt = 0;
		} else {
			low_cnt++;
			high_cnt = 0;
		}
		if ((high_cnt > 100) || (low_cnt > 100))
			break;
	}
	if (low_cnt > 100)
		in = 1;
	mutex_unlock(&host->pinmux_lock);
	return in;
#endif
}

/* int n=0; */
static int aml_uart_switch(struct amlsd_platform *pdata, bool on)
{
#ifndef SD_EMMC_DEBUG_BOARD
	return on;
#else
	struct pinctrl *pc;
	char *name[2] = {
		"sd_to_ao_uart_pins",
		"ao_to_sd_uart_pins",
	};
	struct amlsd_host *host = pdata->host;

	pdata->is_sduart = on;
	mutex_lock(&host->pinmux_lock);
	pc = aml_devm_pinctrl_get_select(host, name[on]);
	mutex_unlock(&host->pinmux_lock);
#endif
	return on;
}

/* clear detect information */
void aml_sd_uart_detect_clr(struct amlsd_platform *pdata)
{
	pdata->is_sduart = 0;
	pdata->is_in = 0;
}

/*
 * setup jtag on/off, and setup ao/ee jtag
 *
 * @state: must be JTAG_STATE_ON/JTAG_STATE_OFF
 * @select: mest be JTAG_DISABLE/JTAG_A53_AO/JTAG_A53_EE
 */
#ifdef CONFIG_ARM64
void jtag_set_state(unsigned int state, unsigned int select)
{
	unsigned int command;
	struct arm_smccc_res res;

	if (state == AMLOGIC_JTAG_STATE_ON)
		command = AMLOGIC_JTAG_ON;
	else
		command = AMLOGIC_JTAG_OFF;
	asm __volatile__("" : : : "memory");

	arm_smccc_smc(command, select, 0, 0, 0, 0, 0, 0, &res);
}

void jtag_select_ao(void)
{
	set_cpus_allowed_ptr(current, cpumask_of(0));
	jtag_set_state(AMLOGIC_JTAG_STATE_ON, AMLOGIC_JTAG_APAO);
	set_cpus_allowed_ptr(current, cpu_all_mask);
}

void jtag_select_sd(void)
{
	set_cpus_allowed_ptr(current, cpumask_of(0));
	jtag_set_state(AMLOGIC_JTAG_STATE_ON, AMLOGIC_JTAG_APEE);
	set_cpus_allowed_ptr(current, cpu_all_mask);
}
#endif

static void aml_jtag_switch_sd(struct amlsd_platform *pdata)
{
	struct pinctrl *pc;
	int i;
	struct amlsd_host *host = pdata->host;

	for (i = 0; i < 100; i++) {
		mutex_lock(&host->pinmux_lock);
		pc = aml_devm_pinctrl_get_select(host,
			"ao_to_sd_jtag_pins");
		mutex_unlock(&host->pinmux_lock);
		if (!IS_ERR(pc))
			break;
		mdelay(1);
	}
	if (is_jtag_apee()) {
#ifdef CONFIG_ARM64
		jtag_select_sd();
#endif
		pr_info("setup apee\n");
	}
}

static void aml_jtag_switch_ao(struct amlsd_platform *pdata)
{
#ifndef SD_EMMC_DEBUG_BOARD

#else
	struct pinctrl *pc;
	int i;
	struct amlsd_host *host = pdata->host;

	for (i = 0; i < 100; i++) {
		mutex_lock(&host->pinmux_lock);
		pc = aml_devm_pinctrl_get_select(host,
			"sd_to_ao_jtag_pins");
		mutex_unlock(&host->pinmux_lock);
		if (!IS_ERR(pc))
			break;
		mdelay(1);
	}
#endif
}
#endif

#ifdef CONFIG_AMLOGIC_M8B_MMC
int aml_sd_uart_detect(struct amlsd_platform *pdata)
{
	struct mmc_host *mmc  = pdata->mmc;

	if (aml_is_card_insert(pdata)) {
		if (pdata->is_in)
			return 1;
		pdata->is_in = true;
		pdata->gpio_cd_sta = true;
		pr_info("normal card in\n");
		if (pdata->caps & MMC_CAP_4_BIT_DATA)
			mmc->caps |= MMC_CAP_4_BIT_DATA;
	} else {
		if (!pdata->is_in)
			return 1;
		pdata->is_in = false;
		pdata->gpio_cd_sta = false;
		pr_info("card out\n");

		pdata->is_tuned = false;
		if (mmc && mmc->card)
			mmc_card_set_removed(mmc->card);
		/* switch to 3.3V */
		aml_sd_voltage_switch(mmc,
				MMC_SIGNAL_VOLTAGE_330);

		if (pdata->caps & MMC_CAP_4_BIT_DATA)
			mmc->caps |= MMC_CAP_4_BIT_DATA;
	}
	return 0;
}
#else
int aml_sd_uart_detect(struct amlsd_platform *pdata)
{
	static bool is_jtag;
	struct mmc_host *mmc  = pdata->mmc;
	struct amlsd_host *host = pdata->host;

	if (aml_is_card_insert(pdata)) {
		if (pdata->is_in)
			return 1;
		pdata->is_in = true;
		pdata->gpio_cd_sta = true;
		if (aml_is_sduart(pdata)) {
			aml_uart_switch(pdata, 1);
			pr_info("Uart in\n");
			mmc->caps &= ~MMC_CAP_4_BIT_DATA;
			if (host->data->chip_type == MMC_CHIP_G12A)
				host->is_sduart = 1;
			if (aml_is_sdjtag(pdata)) {
				aml_jtag_switch_sd(pdata);
				is_jtag = true;
				pdata->is_in = false;
				pr_info("JTAG in\n");
				return 0;
			}
		} else {
			pr_info("normal card in\n");
			aml_uart_switch(pdata, 0);
			aml_jtag_switch_ao(pdata);
			if (host->data->chip_type == MMC_CHIP_G12A)
				host->is_sduart = 0;
			if (pdata->caps & MMC_CAP_4_BIT_DATA)
				mmc->caps |= MMC_CAP_4_BIT_DATA;
		}
	} else {
		if ((!pdata->is_in) && (pdata->is_sduart == false))
			return 1;
		pdata->is_in = false;
		pdata->gpio_cd_sta = false;
		if (is_jtag) {
			is_jtag = false;
			pr_info("JTAG OUT\n");
		} else
			pr_info("card out\n");

		pdata->is_tuned = false;
		if (host->data->chip_type == MMC_CHIP_G12A)
			host->is_sduart = 0;
		if (mmc && mmc->card)
			mmc_card_set_removed(mmc->card);
		aml_uart_switch(pdata, 0);
		aml_jtag_switch_ao(pdata);
		/* switch to 3.3V */
		aml_sd_voltage_switch(mmc,
				MMC_SIGNAL_VOLTAGE_330);

		if (pdata->caps & MMC_CAP_4_BIT_DATA)
			mmc->caps |= MMC_CAP_4_BIT_DATA;
	}
	return 0;
}
#endif

static int card_dealed;
#ifdef CARD_DETECT_IRQ
irqreturn_t aml_irq_cd_thread(int irq, void *data)
{
	struct amlsd_platform *pdata = (struct amlsd_platform *)data;
	struct mmc_host *mmc = pdata->mmc;
	struct amlsd_host *host = pdata->host;
	int ret = 0;

	mutex_lock(&pdata->in_out_lock);
	if (card_dealed == 1) {
		card_dealed = 0;
		mutex_unlock(&pdata->in_out_lock);
		return IRQ_HANDLED;
	}
	ret = aml_sd_uart_detect(pdata);
	if (ret == 1) {/* the same as the last*/
		mutex_unlock(&pdata->in_out_lock);
		return IRQ_HANDLED;
	}
	card_dealed = 1;
	if ((pdata->is_in == 0) && aml_card_type_non_sdio(pdata))
		host->init_flag = 0;
	mutex_unlock(&pdata->in_out_lock);
	/* mdelay(500); */
	if (pdata->is_in)
		mmc_detect_change(mmc, msecs_to_jiffies(100));
	else
		mmc_detect_change(mmc, msecs_to_jiffies(0));

	card_dealed = 0;
	return IRQ_HANDLED;
}

irqreturn_t aml_sd_irq_cd(int irq, void *dev_id)
{
	/* pr_info("cd dev_id %x\n", (unsigned)dev_id); */
	return IRQ_WAKE_THREAD;
}
#else
static int meson_cd_op(void *data)
{
	struct amlsd_platform *pdata = (struct amlsd_platform *)data;
	struct mmc_host *mmc = pdata->mmc;
	struct amlsd_host *host = pdata->host;
	int ret = 0;

#ifdef AML_MMC_TDMA
	if ((host->mem->start == host->data->port_b_base)
			&& (host->data->chip_type == MMC_CHIP_G12A))
		wait_for_completion(&host->drv_completion);
#endif
	mutex_lock(&pdata->in_out_lock);
	if (card_dealed == 1) {
		card_dealed = 0;
		mutex_unlock(&pdata->in_out_lock);
		return 0;
	}
	ret = aml_sd_uart_detect(pdata);
	if (ret == 1) {/* the same as the last*/
		mutex_unlock(&pdata->in_out_lock);
		return 0;
	}
	card_dealed = 1;
	if ((pdata->is_in == 0) && aml_card_type_non_sdio(pdata))
		host->init_flag = 0;
	mutex_unlock(&pdata->in_out_lock);
	/* mdelay(500); */
	if (pdata->is_in)
		mmc_detect_change(mmc, msecs_to_jiffies(100));
	else
		mmc_detect_change(mmc, msecs_to_jiffies(0));

	card_dealed = 0;
#ifdef AML_MMC_TDMA
	if ((host->mem->start == host->data->port_b_base)
			&& (host->data->chip_type == MMC_CHIP_G12A))
		complete(&host->drv_completion);
#endif
	return 0;
}

void meson_mmc_cd_detect(struct work_struct *work)
{
	struct amlsd_platform *pdata = container_of(
			work, struct amlsd_platform, cd_detect.work);
	int i = 0, ret = 0;

	for (i = 0; i < 5; i++) {
		ret = gpio_get_value(pdata->gpio_cd);
		if (pdata->gpio_cd_sta != ret)
			continue;
		meson_cd_op(pdata);
		mdelay(1);
	}
	schedule_delayed_work(&pdata->cd_detect, 50);
}
#endif

#ifdef CONFIG_AMLOGIC_M8B_MMC
/*-----------sg copy buffer------------*/
/**
 * aml_sg_miter_stop - stop mapping iteration for amlogic,
 * We don't disable irq in this function
 */
static void aml_sg_miter_stop(struct sg_mapping_iter *miter)
{
	WARN_ON(miter->consumed > miter->length);

	/* drop resources from the last iteration */
	if (miter->addr) {
		miter->__offset += miter->consumed;
		miter->__remaining -= miter->consumed;

		if (miter->__flags & SG_MITER_TO_SG)
			flush_kernel_dcache_page(miter->page);

		if (miter->__flags & SG_MITER_ATOMIC) {
			WARN_ON_ONCE(preemptible());
			kunmap_atomic(miter->addr);
		} else
			kunmap(miter->page);

		miter->page = NULL;
		miter->addr = NULL;
		miter->length = 0;
		miter->consumed = 0;
	}
}

/**
 * aml_sg_miter_next - proceed mapping iterator to the next mapping for amlogic,
 * We don't disable irq in this function
 */
static bool aml_sg_miter_next(struct sg_mapping_iter *miter)
{
	unsigned long flags;

	sg_miter_stop(miter);

	/*
	 * Get to the next page if necessary.
	 * __remaining, __offset is adjusted by sg_miter_stop
	 */
	if (!miter->__remaining) {
		struct scatterlist *sg;
		unsigned long pgoffset;

		if (!__sg_page_iter_next(&miter->piter))
			return false;

		sg = miter->piter.sg;
		pgoffset = miter->piter.sg_pgoffset;

		miter->__offset = pgoffset ? 0 : sg->offset;
		miter->__remaining = sg->offset + sg->length -
				(pgoffset << PAGE_SHIFT) - miter->__offset;
		miter->__remaining = min_t(unsigned long, miter->__remaining,
					   PAGE_SIZE - miter->__offset);
	}
	miter->page = sg_page_iter_page(&miter->piter);
	miter->consumed = miter->length = miter->__remaining;

	if (miter->__flags & SG_MITER_ATOMIC) {
		/*pr_info(KERN_DEBUG "AML_SDHC miter_next highmem\n"); */
		local_irq_save(flags);
		miter->addr = kmap_atomic(miter->page) + miter->__offset;
		local_irq_restore(flags);
	} else
		miter->addr = kmap(miter->page) + miter->__offset;
	return true;
}

/*
 * aml_sg_copy_buffer - Copy data between a linear buffer
 * and an SG list  for amlogic,
 * We don't disable irq in this function
 */
EXPORT_SYMBOL(aml_sg_copy_buffer);
size_t aml_sg_copy_buffer(struct scatterlist *sgl, unsigned int nents,
			     void *buf, size_t buflen, int to_buffer)
{
	unsigned int offset = 0;
	struct sg_mapping_iter miter;
	unsigned int sg_flags = SG_MITER_ATOMIC;
	unsigned long flags;

	if (to_buffer)
		sg_flags |= SG_MITER_FROM_SG;
	else
		sg_flags |= SG_MITER_TO_SG;

	sg_miter_start(&miter, sgl, nents, sg_flags);
	local_irq_save(flags);

	while (aml_sg_miter_next(&miter) && offset < buflen) {
		unsigned int len;

		len = min(miter.length, buflen - offset);

		if (to_buffer)
			memcpy(buf + offset, miter.addr, len);
		else
			memcpy(miter.addr, buf + offset, len);

		offset += len;
	}

	aml_sg_miter_stop(&miter);
	local_irq_restore(flags);

	return offset;
}

/*-------------------eMMC/tSD-------------------*/
int storage_flag;

bool is_emmc_exist(struct amlsd_host *host) // is eMMC/tSD exist
{
	pr_info("host->storage_flag=%d, POR_BOOT_VALUE=%d\n",
			host->storage_flag, POR_BOOT_VALUE);
	if ((host->storage_flag == EMMC_BOOT_FLAG)
			|| (host->storage_flag == SPI_EMMC_FLAG)
			|| (((host->storage_flag == 0)
					|| (host->storage_flag == -1))
				&& (POR_EMMC_BOOT() || POR_SPI_BOOT())))
		return true;

	return false;
}

/*----sdhc----*/
void aml_sdhc_print_reg_(u32 *buf)
{
	pr_info("***********SDHC_REGS***********\n");
	pr_info("SDHC_ARGU: 0x%08x\n", buf[SDHC_ARGU/4]);
	pr_info("SDHC_SEND: 0x%08x\n", buf[SDHC_SEND/4]);
	pr_info("SDHC_CTRL: 0x%08x\n", buf[SDHC_CTRL/4]);
	pr_info("SDHC_STAT: 0x%08x\n", buf[SDHC_STAT/4]);
	pr_info("SDHC_CLKC: 0x%08x\n", buf[SDHC_CLKC/4]);
	pr_info("SDHC_ADDR: 0x%08x\n", buf[SDHC_ADDR/4]);
	pr_info("SDHC_PDMA: 0x%08x\n", buf[SDHC_PDMA/4]);
	pr_info("SDHC_MISC: 0x%08x\n", buf[SDHC_MISC/4]);
	pr_info("SDHC_DATA: 0x%08x\n", buf[SDHC_DATA/4]);
	pr_info("SDHC_ICTL: 0x%08x\n", buf[SDHC_ICTL/4]);
	pr_info("SDHC_ISTA: 0x%08x\n", buf[SDHC_ISTA/4]);
	pr_info("SDHC_SRST: 0x%08x\n", buf[SDHC_SRST/4]);
	pr_info("SDHC_ESTA: 0x%08x\n", buf[SDHC_ESTA/4]);
	pr_info("SDHC_ENHC: 0x%08x\n", buf[SDHC_ENHC/4]);
	pr_info("SDHC_CLK2: 0x%08x\n", buf[SDHC_CLK2/4]);
}

void aml_sdhc_print_reg(struct amlsd_host *host)
{
	u32 buf[16];

	memcpy_fromio(buf, host->base, 0x3C);
	aml_sdhc_print_reg_(buf);
}

void aml_dbg_print_pinmux(void)
{
	pr_info("Pinmux:\n");
	pr_info("REG2 = 0x%08x\n", aml_read_cbus(PERIPHS_PIN_MUX_2));
	pr_info("REG3 = 0x%08x\n", aml_read_cbus(PERIPHS_PIN_MUX_3));
	pr_info("REG4 = 0x%08x\n", aml_read_cbus(PERIPHS_PIN_MUX_4));
	pr_info("REG5 = 0x%08x\n", aml_read_cbus(PERIPHS_PIN_MUX_5));
	pr_info("REG6 = 0x%08x\n", aml_read_cbus(PERIPHS_PIN_MUX_6));
	pr_info("REG7 = 0x%08x\n", aml_read_cbus(PERIPHS_PIN_MUX_7));
	pr_info("REG8 = 0x%08x\n", aml_read_cbus(PERIPHS_PIN_MUX_8));
}

/*-------------------debug---------------------*/
unsigned long sdhc_debug; // 0xffffffff;
static int __init sdhc_debug_setup(char *str)
{
	ssize_t status = 0;

	status = kstrtol(str, 0, &sdhc_debug);
	return 1;
}
__setup("sdhc_debug=", sdhc_debug_setup);

unsigned long sdio_debug; // 0xffffff;
static int __init sdio_debug_setup(char *str)
{
	ssize_t status = 0;

	status = kstrtol(str, 0, &sdio_debug);
	return 1;
}
__setup("sdio_debug=", sdio_debug_setup);
#endif
