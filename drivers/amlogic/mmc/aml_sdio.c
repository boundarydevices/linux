/*
 * drivers/amlogic/mmc/aml_sdio.c
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
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mmc/host.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/irq.h>
#include <linux/amlogic/sd.h>
#include <linux/amlogic/amlsd.h>
#include <linux/mmc/emmc_partitions.h>
#include <linux/amlogic/gpio-amlogic.h>

/* static struct mmc_claim aml_sdio_claim; */
#define	 sdio_cmd_busy_bit 4
int CMD_PROCESS_JIT;
int SDIO_IRQ_SUPPORT;
static void aml_sdio_send_stop(struct amlsd_host *host);
static unsigned int sdio_error_flag;
static unsigned int sdio_debug_flag;
static unsigned int sdio_err_bak;
static unsigned int timeout_cmd_cnt;

void sdio_debug_irqstatus(struct sdio_status_irq *irqs, struct cmd_send *send)
{
	switch (sdio_debug_flag) {
	case 1:
		irqs->sdio_response_crc7_ok = 0;
		send->response_do_not_have_crc7 = 0;
		pr_err("Force sdio cmd response crc error here\n");
		break;
	case 2:
		irqs->sdio_data_read_crc16_ok = 0;
		irqs->sdio_data_write_crc16_ok = 0;
		pr_err("Force sdio data crc here\n");
		break;
	default:
		break;
	}
	/* only enable once for debug */
	sdio_debug_flag = 0;
}

static void aml_sdio_soft_reset(struct amlsd_host *host)
{
	struct sdio_irq_config irqc = {0};

	/*soft reset*/
	irqc.soft_reset = 1;
	writel(*(u32 *)&irqc, host->base + SDIO_IRQC);
	udelay(2);
}

static int aml_sdio_clktree_init(struct amlsd_host *host)
{
	int ret = 0;

	host->core_clk = devm_clk_get(host->dev, "core");
	if (IS_ERR(host->core_clk)) {
		ret = PTR_ERR(host->core_clk);
		pr_err("devm_clk_get core_clk fail %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(host->core_clk);
	if (ret) {
		pr_err("clk_prepare_enable core_clk fail %d\n", ret);
		return ret;
	}

	pr_info("aml_sdio_clktree_init ok\n");
	return 0;
}

/*
 * init sdio reg
 */
static void aml_sdio_init_param(struct amlsd_host *host)
{
	struct sdio_status_irq irqs = {0};
	struct sdio_config conf = {0};

	aml_sdio_clktree_init(host);

	/* write 1 clear bit8,9 */
	irqs.sdio_if_int = 1;
	irqs.sdio_cmd_int = 1;
	writel(*(u32 *)&irqs, host->base + SDIO_IRQS);

	/* setup config */
	conf.sdio_write_crc_ok_status = 2;
	conf.sdio_write_nwr = 2;
	conf.m_endian = 3;
	conf.cmd_argument_bits = 39;
	conf.cmd_out_at_posedge = 0;
	conf.cmd_disable_crc = 0;
	conf.data_latch_at_negedge = 0;
	conf.cmd_clk_divide = CLK_DIV;
	writel(*(u32 *)&conf, host->base + SDIO_CONF);
}

static bool is_card_last_block(struct amlsd_platform *pdata, u32 lba, u32 cnt)
{
	if (!pdata->card_capacity)
		pdata->card_capacity = mmc_capacity(pdata->mmc->card);

	return (lba + cnt) == pdata->card_capacity;
}

/*
 * read response from REG0_ARGU(136bit or 48bit)
 */
void aml_sdio_read_response(struct amlsd_platform *pdata,
		struct mmc_request *mrq)
{
	int i, resp[4];
	struct amlsd_host *host = pdata->host;
	u32 vmult = readl(host->base + SDIO_MULT);
	struct sdio_mult_config *mult = (void *)&vmult;
	struct mmc_command *cmd = mrq->cmd;

	mult->write_read_out_index = 1;
	mult->response_read_index = 0;
	writel(vmult, host->base + SDIO_MULT);

	if (cmd->flags & MMC_RSP_136) {
		for (i = 0; i <= 3; i++)
			resp[3-i] = readl(host->base + SDIO_ARGU);
		cmd->resp[0] = (resp[0]<<8)|((resp[1]>>24)&0xff);
		cmd->resp[1] = (resp[1]<<8)|((resp[2]>>24)&0xff);
		cmd->resp[2] = (resp[2]<<8)|((resp[3]>>24)&0xff);
		cmd->resp[3] = (resp[3]<<8);
		pr_debug("Cmd %d ,Resp %x-%x-%x-%x\n",
				cmd->opcode, cmd->resp[0], cmd->resp[1],
				cmd->resp[2], cmd->resp[3]);
	} else if (cmd->flags & MMC_RSP_PRESENT) {
		cmd->resp[0] = readl(host->base + SDIO_ARGU);
		pr_debug("Cmd %d, Resp 0x%x\n",
				cmd->opcode, cmd->resp[0]);

		/* Now in sdio controller, something is wrong.
		 * When we read last block in multi-blocks-mode,
		 * it will cause "ADDRESS_OUT_OF_RANGE" error in
		 * card status, we must clear it.
		 */
		/* status error: address out of range */
		if ((cmd->resp[0] & R1_OUT_OF_RANGE)
				&& (mrq->data) &&
				(is_card_last_block(pdata, cmd->arg,
					mrq->data->blocks))) {
			cmd->resp[0] &= (~R1_OUT_OF_RANGE);
			/* clear the error */
		}
	}
}

/*copy buffer from data->sg to dma buffer, set dma addr to reg*/
void aml_sdio_prepare_dma(struct amlsd_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;

	if (data->flags & MMC_DATA_WRITE) {
		aml_sg_copy_buffer(data->sg, data->sg_len,
				host->bn_buf, data->blksz*data->blocks, 1);
		pr_debug("W Cmd %d, %x-%x-%x-%x\n",
				mrq->cmd->opcode,
				host->bn_buf[0], host->bn_buf[1],
				host->bn_buf[2], host->bn_buf[3]);
	}
	/* host->dma_addr = host->bn_dma_buf; */
}

void aml_sdio_set_port_ios(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 vconf = readl(host->base + SDIO_CONF);
	struct sdio_config *conf = (void *)&vconf;

	if (aml_card_type_sdio(pdata)
			&& (pdata->mmc->actual_clock > 50000000)) {
		/* if > 50MHz */
		conf->data_latch_at_negedge = 1; /* [19] //0 */
		conf->do_not_delay_data = 1; /* [18] */
		conf->cmd_out_at_posedge = 0;
	} else {
		conf->data_latch_at_negedge = 0; /* [19] //0 */
		conf->do_not_delay_data = 0; /* [18] */
		conf->cmd_out_at_posedge = 0;
	}
	writel(vconf, host->base+SDIO_CONF);
	if ((conf->cmd_clk_divide == pdata->clkc)
			&& (conf->bus_width == pdata->width))
		return;

	/*Setup Clock*/
	conf->cmd_clk_divide = pdata->clkc;
	/*Setup Bus Width*/
	conf->bus_width = pdata->width;
	writel(vconf, host->base+SDIO_CONF);
}

static void aml_sdio_enable_irq(struct mmc_host *mmc, int enable)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 virqc;
	struct sdio_irq_config *irqc;
	u32 virqs;
	struct sdio_status_irq *irqs;
	u32 vmult;
	struct sdio_mult_config *mult;
	unsigned long flags;

	if (host->xfer_step == XFER_START
			|| host->xfer_step == XFER_AFTER_START) {
		return;

	}
	if (enable) {
		spin_lock_irqsave(&host->mrq_lock, flags);
		if (host->xfer_step == XFER_START
				|| host->xfer_step == XFER_AFTER_START) {
			/* pr_info("cmd irq is running
			 * when aml_sdio_enable_irq()
			 * enable = %d\n", enable);
			 */
			/* pr_info("irqs->sdio_cmd_int = %d\n",
			 * irqs->sdio_cmd_int );
			 */
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			return;
		}
		virqc = readl(host->base + SDIO_IRQC);
		irqc = (void *)&virqc;
		virqs = readl(host->base + SDIO_IRQS);
		irqs = (void *)&virqs;
		vmult = readl(host->base + SDIO_MULT);
		mult = (void *)&vmult;

		/* u32 vmult = readl(host->base + SDIO_MULT); */
		/* struct sdio_mult_config* mult = (void*)&vmult; */

		/* enable if int irq */
		irqc->arc_if_int_en = 1;
		irqs->sdio_if_int = 1;

		mult->sdio_port_sel = pdata->port;
		writel(vmult, host->base + SDIO_MULT);
		writel(virqs, host->base + SDIO_IRQS);
		writel(virqc, host->base + SDIO_IRQC);
		spin_unlock_irqrestore(&host->mrq_lock, flags);
	} else {

		virqc = readl(host->base + SDIO_IRQC);
		irqc = (void *)&virqc;
		virqs = readl(host->base + SDIO_IRQS);

		irqs = (void *)&virqs;
		vmult = readl(host->base + SDIO_MULT);
		mult = (void *)&vmult;

		irqc->arc_if_int_en = 0;
		irqs->sdio_if_int = 1;

		mult->sdio_port_sel = pdata->port;
		writel(vmult, host->base + SDIO_MULT);

		writel(virqs, host->base + SDIO_IRQS);
		writel(virqc, host->base + SDIO_IRQC);
	}
}

/*set to register, start xfer*/
void aml_sdio_start_cmd(struct mmc_host *mmc, struct mmc_request *mrq)
{
	u32 pack_size;
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	struct cmd_send send = {0};
	struct sdio_extension ext = {0};
	u32 virqc = readl(host->base + SDIO_IRQC);
	struct sdio_irq_config *irqc = (void *)&virqc;
	u32 virqs = readl(host->base + SDIO_IRQS);
	struct sdio_status_irq *irqs = (void *)&virqs;
	u32 vmult = readl(host->base + SDIO_MULT);
	struct sdio_mult_config *mult = (void *)&vmult;

	switch (mmc_resp_type(mrq->cmd)) {
	case MMC_RSP_R1:
	case MMC_RSP_R1B:
	case MMC_RSP_R3:
		/*7(cmd)+32(respnse)+7(crc)-1 data*/
		send.cmd_response_bits = 45;
		break;
	case MMC_RSP_R2:
		/* 7(cmd)+120(respnse)+7(crc)-1 data */
		send.cmd_response_bits = 133;
		send.response_crc7_from_8 = 1;
		break;
	default:
		/*no response*/
		break;
	}

	if (!(mrq->cmd->flags & MMC_RSP_CRC))
		send.response_do_not_have_crc7 = 1;

	if (mrq->cmd->flags & MMC_RSP_BUSY)
		send.check_busy_on_dat0 = 1;

	/* clear here */
	timeout_cmd_cnt = 0;

	if (mrq->data) {
		/*total package num*/
		send.repeat_package_times = mrq->data->blocks - 1;
		WARN_ON(mrq->data->blocks > 256);
		/*package size*/
		if (pdata->width) /*0: 1bit, 1: 4bit*/
			pack_size = mrq->data->blksz*8 + (16-1)*4;
		else
			pack_size = mrq->data->blksz*8 + (16-1);
		ext.data_rw_number = pack_size;
		if (mrq->data->flags & MMC_DATA_WRITE)
			send.cmd_send_data = 1;
		else
			send.response_have_data = 1;
	}
	/*cmd index*/
	send.cmd_command = 0x40|mrq->cmd->opcode;

	aml_sdio_soft_reset(host);

	/*enable cmd irq*/
	irqc->arc_cmd_int_en = 1;

	/*clear pending*/
	irqs->sdio_cmd_int = 1;

	aml_sdio_set_port_ios(host->mmc);

	mult->sdio_port_sel = pdata->port;
	vmult |= (1<<31);
	writel(vmult, host->base + SDIO_MULT);
	writel(virqs, host->base + SDIO_IRQS);
	writel(virqc, host->base + SDIO_IRQC);
	/* setup all reg to send cmd */
	writel(mrq->cmd->arg, host->base + SDIO_ARGU);
	writel(*(u32 *)&ext, host->base + SDIO_EXT);
	writel(*(u32 *)&send, host->base + SDIO_SEND);
}

/*
 * clear struct & call mmc_request_done
 */
void aml_sdio_request_done(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	unsigned long flags;
	struct mmc_command *cmd;

	if (delayed_work_pending(&host->timeout))
		cancel_delayed_work_sync(&host->timeout);

	spin_lock_irqsave(&host->mrq_lock, flags);
	WARN_ON(!host->mrq->cmd);
	WARN_ON(host->xfer_step == XFER_FINISHED);
	aml_sdio_read_response(pdata, host->mrq);

	cmd = host->mrq->cmd; /* for debug */
	host->mrq = NULL;
	host->xfer_step = XFER_FINISHED;
	spin_unlock_irqrestore(&host->mrq_lock, flags);

	if (cmd->flags & MMC_RSP_136) {
		pr_debug("Cmd %d ,Resp %x-%x-%x-%x\n",
				cmd->opcode, cmd->resp[0], cmd->resp[1],
				cmd->resp[2], cmd->resp[3]);
	} else if (cmd->flags & MMC_RSP_PRESENT) {
		pr_debug("Cmd %d ,Resp 0x%x\n",
				cmd->opcode, cmd->resp[0]);
	}

#ifdef CONFIG_MMC_AML_DEBUG
	host->req_cnt--;

	aml_dbg_verify_pinmux(pdata);
	aml_dbg_verify_pull_up(pdata);
#endif

	if (pdata->xfer_post)
		pdata->xfer_post(pdata);

	mmc_request_done(host->mmc, mrq);
}

static void aml_sdio_print_err(struct amlsd_host *host, char *msg)
{
	struct amlsd_platform *pdata = mmc_priv(host->mmc);
	u32 virqs = readl(host->base + SDIO_IRQS);
	u32 virqc = readl(host->base + SDIO_IRQC);
	u32 vconf = readl(host->base + SDIO_CONF);
	struct sdio_config *conf = (void *)&vconf;
	u32 clk_rate = clk_get_rate(host->core_clk) / 2;

	pr_err("%s: %s,", mmc_hostname(host->mmc), msg);
	pr_info("Cmd%d arg %#x,", host->mrq->cmd->opcode, host->mrq->cmd->arg);
	pr_info("Xfer %d Bytes,", host->mrq->data?host->mrq->data->blksz
			*host->mrq->data->blocks:0);
	pr_info("host->xfer_step=%d,", host->xfer_step);
	pr_info("host->cmd_is_stop=%d,", host->cmd_is_stop);
	pr_info("pdata->port=%d,", pdata->port);
	pr_info("virqs=%#0x, virqc=%#0x,", virqs, virqc);
	pr_info("conf->cmd_clk_divide=%d,", conf->cmd_clk_divide);
	pr_info("pdata->clkc=%d,", pdata->clkc);
	pr_info("conf->bus_width=%d,", conf->bus_width);
	pr_info("pdata->width=%d,", pdata->width);
	pr_info("conf=%#x, clock=%d\n", vconf,
			clk_rate / (conf->cmd_clk_divide + 1));
}

/*setup delayed workstruct in aml_sdio_request*/
static void aml_sdio_timeout(struct work_struct *work)
{
	static int timeout_cnt;
	struct amlsd_host *host =
		container_of(work, struct amlsd_host, timeout.work);
	u32 virqs;
	struct sdio_status_irq *irqs;
	u32 virqc;
	struct sdio_irq_config *irqc;
	unsigned long flags;
	struct amlsd_platform *pdata = mmc_priv(host->mmc);
	int is_mmc_stop = 0;
	unsigned long time_start_cnt = aml_read_cbus(ISA_TIMERE);


	time_start_cnt = (time_start_cnt - host->time_req_sta) / 1000;

	virqs = readl(host->base + SDIO_IRQS);
	irqs = (void *)&virqs;
	virqc = readl(host->base + SDIO_IRQC);
	irqc = (void *)&virqc;

	spin_lock_irqsave(&host->mrq_lock, flags);
	if (host->xfer_step == XFER_FINISHED) {
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		pr_err("timeout after xfer finished\n");
		return;
	}
	if ((irqs->sdio_cmd_int) /* irq have been occurred */
			|| (host->xfer_step == XFER_IRQ_OCCUR)) {
		/* isr have been run */
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		schedule_delayed_work(&host->timeout, msecs_to_jiffies(500));
		host->time_req_sta = aml_read_cbus(ISA_TIMERE);

		if (irqs->sdio_cmd_int) {
			timeout_cnt++;
			if (timeout_cnt > 30)
				goto timeout_handle;
			pr_err("%s: cmd%d,", mmc_hostname(host->mmc),
					host->mrq->cmd->opcode);
			pr_info("ISR have been run,");
			pr_info("xfer_step=%d,", host->xfer_step);
			pr_info("time_start_cnt=%ldmS,", time_start_cnt);
			pr_info("timeout_cnt=%d\n", timeout_cnt);
		} else
			pr_err("%s: isr have been run\n",
					mmc_hostname(host->mmc));
		return;
	}
	spin_unlock_irqrestore(&host->mrq_lock, flags);
timeout_handle:
	timeout_cnt = 0;

	if (!(irqc->arc_cmd_int_en)) {
		pr_err("%s: arc_cmd_int_en is not enable\n",
				mmc_hostname(host->mmc));
	}

	/* Disable Command-Done-Interrupt to avoid irq occurs
	 * It will be enabled again in the next cmd.
	 */
	irqc->arc_cmd_int_en = 0;   /* disable cmd irq */
	writel(virqc, host->base + SDIO_IRQC);

	spin_lock_irqsave(&host->mrq_lock, flags);

	/* do not retry for sdcard */
	if (!aml_card_type_mmc(pdata)) {
		sdio_error_flag |= (1<<30);
		host->mrq->cmd->retries = 0;
	} else if (((sdio_error_flag & (1<<3)) == 0)
			&& (host->mrq->data != NULL)
			&& pdata->is_in) {
		/* set cmd retry cnt when first error. */
		sdio_error_flag |= (1<<3);
		host->mrq->cmd->retries = AML_TIMEOUT_RETRY_COUNTER;
	}

	/* here clear error flags after error retried */
	if (sdio_error_flag && (host->mrq->cmd->retries == 0))
		sdio_error_flag |= (1<<30);

	host->xfer_step = XFER_TIMEDOUT;
	host->mrq->cmd->error = -ETIMEDOUT;
	spin_unlock_irqrestore(&host->mrq_lock, flags);

	pr_err("time_start_cnt:%ld\n", time_start_cnt);
	aml_sdio_print_err(host, "Timeout error");

#ifdef CONFIG_MMC_AML_DEBUG
	aml_dbg_verify_pinmux(pdata);
	aml_dbg_verify_pull_up(pdata);
	aml_sdio_print_reg(host);
	aml_dbg_print_pinmux();
#endif

	if (host->mrq->stop && aml_card_type_mmc(pdata) && !host->cmd_is_stop) {
		/* pr_err("Send stop cmd before timeout retry..\n"); */
		spin_lock_irqsave(&host->mrq_lock, flags);
		aml_sdio_send_stop(host);
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		is_mmc_stop = 1;
		schedule_delayed_work(&host->timeout, 50);
	} else {
		if (host->cmd_is_stop)
			host->cmd_is_stop = 0;
		aml_sdio_request_done(host->mmc, host->mrq);
	}
}

/*
 * aml handle request
 * 1. setup data
 * 2. send cmd
 * 3. return (aml_sdio_request_done in irq function)
 */
void aml_sdio_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_platform *pdata;
	struct amlsd_host *host;
	unsigned long flags;
	unsigned int timeout;
	u32 virqc;
	struct sdio_irq_config *irqc;

	WARN_ON(!mmc);
	WARN_ON(!mrq);

	pdata = mmc_priv(mmc);
	host = (void *)pdata->host;

	virqc = readl(host->base + SDIO_IRQC);
	irqc = (void *)&virqc;

	if (aml_card_type_non_sdio(pdata)) {
		irqc->arc_if_int_en = 0;
		writel(virqc, host->base + SDIO_IRQC);
	}

	if (aml_check_unsupport_cmd(mmc, mrq))
		return;

	/* only for SDCARD hotplag */
	if ((!pdata->is_in
				|| (!host->init_flag
					&& aml_card_type_non_sdio(pdata)))
			&& (mrq->cmd->opcode != 0)) {
		spin_lock_irqsave(&host->mrq_lock, flags);
		mrq->cmd->error = -ENOMEDIUM;
		mrq->cmd->retries = 0;
		host->mrq = NULL;
		host->xfer_step = XFER_FINISHED;
		spin_unlock_irqrestore(&host->mrq_lock, flags);

		mmc_request_done(mmc, mrq);
		return;
	}

#ifdef CONFIG_MMC_AML_DEBUG
	if (host->req_cnt)
		pr_err("Reentry error! host->req_cnt=%d\n", host->req_cnt);
	host->req_cnt++;
#endif

	if (mrq->cmd->opcode == 0)
		host->init_flag = 1;

	pr_debug("%s: starting CMD%u arg %08x flags %08x\n",
			mmc_hostname(mmc), mrq->cmd->opcode,
			mrq->cmd->arg, mrq->cmd->flags);
	if (mrq->data) {
		/*Copy data to dma buffer for write request*/
		aml_sdio_prepare_dma(host, mrq);
		writel(host->bn_dma_buf, host->base + SDIO_ADDR);
		pr_debug("%s: blksz %d blocks %d flags %08x tsac %d ms nsac %d\n",
				mmc_hostname(mmc), mrq->data->blksz,
				mrq->data->blocks, mrq->data->flags,
				mrq->data->timeout_ns / 1000000,
				mrq->data->timeout_clks);
	}

	/*clear pinmux & set pinmux*/
	if (pdata->xfer_pre)
		pdata->xfer_pre(pdata);

#ifdef CONFIG_MMC_AML_DEBUG
	aml_dbg_verify_pull_up(pdata);
	aml_dbg_verify_pinmux(pdata);
#endif

	if (!mrq->data)
		timeout = 1000;
	else
		timeout = 5000;
	/* 5s */
	if (mrq->cmd->opcode == MMC_ERASE) /* maybe over 30S for erase cmd. */
		timeout = 30000;

	schedule_delayed_work(&host->timeout, msecs_to_jiffies(timeout));

	CMD_PROCESS_JIT = timeout;
	spin_lock_irqsave(&host->mrq_lock, flags);
	if (SDIO_IRQ_SUPPORT)
		if ((mmc->caps & MMC_CAP_SDIO_IRQ)
				&& (mmc->ops->enable_sdio_irq))
			mmc->ops->enable_sdio_irq(mmc, 0);

	if (host->xfer_step != XFER_FINISHED
			&& host->xfer_step != XFER_INIT)
		pr_err("host->xfer_step %d\n", host->xfer_step);

	/* clear error flag if last command retried failed here */
	if (sdio_error_flag & (1<<30))
		sdio_error_flag = 0;

	host->mrq = mrq;
	host->mmc = mmc;
	host->xfer_step = XFER_START;
	host->opcode = mrq->cmd->opcode;
	host->arg = mrq->cmd->arg;
	host->time_req_sta = aml_read_cbus(ISA_TIMERE);

	aml_sdio_start_cmd(mmc, mrq);
	host->xfer_step = XFER_AFTER_START;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
}

struct mmc_command aml_sdio_cmd = {
	.opcode = MMC_STOP_TRANSMISSION,
	.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC,
};
struct mmc_request aml_sdio_stop = {
	.cmd = &aml_sdio_cmd,
};

static void aml_sdio_send_stop(struct amlsd_host *host)
{
	/*Already in mrq_lock*/
	host->cmd_is_stop = 1;
	sdio_err_bak = host->mrq->cmd->error;
	host->mrq->cmd->error = 0;
	aml_sdio_start_cmd(host->mmc, &aml_sdio_stop);
}

/*
 * enable cmd & data irq, call tasket, do aml_sdio_request_done
 */
static irqreturn_t aml_sdio_irq(int irq, void *dev_id)
{
	struct amlsd_host *host = (void *)dev_id;
	u32 virqs = readl(host->base + SDIO_IRQS);
	struct sdio_status_irq *irqs = (void *)&virqs;
	struct mmc_request *mrq;
	unsigned long flags;
	int sdio_cmd_int = 0;

	spin_lock_irqsave(&host->mrq_lock, flags);
	mrq = host->mrq;
	if (!mrq && !irqs->sdio_if_int) {

		if (host->xfer_step == XFER_FINISHED ||
				host->xfer_step == XFER_TIMEDOUT){
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			return IRQ_HANDLED;
		}
		WARN_ON(!mrq);
		/*	aml_sdio_print_reg(host);*/
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		return IRQ_HANDLED;
	}

	if (irqs->sdio_cmd_int  && mrq) {
		if (host->cmd_is_stop)
			host->xfer_step = XFER_IRQ_TASKLET_BUSY;
		else
			host->xfer_step = XFER_IRQ_OCCUR;
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		if ((SDIO_IRQ_SUPPORT)
				&& !(irqs->sdio_if_int)
				&& (host->mmc->sdio_irq_pending != true))
			host->mmc->ops->enable_sdio_irq(host->mmc, 1);
		if (irqs->sdio_if_int && SDIO_IRQ_SUPPORT)
			sdio_cmd_int = 1;
		else
			return IRQ_WAKE_THREAD;
	} else
		spin_unlock_irqrestore(&host->mrq_lock, flags);

	if (irqs->sdio_if_int) {
		if ((host->mmc->sdio_irq_thread)
			&& (!atomic_read(&host->mmc->sdio_irq_thread_abort)))
			mmc_signal_sdio_irq(host->mmc);
	}

	if (SDIO_IRQ_SUPPORT && sdio_cmd_int)
		return IRQ_WAKE_THREAD;

	/* if cmd has stop, call aml_sdio_send_stop */
	return IRQ_HANDLED;
}

irqreturn_t aml_sdio_irq_thread(int irq, void *data)
{
	struct amlsd_host *host = (void *)data;
	u32 virqs = readl(host->base + SDIO_IRQS);
	struct sdio_status_irq *irqs = (void *)&virqs;
	u32 vsend = readl(host->base + SDIO_SEND);
	struct cmd_send *send = (void *)&vsend;
	unsigned long flags;
	struct mmc_request *mrq;
	enum aml_mmc_waitfor	xfer_step;
	struct amlsd_platform *pdata = mmc_priv(host->mmc);

	spin_lock_irqsave(&host->mrq_lock, flags);
	mrq = host->mrq;
	xfer_step = host->xfer_step;

	if ((xfer_step == XFER_FINISHED) || (xfer_step == XFER_TIMER_TIMEOUT)) {
		pr_err("Warning: xfer_step=%d\n", xfer_step);
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		return IRQ_HANDLED;
	}

	if (!mrq) {
		pr_err("CMD%u, arg %08x, mrq NULL xfer_step %d\n",
				host->opcode, host->arg, xfer_step);
		if (xfer_step == XFER_FINISHED ||
				xfer_step == XFER_TIMEDOUT){
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			pr_err("[aml_sdio_irq_thread] out\n");
			return IRQ_HANDLED;
		}
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		return IRQ_HANDLED;
	}

	if ((SDIO_IRQ_SUPPORT)
			&& (host->xfer_step == XFER_TASKLET_DATA)) {
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		return IRQ_HANDLED;
	}

	if (host->cmd_is_stop) {
		host->cmd_is_stop = 0;
		mrq->cmd->error = sdio_err_bak;
		spin_unlock_irqrestore(&host->mrq_lock, flags);
		aml_sdio_request_done(host->mmc, mrq);
		return IRQ_HANDLED;
	}
	host->xfer_step = XFER_TASKLET_DATA;

	if (!mrq->data) {
		if (irqs->sdio_response_crc7_ok
				|| send->response_do_not_have_crc7) {
			mrq->cmd->error = 0;
			spin_unlock_irqrestore(&host->mrq_lock, flags);
		} else {
			mrq->cmd->error = -EILSEQ;
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			aml_sdio_print_err(host, "cmd crc7 error");
		}
		aml_sdio_request_done(host->mmc, mrq);
	} else{
		if (irqs->sdio_data_read_crc16_ok
				|| irqs->sdio_data_write_crc16_ok) {
			mrq->cmd->error = 0;
			spin_unlock_irqrestore(&host->mrq_lock, flags);
		} else {
			mrq->cmd->error = -EILSEQ;
			if ((sdio_error_flag == 0)
					&& aml_card_type_mmc(pdata)) {
				/* set cmd retry cnt when first error. */
				sdio_error_flag |= (1<<0);
				mrq->cmd->retries = AML_ERROR_RETRY_COUNTER;
			}
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			aml_sdio_print_err(host, "data crc16 error");
		}
		spin_lock_irqsave(&host->mrq_lock, flags);
		mrq->data->bytes_xfered = mrq->data->blksz*mrq->data->blocks;

		if ((mrq->cmd->error == 0) || (sdio_error_flag
					&& (mrq->cmd->retries == 0))) {
			sdio_error_flag |= (1<<30);
		}

		spin_unlock_irqrestore(&host->mrq_lock, flags);
		if (mrq->data->flags & MMC_DATA_READ) {
			aml_sg_copy_buffer(mrq->data->sg, mrq->data->sg_len,
					host->bn_buf,
					mrq->data->blksz*mrq->data->blocks, 0);
			pr_debug("R Cmd %d, %x-%x-%x-%x\n",
					host->mrq->cmd->opcode,
					host->bn_buf[0], host->bn_buf[1],
					host->bn_buf[2], host->bn_buf[3]);
		}
		spin_lock_irqsave(&host->mrq_lock, flags);
		if (mrq->stop) {
			aml_sdio_send_stop(host);
			spin_unlock_irqrestore(&host->mrq_lock, flags);
		} else {
			spin_unlock_irqrestore(&host->mrq_lock, flags);
			aml_sdio_request_done(host->mmc, mrq);
		}
	}
	return IRQ_HANDLED;
}

/*
 * 1. clock valid range
 * 2. clk config enable
 * 3. select clock source
 * 4. set clock divide
 */
static void aml_sdio_set_clk_rate(struct amlsd_platform *pdata, u32 clk_ios)
{
	struct amlsd_host *host = (void *)pdata->host;
	u32 vconf = readl(host->base + SDIO_CONF);
	struct sdio_config *conf = (void *)&vconf;
	u32 clk_rate = clk_get_rate(host->core_clk) / 2; /* tmp for 3.10 */
	u32 clk_div;

	if (clk_ios > pdata->f_max)
		clk_ios = pdata->f_max;
	if (clk_ios < pdata->f_min)
		clk_ios = pdata->f_min;

	WARN_ON(!clk_ios);

	/*0: dont set it, 1:div2, 2:div3, 3:div4...*/
	clk_div = clk_rate / clk_ios - !(clk_rate%clk_ios);

	if (aml_card_type_sdio(pdata)
			&& (pdata->f_max > 50000000)) /* if > 50MHz */
		clk_div = 0;

	conf->cmd_clk_divide = clk_div;
	pdata->clkc = clk_div;
	pdata->mmc->actual_clock = clk_rate / (clk_div + 1);
	writel(vconf, host->base + SDIO_CONF);
	pr_debug("Clk IOS %d, Clk Src %d, Host Max Clk %d, clk_divide=%d\n",
			clk_ios, (clk_rate*2), pdata->f_max, clk_div);
}

static void aml_sdio_set_bus_width(struct amlsd_platform *pdata, u32 busw_ios)
{
	u32 bus_width = 0;
	struct amlsd_host *host = (void *)pdata->host;
	u32 vconf = readl(host->base + SDIO_CONF);
	struct sdio_config *conf = (void *)&vconf;

	switch (busw_ios) {
	case MMC_BUS_WIDTH_1:
		bus_width = 0;
		break;
	case MMC_BUS_WIDTH_4:
		bus_width = 1;
		break;
	case MMC_BUS_WIDTH_8:
	default:
		pr_err("SDIO Controller Can Not Support 8bit Data Bus\n");
		break;
	}

	conf->bus_width = bus_width;
	pdata->width = bus_width;
	writel(vconf, host->base + SDIO_CONF);
	pr_debug("Bus Width Ios %d\n", bus_width);
}

static void aml_sdio_set_power(struct amlsd_platform *pdata, u32 power_mode)
{
	switch (power_mode) {
	case MMC_POWER_ON:
		if (pdata->pwr_pre)
			pdata->pwr_pre(pdata);
		if (pdata->pwr_on)
			pdata->pwr_on(pdata);
		break;
	case MMC_POWER_UP:
		break;
	case MMC_POWER_OFF:
	default:
		if (pdata->pwr_pre)
			pdata->pwr_pre(pdata);
		if (pdata->pwr_off)
			pdata->pwr_off(pdata);
		break;
	}
}

/* Routine to configure clock values. Exposed API to core */
static void aml_sdio_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);

	if (!pdata->is_in)
		return;

	/*set power*/
	aml_sdio_set_power(pdata, ios->power_mode);

	/*set clock*/
	if (ios->clock)
		aml_sdio_set_clk_rate(pdata, ios->clock);

	/*set bus width*/
	aml_sdio_set_bus_width(pdata, ios->bus_width);
#if 1
	if (ios->chip_select == MMC_CS_HIGH)
		aml_cs_high(mmc);
	else if (ios->chip_select == MMC_CS_DONTCARE)
		aml_cs_dont_care(mmc);
#endif
}

static int aml_sdio_get_ro(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	u32 ro = 0;

	if (pdata->ro)
		ro = pdata->ro(pdata);
	return ro;
}

int aml_sdio_get_cd(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);

	return pdata->is_in; /* 0: no inserted  1: inserted */
}

#if 0/* def CONFIG_PM */
static int aml_sdio_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	int i;
	struct amlsd_host *host = platform_get_drvdata(pdev);
	struct mmc_host *mmc;
	struct amlsd_platform *pdata;

	pr_info("***Entered %s:%s\n", __FILE__, __func__);
	i = 0;
	list_for_each_entry(pdata, &host->sibling, sibling) {
		mmc = pdata->mmc;
		/* mmc_power_save_host(mmc); */
		ret = mmc_suspend_host(mmc);
		if (ret)
			break;
		i++;
	}

	if (ret) {
		list_for_each_entry(pdata, &host->sibling, sibling) {
			i--;
			if (i < 0)
				break;

			if (!(pdata->caps & MMC_CAP_NONREMOVABLE))
				aml_sd_uart_detect(pdata);

			mmc = pdata->mmc;
			mmc_resume_host(mmc);
		}
	}
	pr_info("***Exited %s:%s\n", __FILE__, __func__);

	return ret;
}

static int aml_sdio_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct amlsd_host *host = platform_get_drvdata(pdev);
	struct mmc_host *mmc;
	struct amlsd_platform *pdata;

	pr_info("***Entered %s:%s\n", __FILE__, __func__);
	list_for_each_entry(pdata, &host->sibling, sibling) {

		/* detect if a card is exist or not if it is removable */
		if (!(pdata->caps & MMC_CAP_NONREMOVABLE))
			aml_sd_uart_detect(pdata);

		mmc = pdata->mmc;
		/* mmc_power_restore_host(mmc); */
		ret = mmc_resume_host(mmc);
		if (ret)
			break;
	}
	pr_info("***Exited %s:%s\n", __FILE__, __func__);
	return ret;
}
#else
#define aml_sdio_suspend	NULL
#define aml_sdio_resume		NULL
#endif

static const struct mmc_host_ops aml_sdio_ops = {
	.request = aml_sdio_request,
	.set_ios = aml_sdio_set_ios,
	.enable_sdio_irq = aml_sdio_enable_irq,
	.get_cd = aml_sdio_get_cd,
	.get_ro = aml_sdio_get_ro,
	.hw_reset = aml_emmc_hw_reset,
};

static ssize_t sdio_debug_func(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	count = kstrtoint(buf, 0, &sdio_debug_flag);
	pr_info("sdio_debug_flag: %d\n", sdio_debug_flag);

	return count;
}

static ssize_t show_sdio_debug(struct class *class,
		struct class_attribute *attr,	char *buf)
{
	pr_info("sdio_debug_flag: %d\n", sdio_debug_flag);
	pr_info("1 : Force sdio cmd crc error\n");
	pr_info("2 : Force sdio data crc error\n");
	pr_info("9 : Force sdio irq timeout error\n");

	return 0;
}

static struct class_attribute sdio_class_attrs[] = {
	__ATTR(debug, 0644, show_sdio_debug, sdio_debug_func),
	__ATTR_NULL
};

static struct amlsd_host *aml_sdio_init_host(struct amlsd_host *host)
{
	if (request_threaded_irq(host->irq,
				(irq_handler_t)aml_sdio_irq,
				aml_sdio_irq_thread,
				IRQF_SHARED, "sdio", (void *)host)) {
		pr_err("Request SDIO Irq Error!\n");
		return NULL;
	}

	host->bn_buf = dma_alloc_coherent(NULL, SDIO_BOUNCE_REQ_SIZE,
			&host->bn_dma_buf, GFP_KERNEL);
	if (host->bn_buf == NULL) {
		pr_err("Dma alloc Fail!\n");
		return NULL;
	}
	INIT_DELAYED_WORK(&host->timeout, aml_sdio_timeout);

	spin_lock_init(&host->mrq_lock);
	mutex_init(&host->pinmux_lock);
	host->xfer_step = XFER_INIT;

	INIT_LIST_HEAD(&host->sibling);

	host->version = AML_MMC_VERSION;
	/*host->storage_flag = storage_flag;*/
	host->pinctrl = NULL;

	host->init_flag = 1;

#ifdef CONFIG_MMC_AML_DEBUG
	host->req_cnt = 0;
	pr_err("CONFIG_MMC_AML_DEBUG is on!\n");
#endif

	host->debug.name =
		kzalloc(strlen((const char *)AML_SDIO_MAGIC)+1, GFP_KERNEL);
	strcpy((char *)(host->debug.name), (const char *)AML_SDIO_MAGIC);
	host->debug.class_attrs = sdio_class_attrs;
	if (class_register(&host->debug))
		pr_info(" class register nand_class fail!\n");

	return host;
}

static int aml_sdio_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc = NULL;
	struct amlsd_host *host = NULL;
	struct amlsd_platform *pdata;
	struct resource *res_mem;
	int size;
	int ret = 0, i;

	pr_info("%s() begin!\n", __func__);

	host = kzalloc(sizeof(struct amlsd_host), GFP_KERNEL);
	if (!host)
		return -ENODEV;

	aml_mmc_ver_msg_show();

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		pr_info("error to get IORESOURCE\n");
		goto fail_init_host;
	}
	size = resource_size(res_mem);

	host->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	pr_info("host->irq = %d\n", host->irq);
	host->base = devm_ioremap_nocache(&pdev->dev, res_mem->start, size);

	aml_sdio_init_host(host);
	/* if (amlsd_get_reg_base(pdev, host))
	 *	goto fail_init_host;
	 */

	host->pdev = pdev;
	host->dev = &pdev->dev;
	platform_set_drvdata(pdev, host);

	host->data = (struct meson_mmc_data *)
		of_device_get_match_data(&pdev->dev);
	if (!host->data) {
		ret = -EINVAL;
		goto probe_free_host;
	}

	if (host->data->pinmux_base)
		host->pinmux_base = ioremap(host->data->pinmux_base, 0x200);

	/* init sdio reg here */
	aml_sdio_init_param(host);

	//	for (i = 0; i < MMC_MAX_DEVICE; i++) {
	for (i = 0; i < 1; i++) {
		/*malloc extra amlsd_platform*/
		mmc = mmc_alloc_host(sizeof(struct amlsd_platform), &pdev->dev);
		if (!mmc) {
			ret = -ENOMEM;
			goto probe_free_host;
		}

		pdata = mmc_priv(mmc);
		memset(pdata, 0, sizeof(struct amlsd_platform));
		if (amlsd_get_platform_data(pdev, pdata, mmc, i)) {
			mmc_free_host(mmc);
			break;
		}
#if 0
		/* if(pdata->parts){ */
		if (pdata->port == PORT_SDIO_C) {
			if (is_emmc_exist(host)) {
				mmc->is_emmc_port = 1;
				/* add_part_table(pdata->parts,*/
				/*pdata->nr_parts);*/
				/* mmc->add_part = add_emmc_partition; */
			} else { /* there is not eMMC/tsd */
				pr_info(
						"[%s]:not eMMC/tsd,skip sdio_c dts config!\n",
						__func__);
				i++; /* skip the port written in the dts */
				memset(pdata, 0, sizeof(struct amlsd_platform));
				if (amlsd_get_platform_data(pdev,
							pdata, mmc, i)) {
					mmc_free_host(mmc);
					break;
				}
			}
		}
#endif
		dev_set_name(&mmc->class_dev, "%s", pdata->pinname);
		if (pdata->caps & MMC_CAP_NONREMOVABLE)
			pdata->is_in = true;

		if (pdata->caps & MMC_PM_KEEP_POWER)
			mmc->pm_caps |= MMC_PM_KEEP_POWER;

		if (pdata->caps & MMC_CAP_SDIO_IRQ)
			SDIO_IRQ_SUPPORT = 1;

		pdata->host = host;
		pdata->mmc = mmc;
		pdata->is_fir_init = true;

		/*	mmc->index = i;*/
		/*	mmc->alldev_claim = &aml_sdio_claim;*/
		mmc->ops = &aml_sdio_ops;
		mmc->ios.clock = 400000;
		mmc->ios.bus_width = MMC_BUS_WIDTH_1;
		mmc->max_blk_count = 4095;
		mmc->max_blk_size = 4095;
		mmc->max_req_size = pdata->max_req_size;
		mmc->max_seg_size = mmc->max_req_size;
		mmc->max_segs = 1024;
		mmc->ocr_avail = pdata->ocr_avail;
		/*	mmc->ocr = pdata->ocr_avail;*/
		mmc->caps = pdata->caps;
		mmc->caps2 = pdata->caps2;
		mmc->f_min = pdata->f_min;
		mmc->f_max = pdata->f_max;

		if (aml_card_type_sdio(pdata)) /* if sdio_wifi */
			mmc->rescan_entered = 1;
		/* do NOT run mmc_rescan for the first time */
		else
			mmc->rescan_entered = 0;

		if (pdata->port_init)
			pdata->port_init(pdata);
		/*	aml_sduart_pre(pdata);*/

		ret = mmc_add_host(mmc);
		if (ret) { /* error */
			pr_err("Failed to add mmc host.\n");
			goto probe_free_host;
		} else { /* ok */
			if (aml_card_type_sdio(pdata)) { /* if sdio_wifi */
				sdio_host = mmc;
				/* mmc->rescan_entered = 1;  */
				/* do NOT run mmc_rescan for the first time */
			}
		}

		/*	aml_sdio_init_debugfs(mmc);*/
		/*Add each mmc host pdata to this controller host list*/
		INIT_LIST_HEAD(&pdata->sibling);
		list_add_tail(&pdata->sibling, &host->sibling);

		/*Register card detect irq : plug in & unplug*/
		if (pdata->gpio_cd
				&& aml_card_type_non_sdio(pdata)) {
			mutex_init(&pdata->in_out_lock);
#ifdef CARD_DETECT_IRQ
			pdata->irq_init(pdata);
			ret = request_threaded_irq(pdata->irq_cd,
					aml_sd_irq_cd, aml_irq_cd_thread,
					IRQF_TRIGGER_RISING
					| IRQF_TRIGGER_FALLING
					| IRQF_ONESHOT,
					"sdio_mmc_cd", pdata);
			if (ret) {
				pr_err("Failed to request SD IN detect\n");
				goto probe_free_host;
			}
#else
			INIT_DELAYED_WORK(&pdata->cd_detect,
					meson_mmc_cd_detect);
			schedule_delayed_work(&pdata->cd_detect, 50);
#endif
		}
	}

	pr_info("%s() success!\n", __func__);
	return 0;

probe_free_host:
	list_for_each_entry(pdata, &host->sibling, sibling) {
		mmc = pdata->mmc;
		mmc_remove_host(mmc);
		mmc_free_host(mmc);
	}
fail_init_host:
	iounmap(host->base);
	free_irq(host->irq, host);
	dma_free_coherent(NULL, SDIO_BOUNCE_REQ_SIZE, host->bn_buf,
			(dma_addr_t)host->bn_dma_buf);
	kfree(host);
	pr_info("aml_sdio_probe() fail!\n");
	return ret;
	}

	int aml_sdio_remove(struct platform_device *pdev)
	{
		struct amlsd_host *host = platform_get_drvdata(pdev);
		struct mmc_host *mmc;
		struct amlsd_platform *pdata;

		dma_free_coherent(NULL, SDIO_BOUNCE_REQ_SIZE, host->bn_buf,
				(dma_addr_t)host->bn_dma_buf);

		free_irq(host->irq, host);
		iounmap(host->base);

		list_for_each_entry(pdata, &host->sibling, sibling) {
			mmc = pdata->mmc;
			mmc_remove_host(mmc);
			mmc_free_host(mmc);
		}

		/*	aml_devm_pinctrl_put(host);*/

		kfree(host);
		return 0;
	}

	static struct meson_mmc_data mmc_data_m8b = {
		.chip_type = MMC_CHIP_M8B,
		.pinmux_base = 0xc1108000,
	};

	static const struct of_device_id aml_sdio_dt_match[] = {
		{
			.compatible = "amlogic, aml_sdio",
			.data = &mmc_data_m8b,
		},
		{},
	};

	MODULE_DEVICE_TABLE(of, aml_sdio_dt_match);

	static struct platform_driver aml_sdio_driver = {
		.probe		 = aml_sdio_probe,
		.remove		= aml_sdio_remove,
		.suspend	= aml_sdio_suspend,
		.resume		= aml_sdio_resume,
		.driver		= {
			.name = "aml_sdio",
			.owner = THIS_MODULE,
			.of_match_table = aml_sdio_dt_match,
		},
	};

	static int __init aml_sdio_init(void)
	{
		return platform_driver_register(&aml_sdio_driver);
	}

	static void __exit aml_sdio_cleanup(void)
	{
		platform_driver_unregister(&aml_sdio_driver);
	}

	module_init(aml_sdio_init);
	module_exit(aml_sdio_cleanup);

	MODULE_DESCRIPTION("Amlogic SDIO Controller driver");
	MODULE_LICENSE("GPL");

