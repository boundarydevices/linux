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
#define _HCI_INTF_C_

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <recv_osdep.h>
#include <xmit_osdep.h>

#include <hal_init.h>
#include <sdio_hal.h>
#include <sdio_ops.h>
#include <linux/mmc/sdio_func.h> 
#include <linux/mmc/sdio_ids.h>
#include <rtl8712_efuse.h>
extern u32 start_drv_threads(_adapter *padapter);
extern void stop_drv_threads (_adapter *padapter);
extern u8 init_drv_sw(_adapter *padapter);
extern u8 free_drv_sw(_adapter *padapter);
extern struct net_device *init_netdev(void);
extern void update_recvframe_attrib_from_recvstat(struct rx_pkt_attrib
*pattrib, struct recv_stat *prxstat);
extern char* initmac;

static const struct sdio_device_id sdio_ids[] = {
	{ SDIO_DEVICE(0x024c, 0x8712)		},
	{ SDIO_DEVICE(0x024c, 0x2300)		},
//	{ SDIO_DEVICE_CLASS(SDIO_CLASS_WLAN)		},
//	{ /* end: all zeroes */				},
};

typedef struct _driver_priv{
		struct sdio_driver r871xs_drv;
}drv_priv, *pdrv_priv;

void sd_sync_int_hdl(struct sdio_func *func);


/*
*  2010/12/01 Comment by Jeff
*  Should we move the following two functions to
*  common code section?
*/

u8 key_char2num(u8 ch)
{
    if((ch>='0')&&(ch<='9'))
        return ch - '0';
    else if ((ch>='a')&&(ch<='f'))
        return ch - 'a' + 10;
    else if ((ch>='A')&&(ch<='F'))
        return ch - 'A' + 10;
    else
	 return 0xff;
}

u8 key_2char2num(u8 hch, u8 lch)
{
    return ((key_char2num(hch) << 4) | key_char2num(lch));
}


//Added by Jordan
extern void stop_drv_timers(_adapter *padapter);
#ifdef SUSPEND_MODE
_adapter *prtl871x_sdio_suspend_hdl = NULL;
extern void rtl871x_sdio_suspend(struct early_suspend *cardpm){
	_adapter *padapter=prtl871x_sdio_suspend_hdl;
	struct dvobj_priv *psddev;//=&padapter->dvobjpriv;
	printk("\rtl871x_sdio_suspend   prtl871x_sdio_suspend_hdl=%p +  \n",prtl871x_sdio_suspend_hdl);
	if(prtl871x_sdio_suspend_hdl !=NULL){
		psddev=&padapter->dvobjpriv;

		//s1.
		if(padapter->pnetdev)
		{
			if (!netif_queue_stopped(padapter->pnetdev))
				netif_stop_queue(padapter->pnetdev);
		}
		
		//s2.
		//s2-1.  issue disassoc_cmd to fw
	//	disassoc_cmd(padapter);
		//s2-2.  indicate disconnect to os
	//	indicate_disconnect(padapter);
		padapter->mlmepriv.fw_state &= ~0x1;

		//s2-3.
		free_assoc_resources(padapter);
		if(padapter->hw_init_completed==_TRUE)
		{
			printk("\nrtl871x_sdio_suspend  rtl871x_hal_deinit start \n");
			rtl871x_hal_deinit(padapter);
			padapter->hw_init_completed=_FALSE;
			printk("\nrtl871x_sdio_suspend  rtl871x_hal_deinit end\n");

		}
		padapter->bSurpriseRemoved=_TRUE;
	}
	else{
		printk("\nrtl871x_sdio_suspend  didn't have the adapter~~\n");
	}
	printk("\n rtl871x_sdio_suspend  \n");

}
extern void rtl871x_sdio_resume(struct early_suspend *cardpm){
	
		printk("\rtl871x_sdio_resume \n");

			return;
}
static struct early_suspend early= {
	.suspend= rtl871x_sdio_suspend,
	.resume= rtl871x_sdio_resume	
};



void init_rtl871x_card_pm(_adapter *padapter){
       
	struct dvobj_priv *psddev=&padapter->dvobjpriv;
	printk("\nregister_mmc_card_pm== \n");
	prtl871x_sdio_suspend_hdl=padapter;
	printk("\nregister_mmc_card_pm== prtl871x_sdio_suspend_hdl=%p  padapter=%p\n",prtl871x_sdio_suspend_hdl,padapter);
	return;
}
extern struct early_suspend dhdpm;
extern void register_mmc_card_pm(struct early_suspend *cardpm);
extern void unregister_mmc_card_pm(void);
#endif

unsigned int sd_dvobj_init(_adapter *padapter)
{
	struct dvobj_priv *psddev = &padapter->dvobjpriv;
	struct sdio_func *func = psddev->func;
	int err;

_func_enter_;

	sdio_claim_host(func);
	err = sdio_enable_func(func);
	if (err) {
		printk(KERN_ERR "%s: sdio_enable_func FAIL(%d)!\n", __func__, err);
		sdio_release_host(func);
		return _FAIL;
	}

	padapter->EepromAddressSize = 6;
	psddev->tx_block_mode = 1;
	psddev->rx_block_mode = 1;

	err = sdio_set_block_size(func, 512);
	if (err) {
		printk(KERN_ERR "%s: sdio_set_block_size FAIL(%d)!\n", __func__, err);
		sdio_release_host(func);
		return _FAIL;
	}
	psddev->block_transfer_len = 512;
	psddev->blk_shiftbits = 9;

	err = sdio_claim_irq(func, sd_sync_int_hdl);
	if (err) {
		printk(KERN_ERR "%s: sdio_claim_irq FAIL(%d)!\n", __func__, err);
		sdio_release_host(func);
		return _FAIL;
	}
	sdio_release_host(func);
	psddev->sdio_himr = 0xff;

_func_exit_;

	return _SUCCESS;
}

void sd_dvobj_deinit(_adapter *padapter)
{
	struct dvobj_priv *psddev = &padapter->dvobjpriv;
	struct sdio_func *func = psddev->func;
	int err;

	RT_TRACE(_module_hci_intfs_c_,_drv_err_,("+SDIO deinit\n"));
	if (func) {
		sdio_claim_host(func);
		err = sdio_release_irq(func);
		if (err)
			printk(KERN_ERR "%s: sdio_release_irq(%d)\n", __func__, err);
		err = sdio_disable_func(func);
		if (err)
			printk(KERN_ERR "%s: sdio_disable_func(%d)\n", __func__, err);
		sdio_release_host(func);
	}
}

s32 _sdbus_read_reg(struct intf_priv *pintfpriv, u32 addr, u32 cnt, void *pdata);
s32 _sdbus_write_reg(struct intf_priv *pintfpriv, u32 addr, u32 cnt, void *pdata);

void sdio_read_int(_adapter *padapter, u32 addr, u8 sz, void *pdata)
{
	struct io_queue	*pio_queue = (struct io_queue*)padapter->pio_queue;
	struct intf_hdl	*pintfhdl = &pio_queue->intf;
	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;
	u32 ftaddr = 0;
	s32 res;

_func_enter_;

//	RT_TRACE(_module_hci_ops_c_,_drv_err_,("sdio_read_int\n"));

	if ((_cvrt2ftaddr(addr, &ftaddr)) == _SUCCESS) {
		res = _sdbus_read_reg(pintfpriv, ftaddr, sz, pdata);
		if (res != _SUCCESS) {
			RT_TRACE(_module_hci_ops_c_, _drv_emerg_, ("sdio_read_int fail!!!\n"));
		}
	} else {
		RT_TRACE(_module_hci_ops_c_, _drv_emerg_, ("sdio_read_int address translate error!!!\n"));
	}
	switch(sz){
	case 2:
		*((u16 *)pdata)=le16_to_cpu(*((u16 *)pdata));
		break;
	case 4:
		*((u32 *)pdata)=le32_to_cpu(*((u32 *)pdata));
		break;
	}
_func_exit_;
}

void sdio_write_int(_adapter *padapter, u32 addr, u32 val, u8 sz)
{
	struct io_queue	*pio_queue = (struct io_queue*)padapter->pio_queue;
	struct intf_hdl	*pintfhdl = &pio_queue->intf;
	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;

	u32 ftaddr = 0;
	s32 res;

_func_enter_;

//	RT_TRACE(_module_hci_ops_c_,_drv_err_,("sdio_write_int\n"));

	val = cpu_to_le32(val);

	if ((_cvrt2ftaddr(addr, &ftaddr)) == _SUCCESS) {
		res = _sdbus_write_reg(pintfpriv, ftaddr, sz, &val);
		if (res != _SUCCESS) {
			RT_TRACE(_module_hci_ops_c_, _drv_emerg_, ("sdio_write_int fail!!!\n"));
		}
	} else {
		RT_TRACE(_module_hci_ops_c_, _drv_emerg_, ("sdio_write_int address translate error!!!\n"));
	}

_func_exit_;
}

static s32 recvbuf2recvframe_s(_adapter *padapter, struct recv_buf *precvbuf)
{
//	_irqL irql;
	u8 *pbuf;
//	u8 bsumbit = _FALSE;
	uint pkt_len, pkt_offset;
	int transfer_len;
	struct recv_stat *prxstat;
	u16 pkt_cnt, drvinfo_sz;
	_queue *pfree_recv_queue;
	union recv_frame *precvframe = NULL,*plast_recvframe = NULL;
	struct recv_priv *precvpriv = &padapter->recvpriv;
//	struct intf_hdl *pintfhdl = &padapter->pio_queue->intf;

	RT_TRACE(_module_rtl871x_recv_c_,_drv_info_,("+recvbuf2recvframe()\n"));

	pfree_recv_queue = &(precvpriv->free_recv_queue);

	pbuf = (u8*)precvbuf->pbuf;

	prxstat = (struct recv_stat *)pbuf;
/*	{
		u8 i;
		printk("\n-----recvbuf-----\n");
		for (i=0;i<64;i=i+8) {
			printk("0x%.2x:0x%.2x:0x%.2x:0x%.2x:0x%.2x:0x%.2x:0x%.2x:0x%.2x\n",pbuf[i],pbuf[i+1],pbuf[i+2],pbuf[i+3],pbuf[i+4],pbuf[i+5],pbuf[i+6],pbuf[i+7]);
		}
		printk("\n-----recvbuf end-----\n");
	}*/
	transfer_len = precvbuf->len;
	precvbuf->ref_cnt++;
	do {
		precvframe = NULL;
		precvframe = alloc_recvframe(pfree_recv_queue);
		if (precvframe == NULL){
			_irqL irqL;

			RT_TRACE(_module_rtl871x_recv_c_,_drv_err_,("recvbuf2recvframe(), precvframe==NULL\n"));
			_enter_critical_ex(&precvbuf->recvbuf_lock, &irqL);
			precvbuf->ref_cnt--;
			_exit_critical_ex(&precvbuf->recvbuf_lock, &irqL);
			break;
		}
		if (plast_recvframe != NULL) {
			if (recv_entry(plast_recvframe) != _SUCCESS) {
				RT_TRACE(_module_rtl871x_recv_c_,_drv_info_,("recvbuf2recvframe(), recv_entry(precvframe) != _SUCCESS\n"));
			}
		}
		prxstat = (struct recv_stat*)pbuf;
		pkt_len = (le32_to_cpu(prxstat->rxdw0))&0x00003fff; //pkt_len = prxstat->frame_length;             

		RT_TRACE(_module_rtl871x_recv_c_,_drv_info_,("rxdesc: offsset0:0x%08x, offsset4:0x%08x, offsset8:0x%08x, offssetc:0x%08x\n",prxstat->rxdw0, prxstat->rxdw1, prxstat->rxdw2, prxstat->rxdw4));

		drvinfo_sz = ((le32_to_cpu(prxstat->rxdw0)&0x000f0000)>>16);//uint 2^3 = 8 bytes
		drvinfo_sz = drvinfo_sz << 3;
		RT_TRACE(_module_rtl871x_recv_c_, _drv_info_, ("pkt_len=%d[0x%x] drvinfo_sz=%d[0x%x]\n", pkt_len, pkt_len, drvinfo_sz, drvinfo_sz));
		precvframe->u.hdr.precvbuf = precvbuf;
		precvframe->u.hdr.adapter = padapter;
		init_recvframe(precvframe, precvpriv);

		precvframe->u.hdr.rx_head = precvframe->u.hdr.rx_data = precvframe->u.hdr.rx_tail = pbuf;
		precvframe->u.hdr.rx_end = precvbuf->pend;
		update_recvframe_attrib_from_recvstat(&precvframe->u.hdr.attrib, prxstat);
		pkt_offset = pkt_len + drvinfo_sz + RXDESC_SIZE;

		recvframe_put(precvframe, pkt_len + drvinfo_sz + RXDESC_SIZE);
		recvframe_pull(precvframe, drvinfo_sz + RXDESC_SIZE);
/*		{
			u8 i;
			printk("\n-----packet-----\n");
			for(i=0;i<32;i++){
			printk("0x%.2x:0x%.2x:0x%.2x:0x%.2x:0x%.2x:0x%.2x:0x%.2x:0x%.2x\n",precvframe->u.hdr.rx_data[i],precvframe->u.hdr.rx_data[i+1],precvframe->u.hdr.rx_data[i+2],precvframe->u.hdr.rx_data[i+3],precvframe->u.hdr.rx_data[i+4],precvframe->u.hdr.rx_data[i+5],precvframe->u.hdr.rx_data[i+6],precvframe->u.hdr.rx_data[i+7]);
			}
			printk("\n-----packet end-----\n");
		}*/
		RT_TRACE(_module_rtl871x_recv_c_,_drv_info_,("precvframe->u.hdr.rx_head=%p precvframe->u.hdr.rx_data=%p precvframe->u.hdr.rx_tail=%p precvframe->u.hdr.rx_end=%p\n",precvframe->u.hdr.rx_head,precvframe->u.hdr.rx_data,precvframe->u.hdr.rx_tail,precvframe->u.hdr.rx_end));

		RT_TRACE(_module_rtl871x_recv_c_,_drv_info_,("pkt_offset=%d [1]\n",pkt_offset));
		pkt_offset = _RND512(pkt_offset);
		RT_TRACE(_module_rtl871x_recv_c_,_drv_info_,("pkt_offset=%d [2] transfer_len=%d\n",pkt_offset,transfer_len));
		transfer_len -= pkt_offset;
		RT_TRACE(_module_rtl871x_recv_c_,_drv_info_,("transfer_len=%d \n",transfer_len));
		pbuf += pkt_offset;
		if (transfer_len > 0) {
			_irqL irqL;

			_enter_critical_ex(&precvbuf->recvbuf_lock, &irqL);
			precvbuf->ref_cnt++;
			_exit_critical_ex(&precvbuf->recvbuf_lock, &irqL);
		}
		plast_recvframe = precvframe;
		precvframe = NULL;
	} while (transfer_len > 0);

	if (plast_recvframe != NULL) {
		if (recv_entry(plast_recvframe) != _SUCCESS) {
			RT_TRACE(_module_rtl871x_recv_c_,_drv_info_,("recvbuf2recvframe(), recv_entry(precvframe) != _SUCCESS\n"));
		}
	}

	dev_kfree_skb_any(precvbuf->pskb);
	precvbuf->pskb = NULL;

	return _SUCCESS;
}

/*
 * Return
 *	_SUCCESS	read data from RXFIFO success
 *	_FAIL		didn't read data yet
 *
 */
u32 read_pkt2recvbuf(PADAPTER padapter, u32 rd_cnt, struct recv_buf *precvbuf)
{
	//3 1. alloc skb
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)) // http://www.mail-archive.com/netdev@vger.kernel.org/msg17214.html
	precvbuf->pskb = dev_alloc_skb(rd_cnt);
#else
	precvbuf->pskb = netdev_alloc_skb(padapter->pnetdev, rd_cnt);
#endif
	if (precvbuf->pskb == NULL) {
		RT_TRACE(_module_hci_ops_os_c_, _drv_err_, ("read_pkt2recvbuf: alloc_skb fail!\n"));
		return _FAIL;
	}

	precvbuf->phead = precvbuf->pskb->head;
	precvbuf->pdata = precvbuf->pskb->data;
#ifdef NET_SKBUFF_DATA_USES_OFFSET
	precvbuf->ptail = precvbuf->pskb->head + precvbuf->pskb->tail;
	precvbuf->pend = precvbuf->pskb->head + precvbuf->pskb->end;
#else
	precvbuf->ptail = precvbuf->pskb->tail;
	precvbuf->pend = precvbuf->pskb->end;
#endif
	precvbuf->pbuf = precvbuf->pskb->data;

	//3 2. read data
	read_port(padapter, RTL8712_DMA_RX0FF, rd_cnt, (u8*)precvbuf);
	precvbuf->ptail = precvbuf->ptail + rd_cnt;
	precvbuf->len = rd_cnt;

	//3 3. handle packet
	recvbuf2recvframe_s(padapter, precvbuf);
#if 0
	dev_kfree_skb_any(precvbuf->pskb);
	precvbuf->pskb = NULL;
	list_delete(&(precvbuf->list));
	list_insert_tail(&precvbuf->list, get_list_head(&precvpriv->free_recv_buf_queue));
	precvpriv->free_recv_buf_queue_cnt++;
#endif

	return _SUCCESS;
}

void sd_int_dpc(PADAPTER padapter);

void sd_sync_int_hdl(struct sdio_func *func)
{
	struct dvobj_priv *psdpriv = sdio_get_drvdata(func);
	_adapter *padapter = (_adapter*)psdpriv->padapter;
	u16 tmp16;
//	uint tasks;

_func_enter_;

	if ((padapter->bDriverStopped ==_TRUE) || (padapter->bSurpriseRemoved == _TRUE)) {
		goto exit;
	}

	//padapter->IsrContent=read16(padapter, SDIO_HISR);
	sdio_read_int(padapter, SDIO_HISR, 2, &psdpriv->sdio_hisr);

	if (psdpriv->sdio_hisr & psdpriv->sdio_himr)
	{
		sdio_write_int(padapter, SDIO_HIMR, 0, 2);
		sd_int_dpc(padapter);
		sdio_write_int(padapter, SDIO_HIMR, psdpriv->sdio_himr, 2);

		sdio_read_int(padapter, SDIO_HIMR, 2, &tmp16);
		if (tmp16 != psdpriv->sdio_himr)
			sdio_write_int(padapter, SDIO_HIMR, psdpriv->sdio_himr, 2);
	} else {
		RT_TRACE(_module_hci_intfs_c_, _drv_info_, ("<=========== sd_sync_int_hdl(): not our INT\n"));
	}
exit:

_func_exit_;

	return;
}

#ifdef RFKILL_SUPPORT
#include <linux/rfkill.h>

extern s32 _r8712_power_off(PADAPTER padapter, u8 poweroff);

static int rtl8712_rfkill_set_block(void *data, bool blocked)
{
	int ret = 0;
	int error = 0;
	PADAPTER padapter = (PADAPTER) data;
	struct rfkill *prfkill = (struct rfkill*)padapter->prfkill;


//	RT_TRACE(_module_hci_intfs_c_,_drv_notice_,
//		 ("+rtl8712_rfkill_set_block: blocked=%d\n", blocked));
	printk(KERN_NOTICE "+%s: blocked=%d\n", __func__, blocked);

#ifdef POWER_DOWN_SUPPORT
	error = _r8712_power_off(padapter, blocked);
	if (error) {
		printk(KERN_ERR "%s: Power Off FAIL!(%d)\n", __func__, error);
		return error;
	}
#endif

	ret = rfkill_set_sw_state(prfkill, blocked);
//	RT_TRACE(_module_hci_intfs_c_,_drv_notice_,
//		 ("-rtl8712_rfkill_set_block: rfkill_set_sw_state=%d\n", ret));
	printk(KERN_NOTICE "-%s: rfkill_set_sw_state=%d\n", __func__, ret);

	return error;
}

static const struct rfkill_ops rfkillops = {
	.poll = NULL,
	.query = NULL,
	.set_block = rtl8712_rfkill_set_block
};
#endif

static int r871xs_drv_init(struct sdio_func *func, const struct sdio_device_id *id)
{
	_adapter *padapter = NULL;
	struct dvobj_priv *pdvobjpriv;
	struct net_device *pnetdev;


//	printk(KERN_INFO "+%s: PROBE device! vendor=0x%x device=0x%x\n",
//		__func__, func->vendor, func->device);
	RT_TRACE(_module_hci_intfs_c_, _drv_alert_,
		 ("+871x - drv_init: id=0x%p func->vendor=0x%x func->device=0x%x\n",
		  id, func->vendor, func->device));

	//step 1.
	pnetdev = init_netdev();
	if (!pnetdev) goto error;

	padapter = netdev_priv(pnetdev);
	pdvobjpriv = &padapter->dvobjpriv;
	pdvobjpriv->padapter = padapter;
	pdvobjpriv->func = func;
	sdio_set_drvdata(func, pdvobjpriv);
	SET_NETDEV_DEV(pnetdev, &func->dev);


	//step 2.
	if (alloc_io_queue(padapter) == _FAIL) {
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("Can't init io_reqs\n"));
		goto error;
	}


#if 0  //temp remove
	//step 3.
	if (loadparam(padapter, pnetdev) == _FAIL) {
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("Read Parameter Failed!\n"));
		goto error;
	}
#endif

	//step 4.
	padapter->dvobj_init = &sd_dvobj_init;
	padapter->dvobj_deinit = &sd_dvobj_deinit;
	padapter->halpriv.hal_bus_init = &sd_hal_bus_init;
	padapter->halpriv.hal_bus_deinit = &sd_hal_bus_deinit;

	if ((padapter->dvobj_init == NULL) ||
	    (padapter->dvobj_init(padapter) != _SUCCESS)) {
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("initialize device object priv Failed!\n"));
		goto error;
	}

	//step 6.
	if (init_drv_sw(padapter) == _FAIL) {
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("Initialize driver software resource Failed!\n"));
		goto error;
	}

	//step 7 read efuse/eeprom data and get mac_addr
	/*
	*  2010/12/01 Comment by Jeff
	*  Add this code segment to read MAC addr from efuse, 
	*  instead of hardcoding. Because Android's dhcpcd will
	*  get the MAC addr at its startup time and use the MAC
	*  addr to form DHCP DISCOVER, it will cause the first
	*  DHCP try fail if the DHCP server is not in the AP.
	*/
	{
		u8 mac[6];
		u8 tmpU1b;
		u8 AutoloadFail, eeprom_sel;
		u8 *pdata = padapter->eeprompriv.efuse_eeprom_data;

		tmpU1b = read8(padapter, EE_9346CR);//CR9346	

		// To check system boot selection.
		if (tmpU1b & _9356SEL) {
			eeprom_sel = 1;
			printk(KERN_INFO "%s: Boot from EEPROM\n", __func__);
		} else {
			eeprom_sel = 0;
//			printk(KERN_INFO "%s: Boot from EFUSE\n", __func__);
		}

		// To check autoload success or not.
		if (tmpU1b & _EEPROM_EN) {
//			printk(KERN_INFO "%s: Autoload OK\n", __func__);
			AutoloadFail = _FALSE;

		} else {
			printk(KERN_ERR "%s: AutoLoad Fail reported from CR9346(0x%02x)!!\n", __func__, tmpU1b);
			AutoloadFail = _TRUE;
//			goto error;
		}

		if (AutoloadFail == _FALSE)
		{
			if (eeprom_sel == 0)
			{
				// Read efuse data
				u32 i, offset;

				//
				// <Roger_Note> The following operation are prevent Efuse leakage by turn on 2.5V.
				// 2008.11.25.
				//
				tmpU1b = read8(padapter, EFUSE_TEST+3);
				write8(padapter, EFUSE_TEST+3, tmpU1b | BIT(7));
				mdelay_os(1);
				write8(padapter, EFUSE_TEST+3, tmpU1b & (~BIT(7)));

				for (i = 0, offset = 0; i < 128; i += 8, offset++)
					efuse_pg_packet_read(padapter, offset, &pdata[i]);			
			}

			if (initmac) {
				// Users specify the mac address
				u32 jj, kk;

				for (jj = 0, kk = 0; jj < ETH_ALEN; jj++, kk += 3)
					mac[jj] = key_2char2num(initmac[kk], initmac[kk+ 1]);
			} else {
				// Use the mac address stored in the Efuse
				// MAC address offset in efuse = 0x12 for usb, 0x4f for sdio
			 	_memcpy(mac, &pdata[0x4f], ETH_ALEN);
			}
		}

		if ((AutoloadFail == _TRUE) ||
		    ((mac[0]==0xff) && (mac[1]==0xff) && (mac[2]==0xff) &&
		     (mac[3]==0xff) && (mac[4]==0xff) && (mac[5]==0xff)) ||
		    ((mac[0]==0x0) && (mac[1]==0x0) && (mac[2]==0x0) &&
		     (mac[3]==0x0) && (mac[4]==0x0) &&(mac[5]==0x0)))
		{
			mac[0] = 0x00;
			mac[1] = 0xe0;
			mac[2] = 0x4c;
			mac[3] = 0x87;
			mac[4] = 0x00;
			mac[5] = 0x00;
		}

		_memcpy(pnetdev->dev_addr, mac/*padapter->eeprompriv.mac_addr*/, ETH_ALEN);

		printk(KERN_NOTICE "%s: MAC Address=%02x:%02x:%02x:%02x:%02x:%02x\n",
			__func__, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	//step 8.
	/* Tell the network stack we exist */
	if (register_netdev(pnetdev) != 0) {
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("register_netdev() failed\n"));
		goto error;
	}

	// step 9.
	// Power On/Off, RF On/Off
#ifdef POWER_DOWN_SUPPORT
	padapter->bpoweroff = _FALSE;
#endif
#ifdef RFKILL_SUPPORT
	// register to as one RFKILL device
	padapter->prfkill = rfkill_alloc(pnetdev->name, &pnetdev->dev, RFKILL_TYPE_WLAN, &rfkillops, padapter);
	if (padapter->prfkill) {
		int ret;
//		rfkill_init_sw_state(padapter->prfkill, false);
		ret = rfkill_register(padapter->prfkill);
		if (ret) {
			rfkill_destroy(padapter->prfkill);
			padapter->prfkill = NULL;
			printk(KERN_ERR "%s: rfkill_register=%d FAIL!\n", __func__, ret);
		}
	} else
		printk(KERN_CRIT "%s: rfkill_alloc fail!\n", __func__);
#endif

	//
	// Led mode
	// 

	padapter->ledpriv.LedStrategy = SW_LED_MODE1;
	padapter->ledpriv.bRegUseLed = _TRUE;
			
	
#ifdef SUSPEND_MODE
	init_rtl871x_card_pm(padapter);
#endif

//	printk(KERN_INFO "-%s: Success. bDriverStopped=%d, bSurpriseRemoved=%d\n",
//		__func__, padapter->bDriverStopped, padapter->bSurpriseRemoved);
	RT_TRACE(_module_hci_intfs_c_, _drv_info_, ("-r871xs_drv_init: Success. bDriverStopped=%d bSurpriseRemoved=%d\n",padapter->bDriverStopped, padapter->bSurpriseRemoved));

	return 0;

error:
	if (padapter->dvobj_deinit == NULL) {
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("Initialize dvobjpriv.dvobj_deinit error!!!\n"));
	} else {
		padapter->dvobj_deinit(padapter);
	}

	if (pnetdev) {
		unregister_netdev(pnetdev);
		free_netdev(pnetdev);
	}

#ifdef SUSPEND_MODE
	prtl871x_sdio_suspend_hdl = NULL;
#endif

//	printk(KERN_INFO "-%s: FAIL! bDriverStopped=%d, bSurpriseRemoved=%d\n",
//		__func__, padapter->bDriverStopped, padapter->bSurpriseRemoved);
	RT_TRACE(_module_hci_intfs_c_, _drv_emerg_, ("-871x_sdio - drv_init, fail!\n"));

	return -1;
}

void rtl871x_intf_stop(_adapter *padapter)
{
	// Disable interrupt, also done in rtl8712_hal_deinit
//	write16(padapter, SDIO_HIMR, 0x00);
}

void r871x_dev_unload(_adapter *padapter)
{
	struct net_device *pnetdev = (struct net_device*)padapter->pnetdev;

	RT_TRACE(_module_hci_intfs_c_, _drv_err_, ("+r871x_dev_unload\n"));

	if (padapter->bup == _TRUE)
	{
#if 0
		//s1.
		if (pnetdev) {
			netif_carrier_off(pnetdev);
			netif_stop_queue(pnetdev);
		}
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("@ r871x_dev_unload:complelte s1!\n"));

		//s2.
		// indicate-disconnect if necssary (free all assoc-resources)
		// dis-assoc from assoc_sta (optional)
		indicate_disconnect(padapter);
		free_network_queue(padapter);
#endif
		//Added by Jordan
		stop_drv_timers(padapter);

		padapter->bDriverStopped = _TRUE;
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("@ r871x_dev_unload:complete s2!\n"));

		//s3.
		rtl871x_intf_stop(padapter);
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("@ r871x_dev_unload:complete s3!\n"));

		//s4.
		stop_drv_threads(padapter);
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("@ r871x_dev_unload:complete s4!\n"));

		//s5.
		if (padapter->bSurpriseRemoved == _FALSE) {
			rtl871x_hal_deinit(padapter);
			padapter->bSurpriseRemoved = _TRUE;
		}
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("@ r871x_dev_unload:complelt s5!\n"));

		//s6.
		if (padapter->dvobj_deinit) {
			padapter->dvobj_deinit(padapter); // call sd_dvobj_deinit()
		} else {
			RT_TRACE(_module_hci_intfs_c_,_drv_err_,("Initialize hcipriv.hci_priv_init error!!!\n"));
		}
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("@ r871x_dev_unload:complete s6!\n"));

		padapter->bup = _FALSE;
	}
	else {
		RT_TRACE(_module_hci_intfs_c_,_drv_err_,("r871x_dev_unload():padapter->bup == _FALSE\n" ));
	}

	RT_TRACE(_module_hci_intfs_c_,_drv_err_,("-r871x_dev_unload\n"));
}

static void r8712s_dev_remove(struct sdio_func *func)
{
	PADAPTER padapter = (PADAPTER) (((struct dvobj_priv*)sdio_get_drvdata(func))->padapter);
	struct net_device *pnetdev;

_func_enter_;

//	printk(KERN_INFO "+%s: REMOVE device!\n", __func__);
	RT_TRACE(_module_hci_intfs_c_, _drv_err_, ("+dev_remove()\n"));

	if (padapter)
	{
		pnetdev = (struct net_device*)padapter->pnetdev;

		if (padapter->bSurpriseRemoved == _FALSE)
		{
			// test surprise remove
			int err;

			sdio_claim_host(func);
			sdio_readb(func, 0, &err);
			sdio_release_host(func);
			if (err == -ENOMEDIUM) {
				padapter->bSurpriseRemoved = _TRUE;
				printk(KERN_NOTICE "%s: device had been removed!\n", __func__);
			}
		}

#ifdef RFKILL_SUPPORT
		if (padapter->prfkill) {
			rfkill_unregister((struct rfkill*)padapter->prfkill);
			rfkill_destroy((struct rfkill*)padapter->prfkill);
			padapter->prfkill = NULL;
		}
#endif

		if (pnetdev)
			unregister_netdev(pnetdev); //will call netdev_close()

#ifdef CONFIG_PWRCTRL
		leave_ps_mode(padapter);
#endif

		r871x_dev_unload(padapter);

		free_drv_sw(padapter); // will free netdev

#ifdef SUSPEND_MODE
		prtl871x_sdio_suspend_hdl=NULL;
#endif
	}

//	printk(KERN_INFO "-%s\n", __func__);
	RT_TRACE(_module_hci_intfs_c_,_drv_err_,("-dev_remove()\n"));

_func_exit_;

	return;
}

static drv_priv drvpriv = {	
		.r871xs_drv.probe		= r871xs_drv_init,
		.r871xs_drv.remove		= r8712s_dev_remove,
		.r871xs_drv.name		= "rtl871x_sdio_wlan",
		.r871xs_drv.id_table	= sdio_ids,
};	


static int __init r8712s_drv_entry(void)
{
	int status;


//	printk(KERN_INFO "+%s: LOAD driver.\n", __func__);
	RT_TRACE(_module_hci_intfs_c_,_drv_err_,("+8712s_sdio - drv_entry\n"));

	status = sdio_register_driver(&drvpriv.r871xs_drv);

#ifdef SUSPEND_MODE
	register_mmc_card_pm(&early);
	printk(KERN_INFO "register_mmc_card_pm--\n");
#endif

//	printk(KERN_INFO "-%s(%#x[%d])\n", __func__, status, status);
	RT_TRACE(_module_hci_intfs_c_,_drv_err_,("-8712_sdio - drv_entry, status=%d\n", status));

	return status;
}

static void __exit r8712s_drv_halt(void)
{
//	printk(KERN_INFO "+%s: UNLOAD driver!\n", __func__);
	RT_TRACE(_module_hci_intfs_c_,_drv_err_,("+8712_sdio - drv_halt\n"));

	sdio_unregister_driver(&drvpriv.r871xs_drv);	// call r8712s_dev_remove()

#ifdef SUSPEND_MODE
	unregister_mmc_card_pm();
	printk(KERN_INFO "unregister_mmc_card_pm--\n");
#endif

//	printk(KERN_INFO "-%s\n", __func__);
	RT_TRACE(_module_hci_intfs_c_,_drv_err_,("-8712_sdio - drv_halt\n"));
}


module_init(r8712s_drv_entry);
module_exit(r8712s_drv_halt);
