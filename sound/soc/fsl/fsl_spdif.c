/*
 * ALSA SoC SPDIF Audio Layer for MXS
 *
 * Copyright (C) 2008-2013 Freescale Semiconductor, Inc.
 *
 * Based on stmp3xxx_spdif_dai.c
 * Vladimir Barinov <vbarinov@embeddedalley.com>
 * Copyright 2008 SigmaTel, Inc
 * Copyright 2008 Embedded Alley Solutions, Inc
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program  is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/asoundef.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include <mach/dma.h>
#include <mach/hardware.h>
#include <mach/busfreq.h>

#include <linux/pinctrl/consumer.h>

#include "imx-spdif.h"
#include "imx-pcm.h"

#define MXC_SPDIF_DEBUG 0

#define MXC_SPDIF_TXFIFO_WML      0x8
#define MXC_SPDIF_RXFIFO_WML      0x8

#define INTR_FOR_PLAYBACK (INT_TXFIFO_RESYNC)
#define INTR_FOR_CAPTURE (INT_SYM_ERR | INT_BIT_ERR | INT_URX_FUL | INT_URX_OV|\
		INT_QRX_FUL | INT_QRX_OV | INT_UQ_SYNC | INT_UQ_ERR |\
		INT_RX_RESYNC | INT_LOSS_LOCK | INT_DPLL_LOCKED)

static unsigned int gainsel_multi[GAINSEL_MULTI_MAX] = {
	24 * 1024, 16 * 1024, 12 * 1024,
	8 * 1024, 6 * 1024, 4 * 1024,
	3 * 1024,
};

static int mxc_spdif_default_data[] = {
	/*
	 * spdif0_clk will be 454.7MHz divided by ccm dividers.
	 * 44.1KHz: 454.7MHz / 7(ccm) / 23(spdif) = 44,128 Hz ~ 0.06% error
	 * 48KHz:   454.7MHz / 4(ccm) / 37(spdif) = 48,004 Hz ~ 0.01% error
	 * 32KHz:   454.7MHz / 6(ccm) / 37(spdif) = 32,003 Hz ~ 0.01% error
	 */
	1,		/* spdif_clk_44100: tx clk from spdif0_clk_root */
	1,		/* spdif_clk_48000: tx clk from spdif0_clk_root */
	23,		/* spdif_div_44100 */
	37,		/* spdif_div_48000 */
	37,		/* spdif_div_32000 */

	0,		/* spdif_rx_clk: rx clk from spdif stream */
};

struct mxc_spdif_platform_data {
	int spdif_clk_44100;	/* tx clk mux in SPDIF_REG_STC; -1 for none */
	int spdif_clk_48000;	/* tx clk mux in SPDIF_REG_STC; -1 for none */
	int spdif_div_44100;	/* tx clk div in SPDIF_REG_STC */
	int spdif_div_48000;	/* tx clk div in SPDIF_REG_STC */
	int spdif_div_32000;	/* tx clk div in SPDIF_REG_STC */
	int spdif_rx_clk;	/* rx clk mux select in SPDIF_REG_SRPC */
	int (*spdif_clk_set_rate) (struct clk *clk, unsigned long rate);
	struct clk *spdif_clk;
	struct clk *spdif_audio_clk;
};

/*
 * SPDIF control structure
 * Defines channel status, subcode and Q sub
 */
struct spdif_mixer_control {
	/* spinlock to access control data */
	spinlock_t ctl_lock;
	/* IEC958 channel tx status bit */
	unsigned char ch_status[4];
	/* User bits */
	unsigned char subcode[2 * SPDIF_UBITS_SIZE];
	/* Q subcode part of user bits */
	unsigned char qsub[2 * SPDIF_QSUB_SIZE];
	/* buffer ptrs for writer */
	unsigned int upos;
	unsigned int qpos;
	/* ready buffer index of the two buffers */
	unsigned int ready_buf;
};

struct mxc_spdif_priv {
	struct mxc_spdif_platform_data *plat_data;
	struct platform_device *imx_pcm_pdev;
	unsigned long __iomem *reg_base;
	unsigned long reg_phys_base;
	struct snd_card *card;	/* ALSA SPDIF sound card handle */
	struct snd_pcm *pcm;	/* ALSA spdif driver type handle */
	atomic_t dpll_locked;	/* DPLL locked status */
	bool tx_active;
	bool rx_active;
};

struct imx_pcm_dma_params dma_params_tx;
struct imx_pcm_dma_params dma_params_rx;

struct spdif_mixer_control mxc_spdif_control;

static unsigned long spdif_base_addr;


/* All the registers of SPDIF are 24-bit implemented */
static u32 spdif_read(u32 reg)
{
	return __raw_readl(spdif_base_addr + reg) & 0xffffff;
}

static void spdif_write(u32 val, u32 reg)
{
	__raw_writel(val & 0xffffff, spdif_base_addr + reg);
}

static void spdif_setbits(u32 bits, u32 reg)
{
	u32 regval;

	regval = spdif_read(reg);
	regval |= bits;
	spdif_write(regval, reg);
}

static void spdif_clrbits(u32 bits, u32 reg)
{
	u32 regval;

	regval = spdif_read(reg);
	regval &= ~bits;
	spdif_write(regval, reg);
}

static void spdif_setmask(u32 bits, u32 mask, u32 reg)
{
	u32 regval;

	regval = spdif_read(reg);
	regval = (regval & ~mask) | (bits & mask);
	spdif_write(regval, reg);
}

#if MXC_SPDIF_DEBUG
static void dumpregs(struct mxc_spdif_priv *priv)
{
	u32 value, i;

	if (!priv->tx_active && !priv->rx_active)
		clk_enable(priv->plat_data->spdif_clk);

	for (i = 0 ; i <= 0x38 ; i += 4) {
		value = spdif_read(i) & 0xffffff;
		pr_debug("reg 0x%02x = 0x%06x\n", i, value);
	}

	i = 0x44;
	value = spdif_read(i) & 0xffffff;
	pr_debug("reg 0x%02x = 0x%06x\n", i, value);

	i = 0x50;
	value = spdif_read(i) & 0xffffff;
	pr_debug("reg 0x%02x = 0x%06x\n", i, value);

	if (!priv->tx_active && !priv->rx_active)
		clk_disable(priv->plat_data->spdif_clk);
}
#else
static void dumpregs(struct mxc_spdif_priv *priv) {}
#endif

/* define each spdif interrupt handlers */
typedef void (*spdif_irq_func_t) (u32 bit, void *desc);

/* spdif irq functions declare */
static void spdif_irq_fifo(u32 bit, void *devid);
static void spdif_irq_dpll_lock(u32 bit, void *devid);
static void spdif_irq_uq(u32 bit, void *devid);
static void spdif_irq_bit_error(u32 bit, void *devid);
static void spdif_irq_sym_error(u32 bit, void *devid);
static void spdif_irq_valnogood(u32 bit, void *devid);
static void spdif_irq_cnew(u32 bit, void *devid);

/* irq function list */
static spdif_irq_func_t spdif_irq_handlers[] = {
	spdif_irq_fifo,
	spdif_irq_fifo,
	spdif_irq_dpll_lock,
	NULL,
	spdif_irq_fifo,
	spdif_irq_uq,
	spdif_irq_uq,
	spdif_irq_uq,
	spdif_irq_uq,
	spdif_irq_uq,
	spdif_irq_uq,
	NULL,
	NULL,
	NULL,
	spdif_irq_bit_error,
	spdif_irq_sym_error,
	spdif_irq_valnogood,
	spdif_irq_cnew,
	NULL,
	spdif_irq_fifo,
	spdif_irq_dpll_lock,
};

static void spdif_intr_enable(u32 intr, int enable)
{
	if (enable)
		spdif_setbits(intr, SPDIF_REG_SIE);
	else
		spdif_clrbits(intr, SPDIF_REG_SIE);
}

/*
 * Get spdif interrupt status and clear the interrupt
 */
static u32 spdif_intr_status_clear(void)
{
	u32 value;

	value = spdif_read(SPDIF_REG_SIS);
	value &= spdif_read(SPDIF_REG_SIE);
	spdif_write(value, SPDIF_REG_SIC);

	return value;
}

static irqreturn_t spdif_isr(int irq, void *dev_id)
{
	u32 int_stat;
	int line;

	int_stat = spdif_intr_status_clear();

	while ((line = ffs(int_stat)) != 0) {
		int_stat &= ~(1UL << (line - 1));
		if (spdif_irq_handlers[line - 1] != NULL)
			spdif_irq_handlers[line - 1] (line - 1, dev_id);
	}

	return IRQ_HANDLED;
}

/*
 * Rx FIFO Full, Underrun/Overrun interrupts
 * Tx FIFO Empty, Underrun/Overrun interrupts
 */
static void spdif_irq_fifo(u32 bit, void *devid)
{
	pr_debug("irq %s\n", __func__);
}

/*
 * DPLL locked and lock loss interrupt handler
 */
static void spdif_irq_dpll_lock(u32 bit, void *devid)
{
	struct mxc_spdif_priv *spdif_priv = (struct mxc_spdif_priv *)devid;
	unsigned int locked = spdif_read(SPDIF_REG_SRPC);

	if (locked & SRPC_DPLL_LOCKED) {
		pr_debug("SPDIF Rx dpll locked\n");
		atomic_set(&spdif_priv->dpll_locked, 1);
	} else {
		/* INT_LOSS_LOCK */
		pr_debug("SPDIF Rx dpll loss lock\n");
		atomic_set(&spdif_priv->dpll_locked, 0);
	}
}

/*
 * U/Q channel related interrupts handler
 *
 * U/QChannel full, overrun interrupts
 * U/QChannel sync error and frame error interrupts
 */
static void spdif_irq_uq(u32 bit, void *devid)
{
	u32 val;
	int index;
	struct spdif_mixer_control *ctrl = &mxc_spdif_control;

	bit = 1 << bit;
	/* get U/Q channel datas */
	switch (bit) {
	case INT_URX_OV:	/* read U data */
		pr_debug("User bit receive overrun\n");
	case INT_URX_FUL:
		pr_debug("U bit receive full\n");

		if (ctrl->upos >= SPDIF_UBITS_SIZE * 2) {
			ctrl->upos = 0;
		} else if (unlikely((ctrl->upos % SPDIF_UBITS_SIZE) + 3
					> SPDIF_UBITS_SIZE)) {
			pr_err("User bit receivce buffer overflow\n");
			break;
		}
		val = spdif_read(SPDIF_REG_SQU);
		ctrl->subcode[ctrl->upos++] = val >> 16;
		ctrl->subcode[ctrl->upos++] = val >> 8;
		ctrl->subcode[ctrl->upos++] = val;
		break;
	case INT_QRX_OV:	/* read Q data */
		pr_debug("Q bit receive overrun\n");
	case INT_QRX_FUL:
		pr_debug("Q bit receive full\n");

		if (ctrl->qpos >= SPDIF_QSUB_SIZE * 2) {
			ctrl->qpos = 0;
		} else if (unlikely((ctrl->qpos % SPDIF_QSUB_SIZE) + 3
				    > SPDIF_QSUB_SIZE)) {
			pr_debug("Q bit receivce buffer overflow\n");
			break;
		}
		val = spdif_read(SPDIF_REG_SRQ);
		ctrl->qsub[ctrl->qpos++] = val >> 16;
		ctrl->qsub[ctrl->qpos++] = val >> 8;
		ctrl->qsub[ctrl->qpos++] = val;
		break;
	case INT_UQ_ERR:	/* read U/Q data and do buffer reset */
		pr_debug("U/Q bit receive error\n");
		val = spdif_read(SPDIF_REG_SQU);
		val = spdif_read(SPDIF_REG_SRQ);
		/* drop this U/Q buffer */
		ctrl->ready_buf = 0;
		ctrl->upos = 0;
		ctrl->qpos = 0;
		break;
	case INT_UQ_SYNC:	/* U/Q buffer reset */
		pr_debug("U/Q sync receive\n");

		if (ctrl->qpos == 0)
			break;
		index = (ctrl->qpos - 1) / SPDIF_QSUB_SIZE;
		/* set ready to this buffer */
		ctrl->ready_buf = index + 1;
		break;
	default:
		return;
	}
}

/*
 * SPDIF receiver found parity bit error interrupt handler
 */
static void spdif_irq_bit_error(u32 bit, void *devid)
{
	pr_debug("SPDIF interrupt parity bit error\n");
}

/*
 * SPDIF receiver found illegal symbol interrupt handler
 */
static void spdif_irq_sym_error(u32 bit, void *devid)
{
	struct mxc_spdif_priv *spdif_priv = (struct mxc_spdif_priv *)devid;

	pr_debug("SPDIF interrupt symbol error\n");
	if (!atomic_read(&spdif_priv->dpll_locked)) {
		/* dpll unlocked seems no audio stream */
		spdif_intr_enable(INT_SYM_ERR, 0);
	}
}

/*
 * SPDIF validity flag no good interrupt handler
 */
static void spdif_irq_valnogood(u32 bit, void *devid)
{
	pr_debug("SPDIF interrupt validate is not good\n");
}

/*
 * SPDIF receive change in value of control channel
 */
static void spdif_irq_cnew(u32 bit, void *devid)
{
	pr_debug("SPDIF interrupt cstatus new\n");
}

static void spdif_softreset(void)
{
	u32 value = 1;
	int cycle = 0;

	spdif_write(SCR_SOFT_RESET, SPDIF_REG_SCR);

	while (value && (cycle++ < 10))
		value = spdif_read(SPDIF_REG_SCR) & SCR_SOFT_RESET;
}

static void spdif_set_clk_accuracy(u8 level)
{
	mxc_spdif_control.ch_status[3] &= ~IEC958_AES3_CON_CLOCK;
	mxc_spdif_control.ch_status[3] |= level & IEC958_AES3_CON_CLOCK;
}

static void spdif_set_cstatus_fs(u8 cstatus_fs)
{
	/* set fs field in consumer channel status */
	mxc_spdif_control.ch_status[3] &= ~IEC958_AES3_CON_FS;
	mxc_spdif_control.ch_status[3] |= cstatus_fs & IEC958_AES3_CON_FS;
}

static u8 reverse_bits(u8 input)
{
	u8 i, output = 0;
	for (i = 8 ; i > 0 ; i--) {
		output <<= 1;
		output |= input & 0x01;
		input >>= 1;
	}
	return output;
}

static void spdif_write_channel_status(void)
{
	unsigned int ch_status;

	ch_status =
		(reverse_bits(mxc_spdif_control.ch_status[0]) << 16) |
		(reverse_bits(mxc_spdif_control.ch_status[1]) << 8) |
		reverse_bits(mxc_spdif_control.ch_status[2]);

	spdif_write(ch_status, SPDIF_REG_STCSCH);

	ch_status = reverse_bits(mxc_spdif_control.ch_status[3]) << 16;
	spdif_write(ch_status, SPDIF_REG_STCSCL);

	pr_debug("STCSCH: 0x%06x\n",
			spdif_read(SPDIF_REG_STCSCH));
	pr_debug("STCSCL: 0x%06x\n",
			spdif_read(SPDIF_REG_STCSCL));
}

/*
 * set SPDIF PhaseConfig register for rx clock
 */
static int spdif_set_rx_clksrc(enum spdif_clk_src clksrc,
		enum spdif_gainsel gainsel, int dpll_locked)
{
	u32 value;

	if (clksrc > SPDIF_CLK_SRC5 || gainsel > GAINSEL_MULTI_3)
		return 1;

	if (dpll_locked)
		value = clksrc;
	else
		value = clksrc + SRPC_CLKSRC_SEL_LOCKED;

	spdif_setmask(SRPC_CLKSRC_SEL_SET(value) | SRPC_GAINSEL_SET(gainsel),
			SRPC_CLKSRC_SEL_MASK | SRPC_GAINSEL_MASK,
			SPDIF_REG_SRPC);

	return 0;
}

/*
 * Get RX data clock rate given the SPDIF bus_clk
 */
static int spdif_get_rxclk_rate(struct clk *bus_clk, enum spdif_gainsel gainsel)
{
	unsigned long freqmeas, phaseconf, busclk_freq = 0;
	u64 tmpval64;
	enum spdif_clk_src clksrc;

	freqmeas = spdif_read(SPDIF_REG_SRFM);
	phaseconf = spdif_read(SPDIF_REG_SRPC);

	clksrc = (phaseconf >> SRPC_CLKSRC_SEL_OFFSET) & 0x0F;
	if (clksrc < 5 && (phaseconf & SRPC_DPLL_LOCKED)) {
		/* get bus clock from system */
		busclk_freq = clk_get_rate(bus_clk);
	}

	/* FreqMeas_CLK = (BUS_CLK*FreqMeas[23:0])/2^10/GAINSEL/128 */
	tmpval64 = (u64) busclk_freq * freqmeas;
	do_div(tmpval64, gainsel_multi[gainsel]);
	do_div(tmpval64, 128 * 1024);

	pr_debug("FreqMeas:    %d\n", (int)freqmeas);
	pr_debug("busclk_freq: %d\n", (int)busclk_freq);
	pr_debug("rx rate:     %d\n", (int)tmpval64);

	return (int)tmpval64;
}

static int spdif_clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long rate_actual;

	rate_actual = clk_round_rate(clk, rate);
	clk_set_rate(clk, rate_actual);
	return 0;
}

static int spdif_set_sample_rate(struct snd_pcm_substream *substream,
		int sample_rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mxc_spdif_priv *spdif_priv =
		snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;
	unsigned long clk = -1, div = 1, cstatus_fs = 0;
	u32 stc, mask;
	int clk_fs;

	switch (sample_rate) {
	case 44100:
		clk_fs = 44100;
		clk = plat_data->spdif_clk_44100;
		div = plat_data->spdif_div_44100;
		cstatus_fs = IEC958_AES3_CON_FS_44100;
		break;
	case 48000:
		clk_fs = 48000;
		clk = plat_data->spdif_clk_48000;
		div = plat_data->spdif_div_48000;
		cstatus_fs = IEC958_AES3_CON_FS_48000;
		break;
	case 32000:
		/* use 48K clk for 32K */
		clk_fs = 48000;
		clk = plat_data->spdif_clk_48000;
		div = plat_data->spdif_div_32000;
		cstatus_fs = IEC958_AES3_CON_FS_32000;
		break;
	default:
		pr_err("%s: unsupported sample rate %d\n",
				__func__, sample_rate);
		return -EINVAL;
	}

	if (clk < 0) {
		pr_info("%s: no defined %d clk src\n", __func__, clk_fs);
		return -EINVAL;
	}

	/*
	 * The S/PDIF block needs a clock of 64 * fs * div.  The S/PDIF block
	 * will divide by (div).  So request 64 * fs * (div+1) which will
	 * get rounded.
	 */
	if (plat_data->spdif_clk_set_rate)
		plat_data->spdif_clk_set_rate(plat_data->spdif_clk,
				64 * sample_rate * (div + 1));

#if MXC_SPDIF_DEBUG
	pr_debug("%s wanted spdif clock rate = %d\n", __func__,
			(int)(64 * sample_rate * div));
	pr_debug("%s got spdif clock rate    = %d\n", __func__,
			(int)clk_get_rate(plat_data->spdif_clk));
#endif

	spdif_set_cstatus_fs(cstatus_fs);

	/* select clock source and divisor */
	stc = STC_TXCLK_SRC_EN | STC_TXCLK_SRC_SET(clk) | STC_TXCLK_DIV(div);
	mask = STC_TXCLK_SRC_EN_MASK | STC_TXCLK_SRC_MASK | STC_TXCLK_DIV_MASK;
	spdif_setmask(stc, mask, SPDIF_REG_STC);
	pr_debug("set sample rate to %d\n", sample_rate);

	return 0;
}

static unsigned int spdif_playback_rates[] = { 32000, 44100, 48000 };

static unsigned int spdif_capture_rates[] = {
	16000, 32000, 44100, 48000, 64000, 96000
};

static struct snd_pcm_hw_constraint_list hw_playback_rates = {
	.count = ARRAY_SIZE(spdif_playback_rates),
	.list = spdif_playback_rates,
	.mask = 0,
};

static struct snd_pcm_hw_constraint_list hw_capture_rates = {
	.count = ARRAY_SIZE(spdif_capture_rates),
	.list = spdif_capture_rates,
	.mask = 0,
};

/*
 * MXC SPDIF IEC958 controller(mixer) functions
 *
 *	Channel status get/put control
 *	User bit value get/put control
 *	Valid bit value get control
 *	DPLL lock status get control
 *	User bit sync mode selection control
 */
static int mxc_pb_spdif_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int mxc_pb_spdif_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *uvalue)
{
	uvalue->value.iec958.status[0] = mxc_spdif_control.ch_status[0];
	uvalue->value.iec958.status[1] = mxc_spdif_control.ch_status[1];
	uvalue->value.iec958.status[2] = mxc_spdif_control.ch_status[2];
	uvalue->value.iec958.status[3] = mxc_spdif_control.ch_status[3];
	return 0;
}

static int mxc_pb_spdif_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *uvalue)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct mxc_spdif_priv *spdif_priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;

	mxc_spdif_control.ch_status[0] = uvalue->value.iec958.status[0];
	mxc_spdif_control.ch_status[1] = uvalue->value.iec958.status[1];
	mxc_spdif_control.ch_status[2] = uvalue->value.iec958.status[2];
	mxc_spdif_control.ch_status[3] = uvalue->value.iec958.status[3];

	clk_enable(plat_data->spdif_clk);

	spdif_write_channel_status();

	clk_disable(plat_data->spdif_clk);

	return 0;
}

static int mxc_spdif_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

/*
 * Get channel status from SPDIF_RX_CCHAN register
 */
static int mxc_spdif_capture_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct mxc_spdif_priv *spdif_priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;
	unsigned int cstatus;

	clk_enable(plat_data->spdif_clk);

	if (!(spdif_read(SPDIF_REG_SIS) & INT_CNEW)) {
		clk_disable(plat_data->spdif_clk);
		return -EAGAIN;
	}

	cstatus = spdif_read(SPDIF_REG_SRCSLH);
	ucontrol->value.iec958.status[0] = (cstatus >> 16) & 0xFF;
	ucontrol->value.iec958.status[1] = (cstatus >> 8) & 0xFF;
	ucontrol->value.iec958.status[2] = cstatus & 0xFF;

	cstatus = spdif_read(SPDIF_REG_SRCSLL);
	ucontrol->value.iec958.status[3] = (cstatus >> 16) & 0xFF;
	ucontrol->value.iec958.status[4] = (cstatus >> 8) & 0xFF;
	ucontrol->value.iec958.status[5] = cstatus & 0xFF;

	/* clear intr */
	spdif_write(INT_CNEW, SPDIF_REG_SIC);

	clk_disable(plat_data->spdif_clk);

	return 0;
}

/*
 * Get User bits (subcode) from chip value which readed out
 * in UChannel register.
 */
static int mxc_spdif_subcode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&mxc_spdif_control.ctl_lock, flags);
	if (mxc_spdif_control.ready_buf) {
		int idx = (mxc_spdif_control.ready_buf - 1) * SPDIF_UBITS_SIZE;
		memcpy(&ucontrol->value.iec958.subcode[0],
				&mxc_spdif_control.subcode[idx],
				SPDIF_UBITS_SIZE);
	} else {
		ret = -EAGAIN;
	}
	spin_unlock_irqrestore(&mxc_spdif_control.ctl_lock, flags);

	return ret;
}

/*
 * Q-subcode infomation.
 * the byte size is SPDIF_UBITS_SIZE/8
 */
static int mxc_spdif_qinfo(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = SPDIF_QSUB_SIZE;
	return 0;
}

/*
 * Get Q subcode from chip value which readed out
 * in QChannel register.
 */
static int mxc_spdif_qget(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&mxc_spdif_control.ctl_lock, flags);
	if (mxc_spdif_control.ready_buf) {
		int idx = (mxc_spdif_control.ready_buf - 1) * SPDIF_QSUB_SIZE;
		memcpy(&ucontrol->value.bytes.data[0],
				&mxc_spdif_control.qsub[idx],
				SPDIF_QSUB_SIZE);
	} else {
		ret = -EAGAIN;
	}
	spin_unlock_irqrestore(&mxc_spdif_control.ctl_lock, flags);

	return ret;
}

/*
 * Valid bit infomation.
 */
static int mxc_spdif_vbit_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

/*
 * Get valid good bit from interrupt status register.
 */
static int mxc_spdif_vbit_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct mxc_spdif_priv *spdif_priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;
	unsigned int int_val;

	clk_enable(plat_data->spdif_clk);

	int_val = spdif_read(SPDIF_REG_SIS);
	ucontrol->value.integer.value[0] = (int_val & INT_VAL_NOGOOD) != 0;
	spdif_write(INT_VAL_NOGOOD, SPDIF_REG_SIC);

	clk_disable(plat_data->spdif_clk);

	return 0;
}

/*
 * DPLL lock infomation.
 */
static int mxc_spdif_rxrate_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 16000;
	uinfo->value.integer.max = 96000;
	return 0;
}

/*
 * Get DPLL lock or not info from stable interrupt status register.
 * User application must use this control to get locked,
 * then can do next PCM operation
 */
static int mxc_spdif_rxrate_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct mxc_spdif_priv *spdif_priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;

	if (atomic_read(&spdif_priv->dpll_locked)) {
		clk_enable(plat_data->spdif_clk);
		ucontrol->value.integer.value[0] =
			spdif_get_rxclk_rate(plat_data->spdif_clk,
					SPDIF_DEFAULT_GAINSEL);
		clk_disable(plat_data->spdif_clk);
	} else {
		ucontrol->value.integer.value[0] = 0;
	}
	return 0;
}

/*
 * User bit sync mode info
 */
static int mxc_spdif_usync_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

/*
 * User bit sync mode:
 * 1 CD User channel subcode
 * 0 Non-CD data
 */
static int mxc_spdif_usync_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct mxc_spdif_priv *spdif_priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;
	unsigned int int_val;

	clk_enable(plat_data->spdif_clk);

	int_val = spdif_read(SPDIF_REG_SRCD);
	ucontrol->value.integer.value[0] = (int_val & SRCD_CD_USER) != 0;

	clk_disable(plat_data->spdif_clk);

	return 0;
}

/*
 * User bit sync mode:
 * 1 CD User channel subcode
 * 0 Non-CD data
 */
static int mxc_spdif_usync_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct mxc_spdif_priv *spdif_priv = snd_soc_dai_get_drvdata(cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;
	u32 int_val = ucontrol->value.integer.value[0] << SRCD_CD_USER_OFFSET;

	clk_enable(plat_data->spdif_clk);

	spdif_setmask(int_val, SRCD_CD_USER, SPDIF_REG_SRCD);

	clk_disable(plat_data->spdif_clk);

	return 0;
}

/*
 * MXC SPDIF IEC958 controller defines
 */
static struct snd_kcontrol_new mxc_spdif_ctrls[] = {
	/* status cchanel controller */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_WRITE |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = mxc_pb_spdif_info,
		.get = mxc_pb_spdif_get,
		.put = mxc_pb_spdif_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", CAPTURE, DEFAULT),
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = mxc_spdif_info,
		.get = mxc_spdif_capture_get,
	},
	/* user bits controller */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "IEC958 Subcode Capture Default",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = mxc_spdif_info,
		.get = mxc_spdif_subcode_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "IEC958 Q-subcode Capture Default",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = mxc_spdif_qinfo,
		.get = mxc_spdif_qget,
	},
	/* valid bit error controller */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "IEC958 V-Bit Errors",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = mxc_spdif_vbit_info,
		.get = mxc_spdif_vbit_get,
	},
	/* DPLL lock info get controller */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "RX Sample Rate",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = mxc_spdif_rxrate_info,
		.get = mxc_spdif_rxrate_get,
	},
	/* User bit sync mode set/get controller */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "IEC958 USyncMode CDText",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_WRITE |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = mxc_spdif_usync_info,
		.get = mxc_spdif_usync_get,
		.put = mxc_spdif_usync_put,
	},
};

int mxc_spdif_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *cpu_dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mxc_spdif_priv *spdif_priv =
		snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;
	int is_playack = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);
	u32 scr, mask;
	int ret = 0;

	/* Tx/Rx config */
	snd_soc_dai_set_dma_data(cpu_dai, substream,
			is_playack ? &dma_params_tx : &dma_params_rx);

	/* enable spdif_xtal_clk */
	ret = clk_enable(plat_data->spdif_clk);
	if (ret < 0)
		goto failed_clk;

	pm_runtime_get_sync(cpu_dai->dev);

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			is_playack ? &hw_playback_rates : &hw_capture_rates);
	if (ret < 0)
		goto failed_hwconstrain;

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto failed_hwconstrain;

	/* Reset module and interrupts only for first initialization */
	if (!spdif_priv->tx_active && !spdif_priv->rx_active) {
		spdif_softreset();

		/* disable all the interrupts */
		spdif_intr_enable(0xffffff, 0);
	}

	if (is_playack) {
		spdif_priv->tx_active = true;
		scr = SCR_TXFIFO_AUTOSYNC | SCR_TXFIFO_CTRL_NORMAL |
			SCR_TXSEL_NORMAL | SCR_USRC_SEL_CHIP |
			SCR_TXFIFO_FSEL_IF8;
		mask = SCR_TXFIFO_AUTOSYNC_MASK | SCR_TXFIFO_CTRL_MASK |
			SCR_TXSEL_MASK | SCR_USRC_SEL_MASK |
			SCR_TXFIFO_FSEL_MASK;
	} else {
		spdif_priv->rx_active = true;
		scr = SCR_RXFIFO_FSEL_IF8 | SCR_RXFIFO_AUTOSYNC;
		mask = SCR_RXFIFO_FSEL_MASK | SCR_RXFIFO_AUTOSYNC_MASK|
			SCR_RXFIFO_CTL_MASK | SCR_RXFIFO_OFF_MASK;
	}
	spdif_setmask(scr, mask, SPDIF_REG_SCR);

	/* Power up SPDIF module */
	spdif_clrbits(SCR_LOW_POWER, SPDIF_REG_SCR);

	return 0;

failed_hwconstrain:
	clk_disable(plat_data->spdif_clk);
failed_clk:
	return ret;
}

static void mxc_spdif_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *cpu_dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mxc_spdif_priv *spdif_priv =
		snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;
	u32 scr, mask;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		spdif_priv->tx_active = false;
		scr = 0;
		mask = SCR_TXFIFO_AUTOSYNC_MASK | SCR_TXFIFO_CTRL_MASK |
			SCR_TXSEL_MASK | SCR_USRC_SEL_MASK |
			SCR_TXFIFO_FSEL_MASK;
	} else {
		spdif_priv->rx_active = false;
		scr = SCR_RXFIFO_OFF | SCR_RXFIFO_CTL_ZERO;
		mask = SCR_RXFIFO_FSEL_MASK | SCR_RXFIFO_AUTOSYNC_MASK|
			SCR_RXFIFO_CTL_MASK | SCR_RXFIFO_OFF_MASK;
	}
	spdif_setmask(scr, mask, SPDIF_REG_SCR);

	/* Power down SPDIF module only if tx&rx are both inactive */
	if (!spdif_priv->tx_active && !spdif_priv->rx_active) {
		spdif_intr_status_clear();
		spdif_setbits(SCR_LOW_POWER, SPDIF_REG_SCR);
	}

	pm_runtime_put_sync(cpu_dai->dev);

	/* disable spdif clock  */
	clk_disable(plat_data->spdif_clk);
}

static int mxc_spdif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mxc_spdif_priv *spdif_priv =
		snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct mxc_spdif_platform_data *plat_data = spdif_priv->plat_data;
	unsigned int sample_rate = params_rate(params);
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret  = spdif_set_sample_rate(substream, sample_rate);
		if (ret < 0) {
			pr_info("%s - err < 0\n", __func__);
			return ret;
		}
		spdif_set_clk_accuracy(IEC958_AES3_CON_CLOCK_1000PPM);
		spdif_write_channel_status();
	} else {
		/* setup rx clock source */
		spdif_set_rx_clksrc(plat_data->spdif_rx_clk,
				SPDIF_DEFAULT_GAINSEL, 1);
	}

	dumpregs(spdif_priv);

	return ret;
}

static int mxc_spdif_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mxc_spdif_priv *spdif_priv =
		snd_soc_dai_get_drvdata(rtd->cpu_dai);
	int is_playack = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);
	u32 intr = is_playack ? INTR_FOR_PLAYBACK : INTR_FOR_CAPTURE;
	u32 dmaen = is_playack ? SCR_DMA_TX_EN : SCR_DMA_RX_EN;;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		spdif_intr_enable(intr, 1);
		spdif_setbits(dmaen, SPDIF_REG_SCR);
		dumpregs(spdif_priv);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spdif_clrbits(dmaen, SPDIF_REG_SCR);
		spdif_intr_enable(intr, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

struct snd_soc_dai_ops fsl_spdif_dai_ops = {
	.startup = mxc_spdif_startup,
	.hw_params = mxc_spdif_hw_params,
	.trigger = mxc_spdif_trigger,
	.shutdown = mxc_spdif_shutdown,
};

static int fsl_spdif_soc_probe(struct snd_soc_dai *dai)
{
	snd_soc_add_dai_controls(dai, mxc_spdif_ctrls,
			ARRAY_SIZE(mxc_spdif_ctrls));
	return 0;
}

static int fsl_spdif_soc_remove(struct snd_soc_dai *dai)
{
	return 0;
}

struct snd_soc_dai_driver fsl_spdif_dai = {
	.probe		= &fsl_spdif_soc_probe,
	.remove		= &fsl_spdif_soc_remove,
	.ops		= &fsl_spdif_dai_ops,
	.playback	= {
		.channels_min	= 2,
		.channels_max	= 2,
		.formats	= MXC_SPDIF_FORMATS_PLAYBACK,
	},
	.capture	= {
		.channels_min	= 2,
		.channels_max	= 2,
		.rates		= MXC_SPDIF_RATES_CAPTURE,
		.formats	= MXC_SPDIF_FORMATS_CAPTURE,
	},
};

static int fsl_spdif_dai_probe(struct platform_device *pdev)
{
	struct mxc_spdif_platform_data *plat_data =
		dev_get_platdata(&pdev->dev);
	struct mxc_spdif_priv *spdif_priv;
	struct device_node *np = pdev->dev.of_node;
	struct resource res;
	struct pinctrl *pinctrl;
	u32 dma_events[2];
	int err, irq;
	int ret = 0;

	dev_info(&pdev->dev, "MXC SPDIF Audio\n");

	spdif_priv = kzalloc(sizeof(struct mxc_spdif_priv), GFP_KERNEL);
	if (spdif_priv == NULL)
		return -ENOMEM;

	/* We might get plat_data from DTS file. */
	if (!plat_data && pdev->dev.of_node) {
		plat_data = devm_kzalloc(&pdev->dev,
				sizeof(struct mxc_spdif_platform_data),
				GFP_KERNEL);
		if (!plat_data) {
			dev_err(&pdev->dev, "Failed to allocate pdata\n");
			goto error_kzalloc;
		}
		if (!of_device_is_available(np))
			goto error_kzalloc;

		/* Get the addresses and IRQ */
		ret = of_address_to_resource(np, 0, &res);
		if (ret) {
			dev_err(&pdev->dev, "Failed to get device resource\n");
			goto error_kzalloc;
		}

		pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
		if (IS_ERR(pinctrl)) {
			dev_err(&pdev->dev, "Failed to setup pinctrl\n");
			ret = PTR_ERR(pinctrl);
			goto error_kzalloc;
		}

		plat_data->spdif_clk_set_rate = &spdif_clk_set_rate;

		plat_data->spdif_clk_44100 = mxc_spdif_default_data[0];
		plat_data->spdif_clk_48000 = mxc_spdif_default_data[1];
		plat_data->spdif_div_44100 = mxc_spdif_default_data[2];
		plat_data->spdif_div_48000 = mxc_spdif_default_data[3];
		plat_data->spdif_div_32000 = mxc_spdif_default_data[4];
		plat_data->spdif_rx_clk = mxc_spdif_default_data[5];

		pdev->id = of_alias_get_id(np, "audio");
		if (pdev->id < 0) {
			dev_err(&pdev->dev, "Missing alias in devicetree");
			goto error_kzalloc;
		}

		spdif_priv->imx_pcm_pdev =
			platform_device_register_simple("imx-pcm-audio",
					pdev->id, NULL, 0);
		if (IS_ERR(spdif_priv->imx_pcm_pdev)) {
			ret = PTR_ERR(spdif_priv->imx_pcm_pdev);
			goto error_kzalloc;
		}
	}


	if (plat_data->spdif_clk_44100 >= 0)
		fsl_spdif_dai.playback.rates |= SNDRV_PCM_RATE_44100;
	if (plat_data->spdif_clk_48000 >= 0)
		fsl_spdif_dai.playback.rates |= SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_48000;

	spdif_priv->tx_active = false;
	spdif_priv->rx_active = false;
	platform_set_drvdata(pdev, spdif_priv);

	spdif_priv->reg_phys_base = res.start;
	spdif_priv->reg_base = ioremap(res.start, res.end - res.start + 1);
	spdif_priv->plat_data = plat_data;

	spdif_base_addr = (unsigned long)spdif_priv->reg_base;

	/* initial spinlock for control data */
	spin_lock_init(&mxc_spdif_control.ctl_lock);

	/* init tx channel status default value */
	mxc_spdif_control.ch_status[0] =
		IEC958_AES0_CON_NOT_COPYRIGHT |
		IEC958_AES0_CON_EMPHASIS_5015;
	mxc_spdif_control.ch_status[1] = IEC958_AES1_CON_DIGDIGCONV_ID;
	mxc_spdif_control.ch_status[2] = 0x00;
	mxc_spdif_control.ch_status[3] =
		IEC958_AES3_CON_FS_44100 | IEC958_AES3_CON_CLOCK_1000PPM;

	plat_data->spdif_clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(plat_data->spdif_clk)) {
		ret = PTR_ERR(plat_data->spdif_clk);
		dev_err(&pdev->dev, "Failed to get clock: %d\n", ret);
		goto failed_clk;
	}
	clk_prepare_enable(plat_data->spdif_clk);

	atomic_set(&spdif_priv->dpll_locked, 0);

	/* spdif interrupt register and disable */
	irq = platform_get_irq(pdev, 0);
	if ((irq <= 0) || request_irq(irq, spdif_isr, 0, "spdif", spdif_priv)) {
		dev_err(&pdev->dev, "Failed to request irq\n");
		err = -EBUSY;
		goto card_err;
	}

	dma_params_tx.dma_addr = res.start + SPDIF_REG_STL;
	dma_params_rx.dma_addr = res.start + SPDIF_REG_SRL;

	dma_params_tx.burstsize = MXC_SPDIF_TXFIFO_WML;
	dma_params_rx.burstsize = MXC_SPDIF_RXFIFO_WML;

	dma_params_tx.peripheral_type = IMX_DMATYPE_SPDIF;
	dma_params_rx.peripheral_type = IMX_DMATYPE_SPDIF;

	/*
	 * TODO: This is a temporary solution and should be changed
	 * to use generic DMA binding later when the helplers get in.
	 */
	ret = of_property_read_u32_array(pdev->dev.of_node,
			"fsl,spdif-dma-events", dma_events, 2);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get dma events\n");
		goto error_clk;
	}
	dma_params_tx.dma = dma_events[0];
	dma_params_rx.dma = dma_events[1];

	pm_runtime_enable(&pdev->dev);

	ret = snd_soc_register_dai(&pdev->dev, &fsl_spdif_dai);
	if (ret) {
		dev_err(&pdev->dev, "Faile to register DAI\n");
		goto failed_register;
	}

	dumpregs(spdif_priv);

	return 0;

failed_register:
error_clk:
	free_irq(irq, spdif_priv);
card_err:
	clk_put(plat_data->spdif_clk);
failed_clk:
	platform_set_drvdata(pdev, NULL);
	if (!IS_ERR(spdif_priv->imx_pcm_pdev))
		platform_device_unregister(spdif_priv->imx_pcm_pdev);
error_kzalloc:
	kfree(spdif_priv);
	return ret;
}

static int __devexit fsl_spdif_dai_remove(struct platform_device *pdev)
{
	struct mxc_spdif_priv *spdif_priv = platform_get_drvdata(pdev);

	snd_soc_unregister_dai(&pdev->dev);
	if (!IS_ERR(spdif_priv->imx_pcm_pdev))
		platform_device_unregister(spdif_priv->imx_pcm_pdev);

	platform_set_drvdata(pdev, NULL);
	kfree(spdif_priv);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int fsl_spdif_runtime_resume(struct device *dev)
{
	request_bus_freq(BUS_FREQ_AUDIO);
	return 0;
}

static int fsl_spdif_runtime_suspend(struct device *dev)
{
	release_bus_freq(BUS_FREQ_AUDIO);
	return 0;
}
#endif

static const struct dev_pm_ops fsl_spdif_pm = {
	SET_RUNTIME_PM_OPS(fsl_spdif_runtime_suspend,
			fsl_spdif_runtime_resume,
			NULL)
};

static const struct of_device_id fsl_spdif_dai_dt_ids[] = {
	{ .compatible = "fsl,fsl-spdif", },
	{}
};
MODULE_DEVICE_TABLE(of, fsl_spdif_dai_dt_ids);


static struct platform_driver fsl_spdif_dai_driver = {
	.probe = fsl_spdif_dai_probe,
	.remove = __devexit_p(fsl_spdif_dai_remove),
	.driver = {
		.name = "fsl-spdif-driver",
		.owner = THIS_MODULE,
		.of_match_table = fsl_spdif_dai_dt_ids,
		.pm = &fsl_spdif_pm,
	},
};

module_platform_driver(fsl_spdif_dai_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale S/PDIF DAI");
MODULE_LICENSE("GPL");
