/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _HCI_OPS_OS_C_

#include <drv_conf.h>
#include <osdep_service.h>
#include <osdep_intf.h>

#include <linux/mmc/sdio_func.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>

//#define DEBUG_IO 1

u8 sdbus_direct_read8(struct intf_priv *pintfpriv, u32 addr)
{
	u8 readv8;
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;

_func_enter_;

	/* read in 1 byte synchronously */
	sdio_claim_host(func);
#ifdef DEBUG_IO
{
	int err;
	readv8 = sdio_readb(func, (addr|0x8000) & 0x1FFFF, &err);
	if (err)
		printk(KERN_CRIT "%s: FAIL!(%d) addr=0x%05x\n", __func__, err, addr);
}
#else
	readv8 = sdio_readb(func, (addr|0x8000) & 0x1FFFF, NULL);
#endif
	sdio_release_host(func);

_func_exit_;

	return  readv8;
}

void sdbus_direct_write8(struct intf_priv *pintfpriv, u32 addr, u8 val8)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;

_func_enter_;

	/* write out 1 byte synchronously */
	sdio_claim_host(func); 
#ifdef DEBUG_IO
{
	int err;
	sdio_writeb(func, val8, (addr|0x8000) & 0x1FFFF, &err);
	if (err)
		printk(KERN_CRIT "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, err, addr, val8);
}
#else
	sdio_writeb(func, val8, (addr|0x8000) & 0x1FFFF, NULL);
#endif
	sdio_release_host(func);

_func_exit_;

	return;
}

u8 sdbus_cmd52r(struct intf_priv *pintfpriv, u32 addr)
{
	u8 readv8;
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;

_func_enter_;

	sdio_claim_host(func); 
#ifdef DEBUG_IO
{
	int err;
	readv8 = sdio_f0_readb(func, addr & 0x1FFFF, &err);
	if (err)
		printk(KERN_CRIT "%s: FAIL!(%d) addr=0x%05x\n", __func__, err, addr);
}
#else
	readv8 = sdio_f0_readb(func, addr & 0x1FFFF, NULL);
#endif
	sdio_release_host(func);

_func_exit_;

	return  readv8;
}

void sdbus_cmd52w(struct intf_priv *pintfpriv, u32 addr, u8 val8)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;

_func_enter_;

	sdio_claim_host(func); 
#ifdef DEBUG_IO
{
	int err;
	sdio_f0_writeb(func, val8, addr & 0x1FFFF, &err);
//	err = mmc_io_rw_direct(func->card, 1, 0, addr, val8, NULL);
	if (err)
		printk(KERN_CRIT "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, err, addr, val8);
}
#else
	sdio_f0_writeb(func, val8, addr & 0x1FFFF, NULL);
#endif
	sdio_release_host(func);

_func_exit_;

	return;
}

/*
 * This function MUST be called after sdio_claim_host() or
 * in SDIO ISR(host had been claimed).
 */
s32 _sdbus_read_reg(struct intf_priv *pintfpriv, u32 addr, u32 cnt, void *pdata)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;
	s32 status;
#ifdef CONFIG_IO_4B
	u32 addr_offset = 0;
	u32 cnt_org = cnt;
	void *pdata_org = pdata;
#endif

_func_enter_;

#ifdef CONFIG_IO_CMD52
	if (cnt <= 2) {
		// prevent 4 bytes access problem
		s32 i = 0;
		for (i = 0; i < cnt; i++) {
			*(u8*)(pdata+i) = sdio_readb(func, (addr+i)&0x1FFFF, &status);
			if (status) break;
		}
		if (status) return _FAIL;
		return _SUCCESS;
	}
#endif

#ifdef CONFIG_IO_4B
#ifdef CONFIG_IO_4B_ADDR
	addr_offset = addr % 4;
	if (addr_offset) {
		addr -= addr_offset;
		cnt += addr_offset;
	}
#endif

	if (cnt % 4)
		cnt = ((cnt >> 2) + 1) << 2;
	if (cnt != cnt_org) {
		pdata = _malloc(cnt);
		if (pdata == NULL) {
			printk(KERN_CRIT "%s: _malloc FAIL! ADDR=%#x Size=%d\n", __func__, addr, cnt);
//			RT_TRACE(_module_hci_ops_os_c_, _drv_emerg_,
//				 ("sdbus_read_reg: _malloc fail\n"));
			return _FAIL; // SDIO_STATUS_NO_RESOURCES
		}
	}
#endif

	status = sdio_memcpy_fromio(func, pdata, addr&0x1FFFF, cnt);
	if (status) {
		printk(KERN_CRIT "%s: read FAIL(%d)! ADDR=%#x Size=%d\n", __func__, status, addr, cnt);
//		RT_TRACE(_module_hci_ops_os_c_, _drv_emerg_,
//			 ("sdbus_read_reg: error %d\n"
//			  "***** Addr = %#x *****\n"
//			  "***** Length = %d *****\n", status, addr, cnt));

#ifdef CONFIG_IO_4B
		if (cnt != cnt_org) _mfree(pdata, cnt);
#endif
		return _FAIL;
	}

#ifdef CONFIG_IO_4B
	if (cnt != cnt_org) {
		_memcpy(pdata_org, pdata + addr_offset, cnt_org);
		_mfree(pdata, cnt);
	}
#endif

_func_exit_;

	return _SUCCESS;
}

uint sdbus_read_reg(struct intf_priv *pintfpriv, u32 addr, u32 cnt, void *pdata)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;
	s32 ret;


	sdio_claim_host(func);
	ret = _sdbus_read_reg(pintfpriv, addr, cnt, pdata);
	sdio_release_host(func);

	return ret;
}

/*
 * This function MUST be called after sdio_claim_host() or
 * in SDIO ISR(host had been claimed).
 */
uint sdbus_read_bytes_to_recvbuf(struct intf_priv *pintfpriv, u32 addr, u32 cnt, u8 *pbuf)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;
	int ret;

_func_enter_;

//	RT_TRACE(_module_hci_ops_os_c_, _drv_info_, ("sdbus_read_bytes_to_recvbuf: addr=%#x cnt=%d\n",addr,cnt));
//	sdio_claim_host(func); // read rx FIFO always in interrupt, SDIO host had claim this
	ret = sdio_memcpy_fromio(func, pbuf, addr, cnt);
//	sdio_release_host(func);
	if (ret) {
		printk(KERN_CRIT "%s: ERROR(%#x[%d])\n", __func__, ret, ret);
//		RT_TRACE(_module_hci_ops_os_c_,_drv_emerg_,("sdbus_read_bytes_to_recvbuf: error(0x%x[%d])\n",ret,ret));
		return _FAIL;
	}

_func_exit_;

	return _SUCCESS;
}

/*
 * This function MUST be called after sdio_claim_host() or
 * in SDIO ISR(host had been claimed).
 */
uint sdbus_read_blocks_to_recvbuf(struct intf_priv *pintfpriv, u32 addr, u32 cnt, u8 *pbuf)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;
	int ret;

_func_enter_;

//	RT_TRACE(_module_hci_ops_os_c_, _drv_info_,
//		 ("+sdbus_read_blocks_to_recvbuf: addr=%#x cnt=%d\n", addr, cnt));

	if ((addr & 0xfffffffc) == ((DID_WLAN_FIFO << 15) | (OFFSET_RX_RX0FFQ << 2))) {
		struct recv_buf *precv_buf = (struct recv_buf*)pbuf;
		pbuf = precv_buf->pbuf;
		RT_TRACE(_module_hci_ops_os_c_, _drv_info_,
			 ("sdbus_read_blocks_to_recvbuf: precv_buf->pbuf=0x%p\n",
			  pbuf));
	}

//	sdio_claim_host(func); // read rx FIFO always in interrupt, SDIO host had claim this
	ret = sdio_memcpy_fromio(func, pbuf,addr,cnt);
//	sdio_release_host(func);
	if (ret) {
		printk(KERN_CRIT "%s: ERROR(%#x[%d])\n", __func__, ret, ret);
//		RT_TRACE(_module_hci_ops_os_c_, _drv_emerg_,
//			 ("sdbus_read_blocks_to_recvbuf: error(%d)\n", ret));
		return _FAIL;
	}

_func_exit_;

	return _SUCCESS;
}

/*
 * This function MUST be called after sdio_claim_host() or
 * in SDIO ISR(host had been claimed).
 */
s32 _sdbus_write_reg(struct intf_priv *pintfpriv, u32 addr, u32 cnt, void *pdata)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;
	s32 status;
#ifdef CONFIG_IO_4B
	u32 addr_offset = 0;
	u32 cnt_org = cnt;
	void *pdata_org = pdata;
#endif

_func_enter_;

#ifdef CONFIG_IO_CMD52
	if (cnt <= 2) {
		// prevent 4 bytes access problem
		int i = 0;
		for (i = 0; i < cnt; i++) {
			sdio_writeb(func, *(u8*)(pdata+i), (addr+i)&0x1FFFF, &status);
			if (status) break;
		}
		if (status) return _FAIL;
		return _SUCCESS;
	}
#endif

#ifdef CONFIG_IO_4B
#ifdef CONFIG_IO_4B_ADDR
	addr_offset = addr % 4;
	if (addr_offset) {
		addr -= addr_offset;
		cnt += addr_offset;
	}
#endif

	if (cnt % 4)
		cnt = ((cnt >> 2) + 1) << 2;
	if (cnt != cnt_org) {
		pdata = _malloc(cnt);
		if (pdata == NULL) {
			printk(KERN_CRIT "%s: _malloc FAIL! ADDR=%#x Size=%d\n", __func__, addr, cnt);
//			RT_TRACE(_module_hci_ops_os_c_, _drv_emerg_,
//				 ("sdbus_write_reg: _malloc fail\n"));
			return _FAIL; // SDIO_STATUS_NO_RESOURCES
		}
		status = sdio_memcpy_fromio(func, pdata, addr&0x1FFFF, cnt);
		if (status) {
			printk(KERN_CRIT "%s: read FAIL(%d)! ADDR=%#x Size=%d\n", __func__, status, addr, cnt);
//			RT_TRACE(_module_hci_ops_os_c_,_drv_emerg_,
//				 ("sdbus_write_reg: read failed %d\n"
//				  "***** Addr = %#x *****\n"
//				  "***** Length = %d *****\n", status, addr, cnt));
			_mfree(pdata, cnt);
			return _FAIL;
		}
		_memcpy(pdata + addr_offset, pdata_org, cnt_org);
		/* if data been modify between this read and write, may cause a problem */
	}
#endif
	status = sdio_memcpy_toio(func, addr&0x1FFFF, pdata, cnt);
#ifdef CONFIG_IO_4B
	if (cnt != cnt_org) _mfree(pdata, cnt);
#endif
	if (status) {
		printk(KERN_CRIT "%s: write FAIL(%d)! ADDR=%#x Size=%d\n", __func__, status, addr, cnt);
//		RT_TRACE(_module_hci_ops_os_c_, _drv_emerg_,
//			 ("sdbus_write_reg: failed %d\n"
//			  "***** Addr = %#x *****\n"
//			  "***** Length = %d *****\n", status, addr, cnt));
		return _FAIL;
	}

_func_exit_;

	return _SUCCESS;
}

uint sdbus_write_reg(struct intf_priv *pintfpriv, u32 addr, u32 cnt, void *pdata)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;
	s32 ret;


	sdio_claim_host(func);
	ret = _sdbus_write_reg(pintfpriv, addr, cnt, pdata);
	sdio_release_host(func);

	return ret;
}

uint sdbus_write_bytes_from_xmitbuf(struct intf_priv *pintfpriv, u32 addr, u32 cnt, u8 *pbuf)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;
	int ret;

_func_enter_;

	sdio_claim_host(func); 
	ret = sdio_memcpy_toio(func, addr,pbuf, cnt);
	sdio_release_host(func);
	if (ret) {
		printk(KERN_CRIT "%s: ERROR(%#x[%d])\n", __func__, ret, ret);
//		RT_TRACE(_module_hci_ops_os_c_, _drv_emerg_, ("sdbus_write_bytes_from_xmitbuf: error(%#x[%d])\n", ret, ret));
		return _FAIL;
	}

_func_exit_;

	return _SUCCESS;       
}

uint sdbus_write_blocks_from_xmitbuf(struct intf_priv *pintfpriv, u32 addr, u32 cnt, u8 *pbuf,u8 async)
{
	struct dvobj_priv *pdvobjpriv = (struct dvobj_priv*)pintfpriv->intf_dev;
	struct sdio_func *func = pdvobjpriv->func;
	int ret;

_func_enter_;

	RT_TRACE(_module_hci_ops_os_c_,_drv_info_,
		 ("sdbus_write_blocks_from_xmitbuf: async=%d buf=0x%p cnt=%d\n",
		  async, pbuf, cnt));

	if (async == _TRUE) {
		struct xmit_buf *pxmitbuf = (struct xmit_buf*)pbuf;
		pbuf = pxmitbuf->pbuf;
		RT_TRACE(_module_hci_ops_os_c_,_drv_info_,
			 ("sdbus_write_blocks_from_xmitbuf: (async) xmitbuf=0x%p buf=0x%p\n",
			  pxmitbuf, pbuf));
	}
		
	sdio_claim_host(func);
	ret = sdio_memcpy_toio(func, addr, pbuf, cnt);
	sdio_release_host(func);
	if (ret) {
		printk(KERN_CRIT "%s: ERROR(%#x[%d])\n", __func__, ret, ret);
//		RT_TRACE(_module_hci_ops_os_c_, _drv_emerg_, ("sdbus_write_blocks_from_xmitbuf: error(%d)\n", ret));
		return _FAIL;
	}

_func_exit_;

	return _SUCCESS;       
}

#ifdef POWER_DOWN_SUPPORT
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>

// core/core.h
#define MMC_CMD_RETRIES        3

static inline void mmc_delay(unsigned int ms)
{
	if (ms < 1000 / HZ) {
		cond_resched();
		mdelay(ms);
	} else {
		msleep(ms);
	}
}
// end of core/core.h

// core/core.c
/*
 * Internal function that does the actual ios call to the host driver,
 * optionally printing some debug output.
 */
static inline void mmc_set_ios(struct mmc_host *host)
{
	struct mmc_ios *ios = &host->ios;

	pr_debug("%s: clock %uHz busmode %u powermode %u cs %u Vdd %u "
		"width %u timing %u\n",
		 mmc_hostname(host), ios->clock, ios->bus_mode,
		 ios->power_mode, ios->chip_select, ios->vdd,
		 ios->bus_width, ios->timing);

	host->ops->set_ios(host, ios);
}

static void mmc_set_clock(struct mmc_host *host, unsigned int hz)
{
	WARN_ON(hz < host->f_min);

	if (hz > host->f_max)
		hz = host->f_max;

	host->ios.clock = hz;
	mmc_set_ios(host);
}

/*
 * Change data bus width of a host.
 */
static void mmc_set_bus_width(struct mmc_host *host, unsigned int width)
{
	host->ios.bus_width = width;
	mmc_set_ios(host);
}

/*
 * Select timing parameters for host.
 */
static void mmc_set_timing(struct mmc_host *host, unsigned int timing)
{
	host->ios.timing = timing;
	mmc_set_ios(host);
}
// end of core/core.c

// core/sdio_ops.c
static int mmc_send_io_op_cond(struct mmc_host *host, u32 ocr, u32 *rocr)
{
	struct mmc_command cmd;
	int i, err = 0;

	BUG_ON(!host);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = SD_IO_SEND_OP_COND;
	cmd.arg = ocr;
	cmd.flags = MMC_RSP_SPI_R4 | MMC_RSP_R4 | MMC_CMD_BCR;

	for (i = 100; i; i--) {
		err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
		if (err)
			break;

		/* if we're just probing, do a single pass */
		if (ocr == 0)
			break;

		/* otherwise wait until reset completes */
		if (mmc_host_is_spi(host)) {
			/*
			 * Both R1_SPI_IDLE and MMC_CARD_BUSY indicate
			 * an initialized card under SPI, but some cards
			 * (Marvell's) only behave when looking at this
			 * one.
			 */
			if (cmd.resp[1] & MMC_CARD_BUSY)
				break;
		} else {
			if (cmd.resp[0] & MMC_CARD_BUSY)
				break;
		}

		err = -ETIMEDOUT;

		mmc_delay(10);
	}

	if (rocr)
		*rocr = cmd.resp[mmc_host_is_spi(host) ? 1 : 0];

	return err;
}

static int mmc_io_rw_direct(struct mmc_card *card, int write, unsigned fn,
	unsigned addr, u8 in, u8* out)
{
	struct mmc_command cmd;
	int err;

	BUG_ON(!card);
	BUG_ON(fn > 7);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = SD_IO_RW_DIRECT;
	cmd.arg = write ? 0x80000000 : 0x00000000;
	cmd.arg |= fn << 28;
	cmd.arg |= (write && out) ? 0x08000000 : 0x00000000;
	cmd.arg |= addr << 9;
	cmd.arg |= in;
	cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err)
		return err;

	if (mmc_host_is_spi(card->host)) {
		/* host driver already reported errors */
	} else {
		if (cmd.resp[0] & R5_ERROR)
			return -EIO;
		if (cmd.resp[0] & R5_FUNCTION_NUMBER)
			return -EINVAL;
		if (cmd.resp[0] & R5_OUT_OF_RANGE)
			return -ERANGE;
	}

	if (out) {
		if (mmc_host_is_spi(card->host))
			*out = (cmd.resp[0] >> 8) & 0xFF;
		else
			*out = cmd.resp[0] & 0xFF;
	}

	return 0;
}
// end of core/sdio_ops.c

// core/sd_ops.c
static int mmc_send_relative_addr(struct mmc_host *host, unsigned int *rca)
{
	int err;
	struct mmc_command cmd;

	BUG_ON(!host);
	BUG_ON(!rca);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = SD_SEND_RELATIVE_ADDR;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R6 | MMC_CMD_BCR;

	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	*rca = cmd.resp[0] >> 16;

	return 0;
}
// end of core/sd_ops.c

// core/mmc_ops.c
static int _mmc_select_card(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	struct mmc_command cmd;

	BUG_ON(!host);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SELECT_CARD;

	if (card) {
		cmd.arg = card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	} else {
		cmd.arg = 0;
		cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;
	}

	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

static int mmc_select_card(struct mmc_card *card)
{
	BUG_ON(!card);

	return _mmc_select_card(card->host, card);
}
// end of core/mmc_ops.c

// core/sdio.c
static int sdio_enable_wide(struct mmc_card *card)
{
	int ret;
	u8 ctrl;

	if (!(card->host->caps & MMC_CAP_4_BIT_DATA))
		return 0;

	if (card->cccr.low_speed && !card->cccr.wide_bus)
		return 0;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_IF, 0, &ctrl);
	if (ret)
		return ret;

	ctrl |= SDIO_BUS_WIDTH_4BIT;

	ret = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_IF, ctrl, NULL);
	if (ret)
		return ret;

	mmc_set_bus_width(card->host, MMC_BUS_WIDTH_4);

	return 0;
}

/*
 * Test if the card supports high-speed mode and, if so, switch to it.
 */
static int sdio_enable_hs(struct mmc_card *card)
{
	int ret;
	u8 speed;

	if (!(card->host->caps & MMC_CAP_SD_HIGHSPEED))
		return 0;

	if (!card->cccr.high_speed)
		return 0;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_SPEED, 0, &speed);
	if (ret)
		return ret;

	speed |= SDIO_SPEED_EHS;

	ret = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_SPEED, speed, NULL);
	if (ret)
		return ret;

	mmc_card_set_highspeed(card);
	mmc_set_timing(card->host, MMC_TIMING_SD_HS);

	return 0;
}
// end of core/sdio.c

static int attrib_direct_write(_adapter *adapter, u32 addr, u32 cnt, u8 *pvalue)
{
	struct sdio_func *func = adapter->dvobjpriv.func;
	int err;
	u32 i;

_func_enter_;

	addr &= 0x1FFFF;

	sdio_claim_host(func);
	for (i = 0; i < cnt; i++) {
		err = mmc_io_rw_direct(func->card, 1, 0, addr+i, pvalue[i], NULL);
		if (err) {
			printk(KERN_CRIT "%s: FAIL!(%d) addr=%#05x val=%#02x\n",
				__func__, err, addr+i, pvalue[i]);
		}
	}
	sdio_release_host(func);

_func_exit_;

	return err;
}

static s32 sdio_dev_recognize(_adapter *padapter)
{
	struct sdio_func *func = padapter->dvobjpriv.func;
	struct mmc_card *card = func->card;
	struct mmc_host *host = card->host;
	u32 ocr;
	s32 err = 0;


	/* reference from mmc_attach_sdio()@core/sdio.c */
	mmc_set_clock(host, host->f_min);

	sdio_claim_host(func);

	err = mmc_send_io_op_cond(host, host->ocr, &ocr); //cmd 5
	if (err) {
		printk(KERN_CRIT "%s: (cmd5)mmc_send_io_op_cond ocr=%#06x FAIL(%d)\n", __func__, host->ocr, err);
		goto err;
	}
	err = mmc_send_relative_addr(host, &card->rca); //cmd 3
	if (err) {
		printk(KERN_CRIT "%s: (cmd3)mmc_send_relative_addr FAIL(%d)\n", __func__, err);
		goto err;
	}
	err = mmc_select_card(card); //cmd 7
	if (err) {
		printk(KERN_CRIT "%s: (cmd7)mmc_select_card FAIL(%d)\n", __func__, err);
		goto err;
	}

	/*
	 * Switch to high-speed (if supported).
	 */
	err = sdio_enable_hs(card);
	if (err) {
		printk(KERN_CRIT "%s: sdio_enable_hs FAIL(%d)\n", __func__, err);
		goto err;
	}

	/*
	 * Change to the card's maximum speed.
	 */
	if (mmc_card_highspeed(card)) {
		/*
		 * The SDIO specification doesn't mention how
		 * the CIS transfer speed register relates to
		 * high-speed, but it seems that 50 MHz is
		 * mandatory.
		 */
		mmc_set_clock(host, 50000000);
	} else {
		mmc_set_clock(host, card->cis.max_dtr);
	}

	/*
	 * Switch to wider bus (if supported).
	 */
	err = sdio_enable_wide(card);
	if (err){
		printk(KERN_CRIT "%s: sdio_enable_wide FAIL(%d)\n", __func__, err);
		goto err;
	}

err:
	sdio_release_host(func);

	return err;
}

static void sdio_dev_poweroff(_adapter *padapter)
{
	struct mmc_host *host = padapter->dvobjpriv.func->card->host;

#if 1
	host->ios.clock = 0;
	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ios.timing = MMC_TIMING_LEGACY;
	mmc_set_ios(host);
#else
	// lower host clock to save power
	mmc_set_clock(host, host->f_min);
	mmc_set_bus_width(host, MMC_BUS_WIDTH_1);
	mmc_set_timing(host, MMC_TIMING_LEGACY);
#endif
}

/*
 * Hardware recover and reset
 */
s32 dev_power_on(_adapter *padapter)
{
	s32 err;


	// reference from r871xs_drv_init(), insmod->probe
	err = sdio_dev_recognize(padapter);
	if (err) return _FAIL;

	// sd_dvobj_init() for SDIO
	if ((padapter->dvobj_init == NULL) ||
	    (padapter->dvobj_init(padapter) != _SUCCESS)) {
		printk(KERN_ERR "%s: initialize device object priv FAIL!\n", __func__);
		return _FAIL;
	}

	return _SUCCESS;
}

void disable_interrupt(_adapter *padapter)
{
	write16(padapter, SDIO_HIMR, 0); // disable HIMR
//	read16(padapter, SDIO_HISR); // clear HISR
}

void dev_power_off(_adapter *padapter)
{
	disable_interrupt(padapter);

	// sd_dvobj_deinit() for SDIO
	if (padapter->dvobj_deinit != NULL)
	    padapter->dvobj_deinit(padapter);

	sdio_dev_poweroff(padapter);
}
#endif /* POWER_DOWN_SUPPORT */

