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
#define _RTL8712_XMIT_C_
#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <rtl871x_byteorder.h>
#include <wifi.h>
#include <osdep_intf.h>
#include <circ_buf.h>

#if defined (PLATFORM_LINUX) && defined (PLATFORM_WINDOWS)
#error "Shall be Linux or Windows, but not both!\n"
#endif

#ifdef PLATFORM_WINDOWS
#include <if_ether.h>
#endif

#ifdef CONFIG_USB_HCI
#include <usb_ops.h>
#endif

sint _init_hw_txqueue(struct hw_txqueue* phw_txqueue, u8 ac_tag)
{

_func_enter_;

	phw_txqueue->ac_tag = ac_tag;

	switch(ac_tag)
	{
		case BE_QUEUE_INX:
			phw_txqueue->ff_hwaddr = RTL8712_DMA_BEQ;
			break;

		case BK_QUEUE_INX:
			phw_txqueue->ff_hwaddr = RTL8712_DMA_BKQ;
			break;

		case VI_QUEUE_INX:
			phw_txqueue->ff_hwaddr = RTL8712_DMA_VIQ;
			break;

		case VO_QUEUE_INX:
			phw_txqueue->ff_hwaddr = RTL8712_DMA_VOQ;
			break;

		case BMC_QUEUE_INX:
			//phw_txqueue->ff_hwaddr = RTL8712_DMA_BMCQ;
			phw_txqueue->ff_hwaddr = RTL8712_DMA_BEQ;
			break;

	}


_func_exit_;

	return _SUCCESS;

}

sint txframes_pending(_adapter *padapter)
{
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;

	return ((_queue_empty(&pxmitpriv->be_pending) == _FALSE) ||
			 (_queue_empty(&pxmitpriv->bk_pending) == _FALSE) ||
			 (_queue_empty(&pxmitpriv->vi_pending) == _FALSE) ||
			 (_queue_empty(&pxmitpriv->vo_pending) == _FALSE));
}

int txframes_sta_ac_pending(_adapter *padapter, struct pkt_attrib *pattrib)
{
	struct sta_info *psta;
	struct tx_servq *ptxservq;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	int priority = pattrib->priority;

	psta = pattrib->psta;

	switch(priority)
	{
			case 1:
			case 2:
				ptxservq = &(psta->sta_xmitpriv.bk_q);
				break;
			case 4:
			case 5:
				ptxservq = &(psta->sta_xmitpriv.vi_q);
				break;
			case 6:
			case 7:
				ptxservq = &(psta->sta_xmitpriv.vo_q);
				break;
			case 0:
			case 3:
			default:
				ptxservq = &(psta->sta_xmitpriv.be_q);
			break;

	}

	return ptxservq->qcnt;
}

u32 get_ff_hwaddr(struct xmit_frame	*pxmitframe)
{
	u32 addr;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	_adapter *padapter = pxmitframe->padapter;
	struct xmit_priv *pxmitpriv = &(padapter->xmitpriv);
	struct dvobj_priv	*pdvobj = (struct dvobj_priv *)&padapter->dvobjpriv;

#ifdef CONFIG_USB_HCI

	if(pxmitframe->frame_tag == TXAGG_FRAMETAG)
	{
		addr = RTL8712_DMA_H2CCMD;
	}
	else if(pxmitframe->frame_tag == MGNT_FRAMETAG)
	{
		addr = RTL8712_DMA_MGTQ;
	}
	else if(pdvobj->nr_endpoint == 6)
	{
		switch(pattrib->priority)
		{
			case 0:
			case 3:
				addr = RTL8712_DMA_BEQ;
			 	break;
			case 1:
			case 2:
				addr = RTL8712_DMA_BKQ;
				break;
			case 4:
			case 5:
				addr = RTL8712_DMA_VIQ;
				break;
			case 6:
			case 7:
				addr = RTL8712_DMA_VOQ;
				break;

			case 0x10:
			case 0x11:
			case 0x12:
			case 0x13://
				addr = RTL8712_DMA_H2CCMD;
				break;

			default:
				addr = RTL8712_DMA_BEQ;
				break;
		}
	}
	else if(pdvobj->nr_endpoint == 4)
	{
		switch(pattrib->qsel)
		{
			case 0:
			case 3:
			case 1:
			case 2:
				addr = RTL8712_DMA_BEQ;//RTL8712_EP_LO;
				break;

			case 4:
			case 5:
			case 6:
			case 7:
				addr = RTL8712_DMA_VOQ;//RTL8712_EP_HI;
				break;

			case 0x10:
			case 0x11:
			case 0x12:
			case 0x13://
				addr = RTL8712_DMA_H2CCMD;;
				break;

			default:
				addr = RTL8712_DMA_BEQ;//RTL8712_EP_LO;
				break;
		}
	}
#endif

#ifdef CONFIG_SDIO_HCI

	if(pxmitframe->frame_tag == TXAGG_FRAMETAG)
	{
		addr = RTL8712_DMA_H2CCMD;
	}
	else if(pxmitframe->frame_tag == MGNT_FRAMETAG)
	{
		addr = RTL8712_DMA_MGTQ;
	}
	else
	{
		switch(pattrib->priority)
		{
			case 0:
			case 3:
				addr = RTL8712_DMA_BEQ;
				break;

			case 1:
			case 2:
				addr = RTL8712_DMA_BKQ;
				break;

			case 4:
			case 5:
				addr = RTL8712_DMA_VIQ;
				break;

			case 6:
			case 7:
				addr = RTL8712_DMA_VOQ;
				break;

			case 0x13://
				addr = RTL8712_DMA_H2CCMD;
				break;

			default:
				addr = RTL8712_DMA_BEQ;
				break;
		}
	}

#endif
	RT_TRACE(_module_rtl8712_xmit_c_, _drv_notice_, ("get_ff_hwaddr,pattrib->priority=%x ,addr:0x%08x\n", pattrib->priority,addr));			
	return addr;
}

struct xmit_frame *dequeue_one_xmitframe(struct xmit_priv *pxmitpriv, struct hw_xmit *phwxmit, struct tx_servq *ptxservq, _queue *pframe_queue)
{
	_list	*xmitframe_plist, *xmitframe_phead;
	struct	xmit_frame	*pxmitframe=NULL;
	_adapter *padapter = pxmitpriv->adapter;

	xmitframe_phead = get_list_head(pframe_queue);
	xmitframe_plist = get_next(xmitframe_phead);

	if ((end_of_queue_search(xmitframe_phead, xmitframe_plist)) == _FALSE)
	{
		pxmitframe = LIST_CONTAINOR(xmitframe_plist, struct xmit_frame, list);

		list_delete(&pxmitframe->list);

		//list_insert_tail(&pxmitframe->list, &phwxmit->pending);

		ptxservq->qcnt--;
		phwxmit->txcmdcnt++;
	}

	return pxmitframe;
}

struct xmit_frame *dequeue_xframe(struct xmit_priv *pxmitpriv, struct hw_xmit *phwxmit, sint entry)
{
	int inx;
	_irqL irqL0;
	_list	*sta_plist, *sta_phead;
	struct tx_servq *ptxservq = NULL;
	_queue *pframe_queue = NULL;
	struct	xmit_frame	*pxmitframe=NULL;
	_adapter *padapter = pxmitpriv->adapter;

_func_enter_;

	for(inx = 0; inx < entry; inx++, phwxmit++)
	{
		_enter_critical_ex(&phwxmit->sta_queue->lock, &irqL0);

		sta_phead = get_list_head(phwxmit->sta_queue);
		sta_plist = get_next(sta_phead);

		while ((end_of_queue_search(sta_phead, sta_plist)) == _FALSE)
		{
			ptxservq= LIST_CONTAINOR(sta_plist, struct tx_servq, tx_pending);

			pframe_queue = &ptxservq->sta_pending;

			//_enter_critical(&pframe_queue->lock, &irqL1);

			if(1/*(bamsdu_enabled == _FALSE) && (btxagg_enabled == _FALSE)*/)
			{
				pxmitframe = dequeue_one_xmitframe(pxmitpriv, phwxmit, ptxservq, pframe_queue);
			}
			else if(0/* btxagg_enabled == _TRUE*/)
			{
				//pxmitframe = dequeue_agg_xmitframe(pxmitpriv, phwxmit, ptxservq, pframe_queue);

				//if(pxmitframe == NULL)
				//{
				//	pxmitframe = handle_single_xmitframe(pxmitpriv, phwxmit, ptxservq, pframe_queue);
				//}
			}
			else if(0/*bamsdu_enabled == _TRUE*/)
			{
				//pxmitframe = dequeue_amsdu_xmitframe(pxmitpriv, phwxmit, ptxservq, pframe_queue);
			}

			//_exit_critical(&pframe_queue->lock, &irqL1);

			if(pxmitframe)
			{
				phwxmit->accnt--;

				_exit_critical_ex(&phwxmit->sta_queue->lock, &irqL0);

				goto _exit_dequeue_one_xmitframe;
			}
			else
			{
				RT_TRACE(_module_rtl8712_xmit_c_, _drv_debug_, ("pxmitframe=0x%p\n", pxmitframe));	
			}

			sta_plist = get_next(sta_plist);

			//Remove sta node when there is no pending packets.
			if(_queue_empty(pframe_queue)) //must be done after get_next and before break
				list_delete(&ptxservq->tx_pending);
		}

		_exit_critical_ex(&phwxmit->sta_queue->lock, &irqL0);
	}

_exit_dequeue_one_xmitframe:

_func_exit_;

	return pxmitframe;
}

struct xmit_frame *dequeue_xframe_ex(struct xmit_priv *pxmitpriv, struct hw_xmit *phwxmit_i, sint entry)
{
	_irqL irqL0;
	_list	*sta_plist, *sta_phead;
	struct hw_xmit *phwxmit;
	struct tx_servq *ptxservq = NULL;
	_queue *pframe_queue = NULL;
	struct	xmit_frame	*pxmitframe=NULL;
	_adapter *padapter = pxmitpriv->adapter;
	int i, inx[4];
#ifdef CONFIG_USB_HCI
	int j, tmp, acirp_cnt[4];
#endif

_func_enter_;

#ifdef CONFIG_USB_HCI
	//entry indx: 0->vo, 1->vi, 2->be, 3->bk.
	inx[0] = 0; acirp_cnt[0] = pxmitpriv->voq_cnt;
	inx[1] = 1; acirp_cnt[1] = pxmitpriv->viq_cnt;
	inx[2] = 2; acirp_cnt[2] = pxmitpriv->beq_cnt;
	inx[3] = 3; acirp_cnt[3] = pxmitpriv->bkq_cnt;

	for(i=0; i<4; i++)
	{
		for(j=i+1; j<4; j++)
		{
			if(acirp_cnt[j]<acirp_cnt[i])
			{
				tmp = acirp_cnt[i];
				acirp_cnt[i] = acirp_cnt[j];
				acirp_cnt[j] = tmp;

				tmp = inx[i];
				inx[i] = inx[j];
				inx[j] = tmp;
			}
		}
	}

#else
 	inx[0] = 0; inx[1] = 1; inx[2] = 2; inx[3] = 3;
#endif

	_enter_critical(&pxmitpriv->lock, &irqL0);

	for(i = 0; i < entry; i++)
	{
		phwxmit = phwxmit_i + inx[i];

		//_enter_critical_ex(&phwxmit->sta_queue->lock, &irqL0);

		sta_phead = get_list_head(phwxmit->sta_queue);
		sta_plist = get_next(sta_phead);

		while ((end_of_queue_search(sta_phead, sta_plist)) == _FALSE)
		{

			ptxservq= LIST_CONTAINOR(sta_plist, struct tx_servq, tx_pending);

			pframe_queue = &ptxservq->sta_pending;

			pxmitframe = dequeue_one_xmitframe(pxmitpriv, phwxmit, ptxservq, pframe_queue);

			if(pxmitframe)
			{
				phwxmit->accnt--;

				//_exit_critical_ex(&phwxmit->sta_queue->lock, &irqL0);

				goto exit_dequeue_xframe_ex;
			}

			sta_plist = get_next(sta_plist);

			//Remove sta node when there is no pending packets.
			if(_queue_empty(pframe_queue)) //must be done after get_next and before break
				list_delete(&ptxservq->tx_pending);

		}

		//_exit_critical_ex(&phwxmit->sta_queue->lock, &irqL0);

	}

exit_dequeue_xframe_ex:

	_exit_critical(&pxmitpriv->lock, &irqL0);

_func_exit_;

	return pxmitframe;
}

void do_queue_select(_adapter	*padapter, struct pkt_attrib *pattrib)
{

#ifdef CONFIG_USB_HCI

	u8	qsel;
	struct xmit_priv *pxmitpriv = &(padapter->xmitpriv);
	struct dvobj_priv	*pdvobj = (struct dvobj_priv *)&padapter->dvobjpriv;

	if(pdvobj->nr_endpoint == 6)
	{
		qsel = pattrib->priority;
	}
	else if(pdvobj->nr_endpoint == 4)
#ifndef CONFIG_WIFI_WMM_TEST
	{
		qsel = pattrib->priority;
	}
#else
	{
		switch(pattrib->priority)
		{
			case 0:
			case 3:
				if(pxmitpriv->bkq_cnt!=0 && pxmitpriv->viq_cnt==0 && pxmitpriv->voq_cnt==0)
				{
					qsel = 0x06;//select vi/vo (high_q)
				}
				else
				{
					//qsel = pattrib->priority;
					qsel = 0;
				}
				break;

			case 1:
			case 2:
				//qsel = pattrib->priority;
				qsel = 1;
				break;

			case 4:
			case 5:
				if(pxmitpriv->voq_cnt!=0)
				{
					qsel = 0x0;//selev be/bk (low_q)
				}
				else
				{
					//qsel = pattrib->priority;
					qsel = 4;
				}
				break;

			case 6:
			case 7:
				//qsel = pattrib->priority;
				qsel = 6;
				break;

			case 0x10:
			case 0x11:
			case 0x12:
			case 0x13://
				//qsel = 0x13;
				qsel = pattrib->priority;

				break;

			default:
				qsel = 0x0;
				break;
		}
	}
#endif

	pattrib->qsel = qsel;

#endif

}

static int toggle_be=0;
static int toggle_bk=0;

int check_xmit_resource(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	int bq=0;
	struct xmit_priv *pxmitpriv = &(padapter->xmitpriv);

	//todo: accordding to the AC(TID) of  txframes to check the each txframes_pending queue.

	//Notes: in winxp os, the usb_wirte_port_complete is at dispatch level, so it needs not to protect
	// 		when checking txframes_pending

#ifdef CONFIG_USB_HCI
	int ac_txirp_cnt, ac_cnt;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	struct hw_xmit *phwxmits =  pxmitpriv->hwxmits;

	do_queue_select(padapter, pattrib);

	switch(pattrib->priority)
	{
		case 1:
		case 2:
			//printk("pxmitpriv->bkq_cnt=%d\n", pxmitpriv->bkq_cnt);
			ac_txirp_cnt=pxmitpriv->bkq_cnt;
			ac_cnt = (phwxmits+3)->accnt;
			break;

		case 4:
		case 5:
			//printk("pxmitpriv->viq_cnt=%d\n", pxmitpriv->viq_cnt);
			ac_txirp_cnt = pxmitpriv->viq_cnt;
			ac_cnt = (phwxmits+1)->accnt;
			break;

		case 6:
		case 7:
			//printk("pxmitpriv->voq_cnt=%d\n", pxmitpriv->voq_cnt);
			ac_txirp_cnt = pxmitpriv->voq_cnt;
			ac_cnt = (phwxmits+0)->accnt;
			break;

		case 0:
		case 3:
		default:
			//printk("pxmitpriv->beq_cnt=%d\n", pxmitpriv->beq_cnt);
			ac_txirp_cnt = pxmitpriv->beq_cnt;
			ac_cnt = (phwxmits+2)->accnt;
			break;
	}


	if(pxmitpriv->free_xmitframe_cnt<(NR_XMITFRAME>>3))
	{
		switch(pattrib->priority)
		{
			case 1:
			case 2:
				if((toggle_bk++)%4==0)
				{
					//printk("irpcnt=%d, accnt=%d tid=%d,toggle=%d\n",ac_txirp_cnt, ac_cnt, pattrib->priority, toggle);
					bq = (-1);//drop packet
				}
				else
				{
					bq = 1;
				}

				break;

			case 4:
			case 5:
				bq = 1;
				break;

			case 6:
			case 7:
				bq = 1;
				break;

			case 0:
			case 3:
			default:
				if((toggle_be++)%4==0)
				{
					//printk("irpcnt=%d, accnt=%d tid=%d,toggle=%d\n",ac_txirp_cnt, ac_cnt, pattrib->priority, toggle);
					bq = (-1);//drop packet
				}
				else
				{
					bq = 1;
				}
				break;
		}
	}
	else if(txframes_pending(padapter))
	{
		if(pxmitpriv->txirp_cnt==1)
		{
			//printk("pxmitpriv->txirp_cnt==1\n");
		}

		bq = 1;//enqueue packet
	}
	else if(ac_txirp_cnt > 1)//pxmitpriv->txirp_cnt
	{
		bq = 1;//enqueue packet
	}
	else
	{
		bq = 0;//dump packet directly
	}

#endif //#ifdef CONFIG_USB_HCI

#ifdef CONFIG_SDIO_HCI

	u32 pkt_len;
	u8 blk_num,pagenum_required;
	struct hw_xmit *phwxmit=pxmitpriv->hwxmits;
	struct hw_txqueue *ptxhwqueue;
	pkt_len=pxmitframe->attrib.pktlen+pxmitframe->attrib.hdrlen+TXDESC_SIZE;
	blk_num=(u8)((pkt_len>>9)+((pkt_len%512 ==0)? 0:1));
	RT_TRACE(_module_rtl8712_xmit_c_,_drv_debug_,("pkt_len =%d;blk_num=%d",  pkt_len, blk_num));

	pxmitframe->attrib.qsel = (uint)pxmitframe->attrib.priority;

	switch(pxmitframe->attrib.priority){
		case 0: //BE
		case 3:
			ptxhwqueue=&pxmitpriv->be_txqueue;
			break;
		case 1: //BK
		case 2:
			ptxhwqueue=&pxmitpriv->bk_txqueue;
			break;
		case 4: //VI
		case 5:
			ptxhwqueue=&pxmitpriv->vi_txqueue;
			break;
		case 6: //VO
		case 7:
			ptxhwqueue=&pxmitpriv->vo_txqueue;
			break;
	}
	pagenum_required=blk_num*2;
#if 0
	if(ptxhwqueue->free_sz >(pagenum_required+5)){
		RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,(" drv's free_sz is  enough; 0x%x(%d) (need page num :%d)\n", ptxhwqueue->free_sz,ptxhwqueue->free_sz,pagenum_required));
		ptxhwqueue->free_sz=ptxhwqueue->free_sz-pagenum_required;
		RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,(" drv's free_sz is  enough; 0x%x(%d) (need page num :%d)\n", ptxhwqueue->free_sz,ptxhwqueue->free_sz,pagenum_required));
	}
	else {
		s32	tmp;
		if((ptxhwqueue->free_sz+pxmitpriv->public_pgsz) >(pagenum_required+5)) {
		// drv_xmitbuf enough
		RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,(" drv's free_sz is  enough; ptxhwqueue->free_sz=%d   xmitpriv->public_pgsz=%d pagenum=%d\n", ptxhwqueue->free_sz,pxmitpriv->public_pgsz,pagenum_required));
			if(pagenum_required>ptxhwqueue->free_sz){
				tmp=pagenum_required-ptxhwqueue->free_sz;
				pxmitpriv->public_pgsz-=(u8)(pagenum_required-ptxhwqueue->free_sz);
				ptxhwqueue->free_sz=0;
			}
			else{
				ptxhwqueue->free_sz-=pagenum_required;
			}
		}
		else{
			RT_TRACE(_module_rtl8712_xmit_c_,_drv_err_,(" drv's free_sz is not enough;  ptxhwqueue->free_sz=%d   xmitpriv->public_pgsz=%d pagenum=%d\n", ptxhwqueue->free_sz,pxmitpriv->public_pgsz,pagenum_required));
			bq=1;
//			read_free_ffsz();
		}
	}
#else
	pxmitframe->pg_num=0;
	if(pxmitpriv->init_pgsz >(pxmitpriv->used_pgsz +pxmitpriv->required_pgsz +pagenum_required+5)){
RT_TRACE(_module_rtl8712_xmit_c_,_drv_err_,(" drv's free_sz is enough;  pxmitpriv->required_pgsz=0x%x \n", pxmitpriv->required_pgsz ));

		pxmitpriv->required_pgsz =pxmitpriv->required_pgsz +pagenum_required;
	//	pxmitpriv->public_pgsz=pxmitpriv->public_pgsz-(u8)pagenum_required;
		pxmitframe->pg_num=pagenum_required;

	}
	else{
			RT_TRACE(_module_rtl8712_xmit_c_,_drv_err_,(" drv's free_sz is not enough;  ptxhwqueue->free_sz=%d   xmitpriv->public_pgsz=%d pagenum=%d\n", ptxhwqueue->free_sz,pxmitpriv->public_pgsz,pagenum_required));
			bq=1;
	}

#endif
#endif

	return bq;
}

#ifdef CONFIG_HOSTAPD_MODE
void update_txdesc_ex(struct xmit_frame *pxmitframe, struct tx_desc *ptxdesc)
{
	uint qsel;
	struct sta_info *psta = NULL;
	struct ht_priv	*pht = NULL;
	_adapter	*padapter = pxmitframe->padapter;
	struct security_priv *psecuritypriv=&padapter->securitypriv;
	struct sta_priv *pstapriv = &padapter->stapriv;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	int	bmcst = IS_MCAST(pattrib->ra);


	if(bmcst)
	{
		psta = get_bcmc_stainfo(padapter);
		pattrib->mac_id = 0;//default set to 0
	}
	else
	{
		psta = get_stainfo(pstapriv, pattrib->ra);
	}

	if(psta==NULL)
		psta = pattrib->psta;

	pht = &psta ->htpriv;

	//ptxdesc->txdw1 |= cpu_to_le32((psta->mac_id)&0x1f);
	ptxdesc->txdw1 |= cpu_to_le32((pattrib->mac_id)&0x1f);//psta->mac_id == pattrib->mac_id

	qsel = (uint)(pattrib->qsel&0x0000001f);
	ptxdesc->txdw1 |= cpu_to_le32((qsel<<QSEL_SHT)&0x00001f00);

	if(!psta->qos_option)
		ptxdesc->txdw1 |= cpu_to_le32(BIT(16));//Non-QoS

	if(pattrib->encrypt	>0 && !pattrib->bswenc)
	{
		switch(pattrib->encrypt)
		{	//SEC_TYPE

			case _WEP40_:
			case _WEP104_:
				ptxdesc->txdw1 |= cpu_to_le32((0x01<<22)&0x00c00000);
				//KEY_ID when WEP is used;
				ptxdesc->txdw1 |= cpu_to_le32((psecuritypriv->dot11PrivacyKeyIndex<<17)&0x00060000);
				break;
			case _TKIP_:
			case _TKIP_WTMIC_:
					ptxdesc->txdw1 |= cpu_to_le32((0x02<<22)&0x00c00000);
					break;
			case _AES_:
					ptxdesc->txdw1 |= cpu_to_le32((0x03<<22)&0x00c00000);
					break;
			case _NO_PRIVACY_:
			default:
					break;
		}
	}

	if(bmcst)
	{
		ptxdesc->txdw2 |= cpu_to_le32(BMC);
	}

	ptxdesc->txdw3 = ((pattrib->seqnum<<SEQ_SHT)&0x0fff0000);


	if( ( pattrib->ether_type != 0x888e ) && ( pattrib->ether_type != 0x0806 ) &&(pattrib->dhcp_pkt != 1) )
	{
		//Not EAP & ARP type data packet
#ifdef CONFIG_80211N_HT
		if(pht->ht_option==1) //B/G/N Mode
		{
			if(pht->ampdu_enable != _TRUE)
			{
				ptxdesc->txdw2 |= cpu_to_le32(BK);
			}

			//ptxdesc->txdw4 = cpu_to_le32(0x80000000);//driver uses data rate
			//ptxdesc->txdw4 |= cpu_to_le32(BIT(18)|BIT(20));
			//ptxdesc->txdw5 = cpu_to_le32(0x001f2600);// MCS7
			//ptxdesc->txdw5 = cpu_to_le32(0x001f3600);// MCS15
		}
		else
#endif
		{
			//ptxdesc->txdw4 = 0x80000000;//driver uses data rate
			//ptxdesc->txdw5 = 0x001f1600;// 54M
			//ptxdesc->txdw5 = 0x001f2600;//MCS7
			//ptxdesc->txdw5 = 0x001f9600;// 54M
		}
	}
	else
	{
		// EAP data packet and ARP packet.
		// Use the 1M data rate to send the EAP/ARP packet.
		// This will maybe make the handshake smooth.

		ptxdesc->txdw4 = cpu_to_le32(0x80000000);//driver uses data rate
		ptxdesc->txdw5 = cpu_to_le32(0x001f8000);// 1M

	}

}
#endif

void update_txdesc(struct xmit_frame *pxmitframe, uint *pmem, int sz)
{
	uint qsel;
	_adapter		*padapter = pxmitframe->padapter;
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct qos_priv		*pqospriv = &pmlmepriv->qospriv;
	struct security_priv	*psecuritypriv = &padapter->securitypriv;
	struct pkt_attrib	*pattrib = &pxmitframe->attrib;
	struct tx_desc		*ptxdesc = (struct tx_desc*)pmem;
#ifdef   CONFIG_USB_HCI
	struct dvobj_priv	*pdvobj = (struct dvobj_priv*)&padapter->dvobjpriv;
	u8 blnSetTxDescOffset;
#endif

	sint bmcst = IS_MCAST(pattrib->ra);

#ifdef CONFIG_80211N_HT
	struct ht_priv *phtpriv = &pmlmepriv->htpriv;
#endif

#ifdef CONFIG_MP_INCLUDED
	struct tx_desc txdesc_mp;
	_memcpy(&txdesc_mp, ptxdesc, sizeof(struct tx_desc));
#endif

	_memset(ptxdesc, 0, sizeof(struct tx_desc));

	//offset 0
	ptxdesc->txdw0 |= cpu_to_le32(sz&0x0000ffff);

#ifdef CONFIG_USB_HCI
	if ( pdvobj->ishighspeed )
	{
		if ( ( (sz + TXDESC_SIZE) % 512 ) == 0 ) {
			blnSetTxDescOffset = 1;
		} else {
			blnSetTxDescOffset = 0;
		}
	}
	else
	{
		if ( ( (sz + TXDESC_SIZE) % 64 ) == 0 ) 	{
			blnSetTxDescOffset = 1;
		} else {
			blnSetTxDescOffset = 0;
		}
	}

	if (blnSetTxDescOffset)
	{
	    ptxdesc->txdw0 |= cpu_to_le32(((TXDESC_SIZE+OFFSET_SZ+8)<<OFFSET_SHT)&0x00ff0000);//32 bytes for TX Desc + 8 bytes pending
	}
	else
#endif
	{
	    ptxdesc->txdw0 |= cpu_to_le32(((TXDESC_SIZE+OFFSET_SZ)<<OFFSET_SHT)&0x00ff0000);//default = 32 bytes for TX Desc
	}

	ptxdesc->txdw0 |= cpu_to_le32(OWN | FSG | LSG);

	RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,("offset0-txdesc=0x%x\n", ptxdesc->txdw0));

#ifdef CONFIG_HOSTAPD_MODE
	if (check_fwstate(pmlmepriv, WIFI_AP_STATE) == _TRUE)
	{
		update_txdesc_ex(pxmitframe, ptxdesc);
		return;
	}
#endif

	if (pxmitframe->frame_tag == DATA_FRAMETAG)
	{
		//offset 4

//		ptxdesc->txdw1 |= (0x05)&0x1f;//CAM_ID(MAC_ID), default=5;
		ptxdesc->txdw1 |= cpu_to_le32((pattrib->mac_id)&0x1f);//CAM_ID(MAC_ID)

		qsel = (uint)(pattrib->qsel & 0x0000001f);
		ptxdesc->txdw1 |= cpu_to_le32((qsel << QSEL_SHT) & 0x00001f00);

		if (!pqospriv->qos_option)
			ptxdesc->txdw1 |= cpu_to_le32(BIT(16));//Non-QoS

		if ((pattrib->encrypt > 0) && !pattrib->bswenc)
		{
			switch (pattrib->encrypt)
			{	//SEC_TYPE
				case _WEP40_:
				case _WEP104_:
					ptxdesc->txdw1 |= cpu_to_le32((0x01<<22)&0x00c00000);

					//KEY_ID when WEP is used;
					ptxdesc->txdw1 |= cpu_to_le32((psecuritypriv->dot11PrivacyKeyIndex<<17)&0x00060000);
					break;

				case _TKIP_:
				case _TKIP_WTMIC_:
					ptxdesc->txdw1 |= cpu_to_le32((0x02<<22)&0x00c00000);
					break;

				case _AES_:
					ptxdesc->txdw1 |= cpu_to_le32((0x03<<22)&0x00c00000);
					break;

				case _NO_PRIVACY_:
				default:
					break;
			}
		}

		RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,("offset4-txdesc=0x%x\n", ptxdesc->txdw1));

		//offset 8
		//ptxdesc->txdw2 |= AGG_EN;
		//ptxdesc->txdw2 |= cpu_to_le32(BK);
		if (bmcst) {
			ptxdesc->txdw2 |= cpu_to_le32(BMC);
		}

		//offset 12
		//f/w will increase the seqnum by itself, driver pass the correct priority to fw
		//fw will check the correct priority for increasing the seqnum per tid.
		//about usb using 4-endpoint, qsel points out the correct mapping between AC&Endpoint,
		//the purpose is that correct mapping let the MAC releases the AC Queue list correctly.
		//ptxdesc->txdw3 = ((pattrib->seqnum<<SEQ_SHT)&0x0fff0000);
		ptxdesc->txdw3 = cpu_to_le32((pattrib->priority<<SEQ_SHT)&0x0fff0000);

		RT_TRACE(_module_rtl8712_xmit_c_, _drv_notice_, ("offsetC-txdesc=0x%x\n", ptxdesc->txdw3));

		//offset 16
		//ptxdesc->txdw4 = 0x80002040;//gtest
		//ptxdesc->txdw4 |= TXBW;
		//ptxdesc->txdw4 = 0x80000000;//gtest
		//ptxdesc->txdw4 = 0x80000800;//CTS-TO-SELF
		//ptxdesc->txdw4 = 0x00020000;//

		//offset 20
		//ptxdesc->txdw5 = 0x001f9600;//gtest//54M
		//ptxdesc->txdw5 = 0x001f8000;//gtest//1M
		//ptxdesc->txdw5 = 0x001f1600;//gtest//54M
		//ptxdesc->txdw5 = 0x001f2600;//gtest//MCS7

		if ((pattrib->ether_type != 0x888e) && (pattrib->ether_type != 0x0806) && (pattrib->dhcp_pkt != 1))
		{
			//Not EAP & ARP type data packet

#ifdef CONFIG_80211N_HT
			if(phtpriv->ht_option==1) //B/G/N Mode
			{
				if(phtpriv->ampdu_enable != _TRUE) {
					ptxdesc->txdw2 |= cpu_to_le32(BK);
				}

				//ptxdesc->txdw4 = cpu_to_le32(0x80000000);//driver uses data rate
				//ptxdesc->txdw4 |= cpu_to_le32(BIT(18)|BIT(20));
				//ptxdesc->txdw5 = cpu_to_le32(0x001f2600);// MCS7
				//ptxdesc->txdw5 = cpu_to_le32(0x001f3600);// MCS15
			}
			else
#endif
			{
				//ptxdesc->txdw4 = 0x80000000;//driver uses data rate
				//ptxdesc->txdw5 = 0x001f1600;// 54M
				//ptxdesc->txdw5 = 0x001f2600;//MCS7
				//ptxdesc->txdw5 = 0x001f9600;// 54M
			}
		}
		else
		{
			// EAP data packet and ARP packet.
			// Use the 1M data rate to send the EAP/ARP packet.
			// This will maybe make the handshake smooth.

			ptxdesc->txdw4 = cpu_to_le32(0x80000000);//driver uses data rate
			ptxdesc->txdw5 = cpu_to_le32(0x001f8000);// 1M
		}

#ifdef CONFIG_MP_INCLUDED
		if (pattrib->pctrl == 1) // mp tx packets
		{
			struct tx_desc *ptxdesc_mp;

			ptxdesc_mp = &txdesc_mp;

			// offset 8
			ptxdesc->txdw2 = cpu_to_le32(ptxdesc_mp->txdw2);
			if (bmcst) ptxdesc->txdw2 |= cpu_to_le32(BMC);
			ptxdesc->txdw2 |= cpu_to_le32(BK);
//RT_TRACE(_module_rtl8712_xmit_c_,_drv_alert_,("mp pkt offset8-txdesc=0x%8x\n", ptxdesc->txdw2));			

			// offset 16
			ptxdesc->txdw4 = cpu_to_le32(ptxdesc_mp->txdw4);
//RT_TRACE(_module_rtl8712_xmit_c_,_drv_alert_,("mp pkt offset16-txdesc=0x%8x\n", ptxdesc->txdw4));

			// offset 20
			ptxdesc->txdw5 = cpu_to_le32(ptxdesc_mp->txdw5);
//RT_TRACE(_module_rtl8712_xmit_c_,_drv_alert_,("mp pkt offset20-txdesc=0x%8x\n", ptxdesc->txdw5));				

			pattrib->pctrl = 0;//reset to zero;
		}
#endif

#ifdef CONFIG_RTL8712_TCP_CSUM_OFFLOAD_TX
		if ( pattrib->hw_tcp_csum == 1 ) {
			// ptxdesc->txdw6 = 0; // clear TCP_CHECKSUM and IP_CHECKSUM. It's zero already!!
			u8 ip_hdr_offset = 32 + pattrib->hdrlen + pattrib->iv_len + 8;
			ptxdesc->txdw7 = (1 << 31) | (ip_hdr_offset << 16);
			printk(KERN_DEBUG "ptxdesc->txdw7 = %08x\n", ptxdesc->txdw7);
		}
#endif
	}
	else if(pxmitframe->frame_tag == MGNT_FRAMETAG)
	{
		//printk("pxmitframe->frame_tag == MGNT_FRAMETAG\n");

		//offset 4
		ptxdesc->txdw1 |= (0x05)&0x1f;//CAM_ID(MAC_ID), default=5;
		//ptxdesc->txdw1 |= cpu_to_le32((pattrib->mac_id)&0x1f);//CAM_ID(MAC_ID)

		qsel = (uint)(pattrib->qsel&0x0000001f);

		ptxdesc->txdw1 |= cpu_to_le32((qsel<<QSEL_SHT)&0x00001f00);

		//if(!pqospriv->qos_option)
		ptxdesc->txdw1 |= cpu_to_le32(BIT(16));//Non-QoS

		//offset 8
		//ptxdesc->txdw2 |= AGG_EN;
		//ptxdesc->txdw2 |= cpu_to_le32(BK);
		if(bmcst)
		{
			ptxdesc->txdw2 |= cpu_to_le32(BMC);
		}

		//offset 12
		//f/w will increase the seqnum by itself, driver pass the correct priority to fw
		//fw will check the correct priority for increasing the seqnum per tid.
		//about usb using 4-endpoint, qsel points out the correct mapping between AC&Endpoint,
		//the purpose is that correct mapping let the MAC releases the AC Queue list correctly.
		//ptxdesc->txdw3 = ((pattrib->seqnum<<SEQ_SHT)&0x0fff0000);
		ptxdesc->txdw3 = cpu_to_le32((pattrib->priority<<SEQ_SHT)&0x0fff0000);

		RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,("offsetC-txdesc=0x%x\n", ptxdesc->txdw3));

		//offset 16
		ptxdesc->txdw4 = cpu_to_le32(0x80002040);//gtest
		//ptxdesc->txdw4 = 0x80001000;//
		//ptxdesc->txdw4 = 0x80000800;//
		//ptxdesc->txdw4 = 0x80000000;//
		//ptxdesc->txdw4 |= TXBW;

		//offset 20
		//ptxdesc->txdw5 = 0x001f8800;//gtest//6M
		ptxdesc->txdw5 = cpu_to_le32(0x001f8000);//gtest//1M
		//ptxdesc->txdw5 = 0x001f0000;//gtest//1M
	}
	else if(pxmitframe->frame_tag == TXAGG_FRAMETAG)
	{
		//printk("pxmitframe->frame_tag == TXAGG_FRAMETAG\n");

		//offset 4
		qsel = 0x13;
		ptxdesc->txdw1 |= cpu_to_le32((qsel<<QSEL_SHT)&0x00001f00);
	}
	else
	{
		//printk("pxmitframe->frame_tag = %d\n", pxmitframe->frame_tag);

		//offset 4
		qsel = (uint)(pattrib->priority&0x0000001f);
		ptxdesc->txdw1 |= cpu_to_le32((qsel<<QSEL_SHT)&0x00001f00);

		//offset 8
		//ptxdesc->txdw2 |= AGG_EN;

		//offset 12
		ptxdesc->txdw3 = cpu_to_le32((pattrib->seqnum<<SEQ_SHT)&0x0fff0000);

		RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,("offsetC-txdesc=0x%x\n", ptxdesc->txdw3));

		//offset 16
		ptxdesc->txdw4 = cpu_to_le32(0x80002040);//gtest
		//ptxdesc->txdw4 |= TXBW;

		//offset 20
		ptxdesc->txdw5 = cpu_to_le32(0x001f9600);//gtest
	}

}

int xmitframe_complete(_adapter *padapter, struct xmit_priv *pxmitpriv, struct xmit_buf *pxmitbuf)
{
	struct hw_xmit *phwxmits;
	sint hwentry;
	struct xmit_frame *pxmitframe=NULL;
	int res=_SUCCESS, xcnt = 0;

	phwxmits = pxmitpriv->hwxmits;
	hwentry = pxmitpriv->hwxmit_entry;

	RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,("xmitframe_complete()\n"));

	if(pxmitbuf==NULL)
	{
		pxmitbuf = alloc_xmitbuf(pxmitpriv);
		if(!pxmitbuf)
		{
			return _FALSE;
		}
	}

	do
	{
		//pxmitframe =  dequeue_xframe(pxmitpriv, phwxmits, hwentry);
		pxmitframe =  dequeue_xframe_ex(pxmitpriv, phwxmits, hwentry);

		if(pxmitframe)
		{
			pxmitframe->pxmitbuf = pxmitbuf;

#ifdef CONFIG_USB_HCI

#if defined(PLATFORM_OS_XP)||defined(PLATFORM_LINUX)

			pxmitframe->pxmit_urb[0] = pxmitbuf->pxmit_urb[0];
#endif

#ifdef PLATFORM_OS_XP
			pxmitframe->pxmit_irp[0] = pxmitbuf->pxmit_irp[0];
#endif

#endif
			pxmitframe->buf_addr = pxmitbuf->pbuf;

			//pxmitbuf->priv_data = pxmitframe;	

			if(pxmitframe->frame_tag == DATA_FRAMETAG)
			{
				if(pxmitframe->attrib.priority<=15)//TID0~15
				{
					res = xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);
				}

				//printk("xmitframe_complete(): os_xmit_complete\n");
				os_xmit_complete(padapter, pxmitframe);//always return ndis_packet after xmitframe_coalesce
			}

			RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,("xmitframe_complete(): dump_xframe\n"));

			//_enter_critical(&pxmitpriv->lock, &irqL);
			if(res == _SUCCESS)
			{
			dump_xframe(padapter, pxmitframe);
			}
			else
			{
				free_xmitframe_ex(pxmitpriv, pxmitframe);
			}
			//_exit_critical(&pxmitpriv->lock, &irqL);

			xcnt++;
		}
		else
		{
			//printk("xmitframe_complete()->free_xmitbuf()\n");
			free_xmitbuf(pxmitpriv, pxmitbuf);
			return _FALSE;
		}

		break;
	}while(0/*xcnt < (NR_XMITFRAME >> 3)*/);

	return _TRUE;
}

void dump_xframe(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	int t, sz, w_sz;
	u8 *mem_addr;
	u32 ff_hwaddr;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;
	struct security_priv *psecuritypriv = &padapter->securitypriv;
	struct dvobj_priv *pdvobj = (struct dvobj_priv*)&padapter->dvobjpriv;

//#ifdef CONFIG_USB_HCI		
//	pxmitframe->irpcnt= pattrib->nr_frags;
//#endif

#ifdef CONFIG_80211N_HT
	if (pxmitframe->attrib.ether_type != 0x0806)
	{
		if (pxmitframe->attrib.ether_type != 0x888e)
		{
			issue_addbareq_cmd(padapter, pattrib->priority);
		}
	}
#endif

	//mem_addr = ((unsigned char *)pxmitframe->mem) + WLANHDR_OFFSET;
	mem_addr = pxmitframe->buf_addr;

        RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,("dump_xframe()\n"));
	//printk("dump_xframe()\n");

	for (t = 0; t < pattrib->nr_frags; t++)
	{
		if (t != (pattrib->nr_frags - 1))
		{
			RT_TRACE(_module_rtl8712_xmit_c_,_drv_err_,("pattrib->nr_frags=%d\n", pattrib->nr_frags));

			sz = pxmitpriv->frag_len;
			sz = sz - 4 - (psecuritypriv->sw_encrypt ? 0 : pattrib->icv_len);

#ifdef CONFIG_USB_HCI
			pxmitframe->last[t] = 0;
#endif
		}
		else
		{
			sz = pattrib->last_txcmdsz;

#ifdef CONFIG_USB_HCI
			pxmitframe->last[t] = 1;
#endif
		}

		update_txdesc(pxmitframe, (uint*)mem_addr, sz);

		w_sz = sz + TXDESC_SIZE;

#ifdef CONFIG_USB_HCI

		pxmitframe->mem_addr = mem_addr;

		//_enter_critical(&pxmitpriv->lock, &irqL);
		//pxmitframe->irpcnt--;
		//pxmitframe->fragcnt++;
		pxmitframe->bpending[t] = _FALSE;
		//_exit_critical(&pxmitpriv->lock, &irqL);

#endif //#ifdef CONFIG_USB_HCI

		ff_hwaddr = get_ff_hwaddr(pxmitframe);

#ifdef PLATFORM_OS_CE
		write_port(padapter, ff_hwaddr, w_sz, (unsigned char*)mem_addr);
#else
		write_port(padapter, ff_hwaddr, w_sz, (unsigned char*)pxmitframe);
#endif

		RT_TRACE(_module_rtl8712_xmit_c_,_drv_info_,("write_port, w_sz=%d\n", w_sz));
		//printk("write_port, w_sz=%d, sz=%d, txdesc_sz=%d, tid=%d\n", w_sz, sz, w_sz-sz, pattrib->priority);

		mem_addr += w_sz;

		mem_addr = (u8 *)RND4(((addr_t)(mem_addr)));
	}

	//xmitframe_complete(padapter, pxmitpriv);
}

#ifdef CONFIG_SDIO_HCI
void update_free_ffsz(_adapter *padapter)
{
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;

	RT_TRACE(_module_rtl8712_xmit_c_, _drv_notice_,
		 ("====(before)=padapter->xmitpriv.public_pgsz=0x%x====update_free_ffsz: free_pg=0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
		  pxmitpriv->public_pgsz,
		  pxmitpriv->free_pg[0],pxmitpriv->free_pg[1],pxmitpriv->free_pg[2],pxmitpriv->free_pg[3],
		  pxmitpriv->free_pg[4],pxmitpriv->free_pg[5],pxmitpriv->free_pg[6],pxmitpriv->free_pg[7]));

#if defined(CONFIG_IO_4B) && defined(CONFIG_IO_4B_ADDR)
	*(u32*)pxmitpriv->free_pg = read32(padapter, SDIO_BCNQ_FREEPG);
	*((u32*)pxmitpriv->free_pg + 1) = read32(padapter, SDIO_BCNQ_FREEPG + 4);
#else
	read_mem(padapter, SDIO_BCNQ_FREEPG, 8, pxmitpriv->free_pg);
#endif
	pxmitpriv->public_pgsz = pxmitpriv->free_pg[0];
	if (pxmitpriv->public_pgsz > pxmitpriv->init_pgsz) {
		pxmitpriv->init_pgsz = pxmitpriv->public_pgsz;
	}
	{
		u8 diff;
		if (pxmitpriv->public_pgsz > (pxmitpriv->init_pgsz - pxmitpriv->used_pgsz)) {
			RT_TRACE(_module_rtl8712_xmit_c_,_drv_notice_,
				 ("====(0)=====update_free_ffsz: pxmitpriv->public_pgsz=0x%x pxmitpriv->init_pgsz=0x%x pxmitpriv->used_pgsz=0x%x\n",
				  pxmitpriv->public_pgsz, pxmitpriv->init_pgsz, pxmitpriv->used_pgsz));
			diff = pxmitpriv->public_pgsz - (pxmitpriv->init_pgsz - pxmitpriv->used_pgsz);
			pxmitpriv->used_pgsz = pxmitpriv->used_pgsz - diff;
		//	pxmitpriv->required_pgsz = pxmitpriv->required_pgsz - diff;
			RT_TRACE(_module_hci_ops_c_,_drv_notice_,
				 ("====(1)=====update_free_ffsz: pxmitpriv->public_pgsz=0x%x diff=0x%x pxmitpriv->used_pgsz=0x%x pxmitpriv->required_pgsz=0x%x\n",
				  pxmitpriv->public_pgsz, diff, pxmitpriv->used_pgsz, pxmitpriv->required_pgsz));
		} else {

		}
	}

	RT_TRACE(_module_rtl8712_xmit_c_,_drv_notice_,
		 ("====(after)=====update_free_ffsz: free_pg=0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
		  pxmitpriv->free_pg[0],pxmitpriv->free_pg[1],pxmitpriv->free_pg[2],pxmitpriv->free_pg[3],
		  pxmitpriv->free_pg[4],pxmitpriv->free_pg[5],pxmitpriv->free_pg[6],pxmitpriv->free_pg[7]));

	return;
}

u8 check_fifosz(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	u8 res = _SUCCESS;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	struct xmit_priv *pxmitpriv = &(padapter->xmitpriv);
	u8 public_used=0,dedicated_sz=0;
	u32 pkt_len;
	u8 idx=0,blk_num,pagenum_required;
	u8 ac_idx;

	pkt_len=pxmitframe->attrib.pktlen+pxmitframe->attrib.hdrlen+TXDESC_SIZE;
	blk_num=(u8)((pkt_len>>9)+((pkt_len%512 ==0)? 0:1));
	RT_TRACE(_module_rtl8712_xmit_c_,_drv_debug_,("pkt_len = %d; blk_num = %d pkt_len mod 512 = %d\n",  pkt_len, blk_num,pkt_len%512));

	pxmitframe->attrib.qsel = (uint)pxmitframe->attrib.priority;

	switch(pxmitframe->attrib.priority){
		case 0: //BE
		case 3:
			ac_idx=BEQ_FREEPG_INX;
			break;
		case 1: //BK
		case 2:
			ac_idx=BKQ_FREEPG_INX;
			break;
		case 4: //VI
		case 5:
			ac_idx=VIQ_FREEPG_INX;
			break;
		case 6: //VO
		case 7:
			ac_idx=VOQ_FREEPG_INX;
			break;
	}
	pagenum_required=blk_num*2;

	pxmitframe->pg_num=pagenum_required;
	dedicated_sz=pxmitpriv->free_pg[ac_idx]-pxmitpriv->free_pg[BCNQ_FREEPG_INX];
	if(pxmitpriv->free_pg[ac_idx] >(pxmitframe->pg_num+5)){
		if(dedicated_sz >=pxmitframe->pg_num){
			public_used=0;
		}
		else{
			public_used=pxmitframe->pg_num-dedicated_sz;
		}

		for(idx=0;idx<8;idx++){
			if(idx==ac_idx){
				pxmitpriv->free_pg[idx]-=pxmitframe->pg_num;
			}
			else{
				pxmitpriv->free_pg[idx]-=public_used;
			}
		}
	}
	else{
		res=_FAIL;
	}

	return res;

}
void enqueue_xmitbuf(struct xmit_priv *pxmitpriv,struct xmit_buf *pxmitbuf){
	_irqL irqL;

	_enter_critical(&pxmitpriv->pending_xmitbuf_queue.lock, &irqL);
	list_delete(&pxmitbuf->list);
	list_insert_tail(&(pxmitbuf->list), get_list_head(&pxmitpriv->pending_xmitbuf_queue));
	_exit_critical(&pxmitpriv->pending_xmitbuf_queue.lock, &irqL);
	RT_TRACE(_module_rtl8712_xmit_c_,_drv_notice_,("\n Enqueue xmitbuf ok!!!\n"));
	return;
}

struct xmit_buf *dequeue_xmitbuf(struct xmit_priv *pxmitpriv)
{
	_irqL irqL;
	struct xmit_buf *pxmitbuf=  NULL;
	_list *plist, *phead;
	_queue *pending_xmitbuf_queue = &pxmitpriv->pending_xmitbuf_queue;

_func_enter_;

	RT_TRACE(_module_rtl8712_xmit_c_,_drv_notice_,("+Dequeue_xmitbuf\n"));

	_enter_critical(&pending_xmitbuf_queue->lock, &irqL);

	if (_queue_empty(pending_xmitbuf_queue) == _TRUE) {
		pxmitbuf = NULL;
	} else {
		phead = get_list_head(pending_xmitbuf_queue);

		plist = get_next(phead);

		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);

		list_delete(&pxmitbuf->list);
	}

	_exit_critical(&pending_xmitbuf_queue->lock, &irqL);

_func_exit_;

	return pxmitbuf;
}

u32 xmit_xmitframes(_adapter *padapter, struct xmit_priv *pxmitpriv)
{
	u32 tx_action = 0;
	struct hw_xmit *hwxmits = pxmitpriv->hwxmits;
	u8 no_res = _FALSE, idx, hwentry = pxmitpriv->hwxmit_entry;
	_irqL irqL0, irqL1;
	struct tx_servq *ptxservq = NULL;
	_list *sta_plist, *sta_phead, *xmitframe_plist, *xmitframe_phead;
	struct xmit_frame *pxmitframe = NULL;
	_queue *pframe_queue = NULL;
	struct xmit_buf *pxmitbuf = NULL;


	for (idx = 0; idx < hwentry; idx++, hwxmits++)
	{
		if (NULL == pxmitbuf) {
			RT_TRACE(_module_rtl8712_xmit_c_,_drv_notice_,("======xmit_xmitframe: idx=%d alloc xmitbuf\n",idx));
			pxmitbuf = alloc_xmitbuf(pxmitpriv);
			if (pxmitbuf == NULL) {
				RT_TRACE(_module_rtl8712_xmit_c_,_drv_err_,("xmit_buf is not enough!!!!!\n"));
				break;
			}
		} else {
			RT_TRACE(_module_rtl8712_xmit_c_,_drv_notice_,("======xmit_xmitframe: idx=%d use the same xmitbuf\n",idx));
		}

		_enter_critical(&hwxmits->sta_queue->lock, &irqL0);

		sta_phead = get_list_head(hwxmits->sta_queue);
		sta_plist = get_next(sta_phead);

		while (end_of_queue_search(sta_phead, sta_plist) == _FALSE)
		{
			ptxservq = LIST_CONTAINOR(sta_plist, struct tx_servq , tx_pending);

			pframe_queue = &ptxservq->sta_pending;

			_enter_critical(&pframe_queue->lock, &irqL1);

			xmitframe_phead = get_list_head(pframe_queue);
			xmitframe_plist = get_next(xmitframe_phead);

			while (end_of_queue_search(xmitframe_phead, xmitframe_plist) == _FALSE)
			{
				pxmitframe = LIST_CONTAINOR(xmitframe_plist, struct xmit_frame, list);
				if (NULL == pxmitframe) {
					//queue empty
					break;
				}
#if 1
				if (((pxmitframe->attrib.pktlen + pxmitframe->attrib.hdrlen + TXDESC_SIZE) + pxmitbuf->len) > MAX_XMITBUF_SZ)
				{
					enqueue_xmitbuf(pxmitpriv, pxmitbuf);
					pxmitbuf = NULL;
					pxmitbuf = alloc_xmitbuf(pxmitpriv);
					if (pxmitbuf == NULL) {
						RT_TRACE(_module_rtl8712_xmit_c_, _drv_emerg_, ("\n\n=========xmit_buf is not enough!!!!!\n"));
						break;
					}
				}
#endif
				//check hw resource
				// if ok
				if (_SUCCESS == check_fifosz(padapter, pxmitframe))
				{
					//check the frag case
					if (((pxmitframe->attrib.pktlen + pxmitframe->attrib.hdrlen + TXDESC_SIZE) + pxmitbuf->len) < MAX_XMITBUF_SZ)
					{
						list_delete(&pxmitframe->list);

						//coalesce the xmitframe to xmitbuf
						pxmitframe->pxmitbuf = pxmitbuf;
						pxmitframe->buf_addr = pxmitbuf->ptail;

						pxmitbuf->ff_hwaddr = get_ff_hwaddr(pxmitframe);

						xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);
#ifdef CONFIG_80211N_HT
						if ((pxmitframe->attrib.ether_type != 0x888e) && (pxmitframe->attrib.dhcp_pkt != 1))
							issue_addbareq_cmd(padapter, pxmitframe->attrib.priority);
#endif
						RT_TRACE(_module_rtl8712_xmit_c_, _drv_notice_, ("\n=before free_xmitframe=\n"));
						// need to check the xmitbuf size
						free_xmitframe(pxmitpriv, pxmitframe);
						RT_TRACE(_module_rtl8712_xmit_c_, _drv_notice_, ("\n=after free_xmitframe=\n"));
						pxmitframe = NULL;
					}
					else {
						//xmitbuf is not enough
						RT_TRACE(_module_rtl8712_xmit_c_,_drv_emerg_,("\n\n\n=000========xmit_buf is not enough!!!!!"));
					}
				}
				// NOK
				else {
					//HW resource is not enough
					RT_TRACE(_module_rtl8712_xmit_c_, _drv_err_, ("\n\n\n========HW resource is not enough idx=%d!!!!!", idx));
					tx_action = -1;
					goto out1;
				}
				xmitframe_plist = get_next(xmitframe_phead);
			}
out1:
			_exit_critical(&pframe_queue->lock, &irqL1);
			sta_plist = get_next(sta_plist);

			if (_queue_empty(pframe_queue)) //must be done after get_next and before break
				list_delete(&ptxservq->tx_pending);
		}

		_exit_critical(&hwxmits->sta_queue->lock, &irqL0);
		//dump xmit_buf to hw fifo
		if (pxmitbuf->len > 0) {
			RT_TRACE(_module_rtl8712_xmit_c_, _drv_notice_, ("\n ====pxmitbuf->len=%d enqueue\n",pxmitbuf->len));

			//enqueue
			enqueue_xmitbuf(pxmitpriv, pxmitbuf);
			pxmitbuf = NULL;
		} else {
			RT_TRACE(_module_rtl8712_xmit_c_, _drv_notice_, ("\n ==[else]==pxmitbuf->len=%d\n",pxmitbuf->len));
		}
	}

	if (pxmitbuf) {//add to prevent kernel panic if bk queue have packet to xmit
		if (pxmitbuf->len == 0) {
			free_xmitbuf(pxmitpriv, pxmitbuf);
			RT_TRACE(_module_rtl8712_xmit_c_, _drv_notice_, ("\n ==[free xmitbuf]==pxmitbuf->len=%d\n", pxmitbuf->len));
		}
	}

	while (_queue_empty(&pxmitpriv->pending_xmitbuf_queue) == _FALSE) {
		pxmitbuf = dequeue_xmitbuf(pxmitpriv);
		if (NULL != pxmitbuf) {
			write_port(padapter, pxmitbuf->ff_hwaddr, pxmitbuf->len, (u8*)pxmitbuf);
			pxmitbuf->len = 0;
			free_xmitbuf(pxmitpriv,pxmitbuf);
		}
	}

	return tx_action;
}

thread_return xmit_thread(thread_context context)
{
	u8 hwentry;
	struct hw_xmit *hwxmits;
	_adapter *padapter = (_adapter *)context;
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;


	alloc_hwxmits(padapter);

	hwxmits = pxmitpriv->hwxmits;
	hwentry = pxmitpriv->hwxmit_entry;

	init_hwxmits(hwxmits, hwentry);

#ifdef PLATFORM_LINUX
	daemonize("%s", padapter->pnetdev->name);
	allow_signal(SIGTERM);
#endif

	while(1)
	{
		RT_TRACE(_module_rtl8712_xmit_c_, _drv_info_, ("Before down xmit_sema\n"));
		if (_down_sema(&pxmitpriv->xmit_sema) == _FAIL) {
			//Error Case!!!
			RT_TRACE(_module_rtl8712_xmit_c_, _drv_emerg_, ("down xmit_sema fail\n"));

			if (txframes_pending(padapter) == _TRUE)
				RT_TRACE(_module_rtl8712_xmit_c_, _drv_emerg_, ("xmit_priv still has data but we can't down xmit_sema"));

			break;
		}
		RT_TRACE(_module_rtl8712_xmit_c_, _drv_notice_, ("down xmit_sema success\n"));

_next:
		if ((padapter->bDriverStopped == _TRUE) || (padapter->bSurpriseRemoved == _TRUE)) {
			//DbgPrint("xmit_thread:bDriverStopped(%d) OR bSurpriseRemoved(%d)", padapter->bDriverStopped, padapter->bSurpriseRemoved);
			RT_TRACE(_module_rtl8712_xmit_c_,_drv_err_,("xmit thread => bDriverStopped or bSurpriseRemoved \n"));
			break;
		}

		if (txframes_pending(padapter) == _FALSE) {
			continue;
		}

		if (register_tx_alive(padapter) != _SUCCESS) {
			continue;
		}

		update_free_ffsz(padapter);

		// dequeue frame
		xmit_xmitframes(padapter, pxmitpriv);

		if ((txframes_pending(padapter)) == _TRUE) {
			udelay_os(1);
			goto _next;//no hw resource to send
		} else {
			unregister_tx_alive(padapter);
		}

#ifdef PLATFORM_LINUX
		if (signal_pending(current)) {
			flush_signals(current);
		}
#endif
	}

	//free_hwxmits(padapter);

	_up_sema(&pxmitpriv->terminate_xmitthread_sema);

	thread_exit();
}
#endif

int xmit_direct(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	//struct xmit_priv *pxmitpriv = &padapter->xmitpriv;
	//_irqL irqL;
	int res = _SUCCESS;


	res = xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);

	pxmitframe->pkt = NULL;

	//_enter_critical(&pxmitpriv->lock, &irqL);
	if (res == _SUCCESS) {
		dump_xframe(padapter, pxmitframe);
	}
	//_exit_critical(&pxmitpriv->lock, &irqL);

	return res;
}

int xmit_enqueue(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	if (xmit_classifier(padapter, pxmitframe) == _FAIL)
	{
		RT_TRACE(_module_rtl8712_xmit_c_,_drv_err_,("drop xmit pkt for classifier fail\n"));		
		pxmitframe->pkt = NULL;
		return _FAIL;
	}

	return _SUCCESS;
}

