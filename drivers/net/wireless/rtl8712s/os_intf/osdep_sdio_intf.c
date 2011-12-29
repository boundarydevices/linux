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
#define _OSDEP_HCI_INTF_C_

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <recv_osdep.h>
#include <xmit_osdep.h>

#include <hal_init.h>
#include <rtl871x_byteorder.h>
#include <sdio_ops.h>
#include <rtl871x_debug.h>

#ifndef CONFIG_SDIO_HCI
#error "CONFIG_SDIO_HCI shall be on!\n"
#endif

s32 read_pkt2recvbuf(PADAPTER padapter, u32 rd_cnt, struct recv_buf *precvbuf);


void sd_recv_rxfifo(PADAPTER padapter)
{
	u16 rx_blknum;
	u32 blk_sz;
	s32 cnt, res;
	struct dvobj_priv *pdvobjpriv;
	struct recv_priv *precvpriv;
	struct recv_buf *precvbuf = NULL;
	_list *precvbuf_head, *precvbuf_list;
	_irqL irql, irqL;


	pdvobjpriv = &padapter->dvobjpriv;
	precvpriv = &padapter->recvpriv;

	blk_sz = pdvobjpriv->block_transfer_len;
	rx_blknum = pdvobjpriv->rxblknum;

//	_enter_hwio_critical(&pdvobjpriv->rx_protect, &rx_proc_irq);
//	pdvobjpriv->rxblknum = read16(padapter, SDIO_RX0_RDYBLK_NUM);
	sdio_read_int(padapter, SDIO_RX0_RDYBLK_NUM, 2, &pdvobjpriv->rxblknum);
	if (rx_blknum > pdvobjpriv->rxblknum) {
		cnt = (0x10000 - rx_blknum + pdvobjpriv->rxblknum) * blk_sz;
	} else {
		cnt = (pdvobjpriv->rxblknum - rx_blknum) * blk_sz;
	}
	RT_TRACE(_module_hci_intfs_c_, _drv_info_,
		 ("sd_recv_rxfifo: rxblknum=%d blknum(old)=%d cnt=%d\n",
		  pdvobjpriv->rxblknum, rx_blknum, cnt));

	if (cnt == 0) {
		RT_TRACE(_module_hci_intfs_c_, _drv_info_,
			 ("sd_recv_rxfifo: rxblknum=%d rxblknum_rd=%d\n",
			  pdvobjpriv->rxblknum, pdvobjpriv->rxblknum_rd));
		goto exit;
	}

	if (_queue_empty(&precvpriv->free_recv_buf_queue) == _TRUE) {
		RT_TRACE(_module_hci_intfs_c_, _drv_emerg_,
			 ("sd_recv_rxfifo: precvbuf=NULL precvpriv->free_recv_buf_queue_cnt=%d\n",
			  precvpriv->free_recv_buf_queue_cnt));
		goto drop_pkt;
	}

	_enter_critical(&precvpriv->free_recv_buf_queue.lock, &irql);
	precvbuf_head = get_list_head(&precvpriv->free_recv_buf_queue);
	precvbuf_list = get_next(precvbuf_head);
	precvbuf = LIST_CONTAINOR(precvbuf_list, struct recv_buf, list);
	list_delete(&precvbuf->list);
	precvpriv->free_recv_buf_queue_cnt--;
	precvbuf->ref_cnt = 1;
	_exit_critical(&precvpriv->free_recv_buf_queue.lock, &irql);
	RT_TRACE(_module_hci_intfs_c_, _drv_info_,
		 ("sd_recv_rxfifo: precvbuf=0x%p dequeue recv buf cnt=%d\n",
		  precvbuf, precvpriv->free_recv_buf_queue_cnt));

	res = read_pkt2recvbuf(padapter, cnt, precvbuf);

	_enter_critical_ex(&precvbuf->recvbuf_lock, &irqL);
	precvbuf->ref_cnt--;
	if (precvbuf->ref_cnt == 0) {
		_enter_critical(&precvpriv->free_recv_buf_queue.lock, &irql);
		list_delete(&precvbuf->list);
		list_insert_tail(&precvbuf->list, precvbuf_head);
		precvpriv->free_recv_buf_queue_cnt++;
		_exit_critical(&precvpriv->free_recv_buf_queue.lock, &irql);
		RT_TRACE(_module_rtl8712_recv_c_, _drv_notice_,
			 ("sd_recv_rxfifo: precvbuf=0x%p enqueue recv buf cnt=%d\n",
			  precvbuf, precvpriv->free_recv_buf_queue_cnt));
	}
	_exit_critical_ex(&precvbuf->recvbuf_lock, &irqL);

	if (res == _FAIL) {
		RT_TRACE(_module_hci_intfs_c_, _drv_emerg_,
			 ("sd_recv_rxfifo: read_pkt2recvbuf FAIL!\n"));
		goto drop_pkt;
	}

	goto exit;

drop_pkt:
	printk("%s: DROP RX DATA! size=%d\n", __func__, cnt);
//	RT_TRACE(_module_hci_intfs_c_, _drv_emerg_,
//		 ("sd_recv_rxfifo: DROP RX Data! size=%d\n", cnt));

	// read data from RXFIFO and drop!
	precvbuf = (struct recv_buf*)precvpriv->precv_buf_for_drop;
	while (cnt > 0) {
		blk_sz = (cnt > MAX_RECVBUF_SZ) ? MAX_RECVBUF_SZ : cnt;
		read_port(padapter, RTL8712_DMA_RX0FF, blk_sz, (u8*)precvbuf);
		cnt -= blk_sz;
	}

exit:
//	_exit_hwio_critical(&pdvobjpriv->rx_protect, &rx_proc_irq);
	return;
}

void sd_c2h_hdl(PADAPTER padapter)
{
	u8 cmd_seq,pkt_num=0,evt_code=0;
	u16 cmd_tag, evt_len=0;
	u16 c2h_blknum=0;
	u32 cnt,cmdpkt_len=0,cur_pos=0,pkt_len=0;//,ptr;
	struct evt_priv *pevtpriv=&padapter->evtpriv;

	c2h_blknum = padapter->dvobjpriv.c2hblknum;
	sdio_read_int(padapter, SDIO_C2H_RDYBLK_NUM, 2, &padapter->dvobjpriv.c2hblknum);
	RT_TRACE(_module_hci_intfs_c_, _drv_notice_,
		 ("+sd_c2h_hdl: c2h_blknum=0x%04x,0x%04x\n",
		  c2h_blknum, padapter->dvobjpriv.c2hblknum));

	if (c2h_blknum > padapter->dvobjpriv.c2hblknum)
		cnt = 0x10000 - c2h_blknum + padapter->dvobjpriv.c2hblknum;
	else
		cnt = padapter->dvobjpriv.c2hblknum - c2h_blknum;
	if (cnt == 0) goto exit;
	cnt *= padapter->dvobjpriv.block_transfer_len;
	RT_TRACE(_module_hci_intfs_c_, _drv_info_, ("sd_c2h_hdl: cnt=%d\n",cnt));

	_memset(pevtpriv->c2h_mem, 0, C2H_MEM_SZ);
	cur_pos = 0;
	//4 Read the first part of c2h event 
	if (cnt > C2H_MEM_SZ) {
		RT_TRACE(_module_hci_intfs_c_, _drv_emerg_, ("!sd_c2h_hdl: cnt=%d too big\n", cnt));
		goto exit;
	}
	read_port(padapter, RTL8712_DMA_C2HCMD, cnt, pevtpriv->c2h_mem);

get_next:
	//4 Check the c2h event tag [must be 0x1ff]
	cmd_tag = *(u16*)&pevtpriv->c2h_mem[cur_pos+4];
	cmd_tag = le16_to_cpu(cmd_tag);
	cmd_tag=cmd_tag &0x01ff;
	if(cmd_tag !=0x1ff){
		RT_TRACE(_module_hci_intfs_c_, _drv_err_, ("sd_c2h_hdl: 0x1ff error[0x%x]\n",pevtpriv->c2h_mem[cur_pos+4]));
		goto exit;
	}

	//4 Check the c2h packet length
	cmdpkt_len = *(u16*)&pevtpriv->c2h_mem[cur_pos+0];
	cmdpkt_len = le16_to_cpu(cmdpkt_len);
	cmdpkt_len=cmdpkt_len&0x3fff;
	pkt_len=cmdpkt_len+24;  //packet header is 24 bytes
	RT_TRACE(_module_hci_intfs_c_, _drv_info_, ("sd_c2h_hdl: cmd_sz=%d[0x%x]\n",cmdpkt_len,cmdpkt_len));
	
	//4 Check the cmd sequence
	cmd_seq=pevtpriv->c2h_mem[cur_pos+27];
	cmd_seq=cmd_seq&0x7f;
	if (pevtpriv->event_seq != cmd_seq) {
		RT_TRACE(_module_hci_intfs_c_, _drv_err_, ("sd_c2h_hdl: pevtpriv->event_seq(%d)!=c2hbuf seq(%d)\n",pevtpriv->event_seq,cmd_seq));
	} else {
		RT_TRACE(_module_hci_intfs_c_, _drv_info_, ("sd_c2h_hdl: pevtpriv->event_seq(%d)==c2hbuf seq(%d)\n",pevtpriv->event_seq,cmd_seq));
	}
	evt_code=pevtpriv->c2h_mem[cur_pos+26];
	evt_len=*(u16 *)&pevtpriv->c2h_mem[cur_pos+24];
	RT_TRACE(_module_hci_intfs_c_, _drv_info_, ("sd_c2h_hdl: evt_code=%d evt_len=%d\n", evt_code, evt_len));

	rxcmd_event_hdl(padapter,&pevtpriv->c2h_mem[cur_pos]);
	if((cnt-cur_pos-pkt_len)>512){
		cur_pos+=_RND512(pkt_len);
		RT_TRACE(_module_hci_intfs_c_, _drv_info_, ("sd_c2h_hdl: cur_pos=%d\n",cur_pos));
		goto get_next;

	}
exit:

	return;
} 
void cpwm_hdl(PADAPTER padapter){
	struct reportpwrstate_parm reportpwrstate;
	sdio_read_int(padapter,SDIO_HCPWM, 1, &reportpwrstate.state);
#ifdef CONFIG_PWRCTRL
	cpwm_int_hdl(padapter, &reportpwrstate);
#endif
	return;
}
void sd_int_dpc( PADAPTER	padapter)
{
	struct dvobj_priv *sddev=&padapter->dvobjpriv;
	uint tasks = (sddev->sdio_hisr);

	RT_TRACE(_module_hci_intfs_c_,_drv_notice_,("+sd_int_dpc[0x%x]\n",sddev->sdio_hisr));
	if(tasks & _C2HCMD)
	{
//              RT_TRACE(_module_hci_intfs_c_,_drv_err_,("INT : _C2HCMD "));
		sddev->sdio_hisr  ^= _C2HCMD;
		//RT_TRACE(_module_hci_intfs_c_,_drv_err_,("======C2H_CMD========"));
		sd_c2h_hdl(padapter);
	}

	if(tasks & _RXDONE)
	{
		RT_TRACE(_module_hci_intfs_c_,_drv_notice_,("==============INT : _RXDONE\n"));
		sddev->sdio_hisr  ^= _RXDONE;
		sd_recv_rxfifo(padapter) ;

	}
	if(tasks&_CPWM_INT){
		RT_TRACE(_module_hci_intfs_c_,_drv_notice_,("==============INT : _CPWM_INT\n"));
		sddev->sdio_hisr  ^= _CPWM_INT;
		cpwm_hdl(padapter);
	}
#if 0

	if((tasks & _VOQ_AVAL_IND) ||(tasks & _VIQ_AVAL_IND)||(tasks & _BEQ_AVAL_IND)||(tasks & _BKQ_AVAL_IND)||(tasks & _BMCQ_AVAL_IND)){
		RT_TRACE(_module_hci_intfs_c_,_drv_notice_,("==============INT : _TXDONE"));

	update_free_ffsz(padapter);
	}
	else{
		if(((padapter->xmitpriv.init_pgsz-padapter->xmitpriv.used_pgsz) >0 && (padapter->xmitpriv.init_pgsz-padapter->xmitpriv.used_pgsz) <0x2f )|| padapter->xmitpriv.required_pgsz >0){
		RT_TRACE(_module_hci_intfs_c_,_drv_notice_,("==============padapter->xmitpriv.public_pgsz[0x%x] <30 ",padapter->xmitpriv.public_pgsz));

		update_free_ffsz(padapter);

		}
	}
#endif

	return;
}


