/******************************************************************************
 *
 * Copyright(c) 2013 Realtek Corporation. All rights reserved.
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
#define _RTL8812AS_XMIT_C_

#include <rtl8812a_hal.h>


#define TX_PAGE_SIZE		PAGE_SIZE_TX_8821A

static void __fill_txdesc_sectype(struct pkt_attrib *pattrib, PTXDESC_8821A ptxdesc)
{
	if ((pattrib->encrypt > 0) && !pattrib->bswenc)
	{
		switch (pattrib->encrypt)
		{
			// SEC_TYPE
			case _WEP40_:
			case _WEP104_:
			case _TKIP_:
			case _TKIP_WTMIC_:
				ptxdesc->sectype = 1;
				break;
#ifdef CONFIG_WAPI_SUPPORT
			case _SMS4_:
				ptxdesc->sectype = 2;
				break;
#endif
			case _AES_:
				ptxdesc->sectype = 3;
				break;

			case _NO_PRIVACY_:
			default:
					break;
		}
	}
}

static void __fill_txdesc_vcs(PADAPTER padapter, struct pkt_attrib *pattrib, PTXDESC_8821A ptxdesc)
{
	if (pattrib->vcs_mode)
	{
		switch(pattrib->vcs_mode)
		{
			case RTS_CTS:
				ptxdesc->rtsen = 1;
				break;

			case CTS_TO_SELF:
				ptxdesc->cts2self = 1;
				break;

			case NONE_VCS:
			default:
				break;
		}

		if (padapter->mlmeextpriv.mlmext_info.preamble_mode == PREAMBLE_SHORT)
			ptxdesc->rts_short = 1;

		ptxdesc->rtsrate = 0x8; // RTS Rate=24M
		ptxdesc->rts_ratefb_lmt = 0xF;

		// Enable HW RTS
//		ptxdesc->hw_rts_en = 1;
	}
}

static void __fill_txdesc_phy(PADAPTER padapter, struct pkt_attrib *pattrib, PTXDESC_8821A ptxdesc)
{
	if (pattrib->ht_en) {
		// Set Bandwidth and sub-channel settings.
		ptxdesc->data_bw = BWMapping_8812(padapter, pattrib);

		//ptxdesc->data_sc = SCMapping_8812(padapter ,pattrib);
	}
}

static void _fillDefaultTxdesc(struct xmit_frame *pxmitframe, PTXDESC_8821A ptxdesc)
{
	PADAPTER padapter;
	PHAL_DATA_TYPE pHalData;
	struct mlme_ext_priv *pmlmeext;
	struct mlme_ext_info *pmlmeinfo; 
	struct pkt_attrib *pattrib;
	s32 bmcst;


	padapter = pxmitframe->padapter;
	pHalData = GET_HAL_DATA(padapter);
	pmlmeext = &padapter->mlmeextpriv;
	pmlmeinfo = &(pmlmeext->mlmext_info);

	pattrib = &pxmitframe->attrib;
	bmcst = IS_MCAST(pattrib->ra);

	if (pxmitframe->frame_tag == DATA_FRAMETAG)
	{
		ptxdesc->macid = pattrib->mac_id; // CAM_ID(MAC_ID)
		ptxdesc->rate_id = pattrib->raid;
		ptxdesc->qsel = pattrib->qsel;
		ptxdesc->seq = pattrib->seqnum;

		__fill_txdesc_sectype(pattrib, ptxdesc);
		__fill_txdesc_vcs(padapter, pattrib, ptxdesc);

		if ((pattrib->ether_type != 0x888e)
			&& (pattrib->ether_type != 0x0806)
			&& (pattrib->dhcp_pkt != 1)
#ifdef CONFIG_AUTO_AP_MODE
		    && (pattrib->pctrl != _TRUE)
#endif
			)
		{
			// Non EAP & ARP & DHCP type data packet

			if (pattrib->ampdu_en == _TRUE)
			{
				ptxdesc->agg_en = 1;
				ptxdesc->max_agg_num = 0x1f;
				ptxdesc->ampdu_density = pattrib->ampdu_spacing;
			}
			else
			{
				ptxdesc->bk = 1;
			}

			__fill_txdesc_phy(padapter, pattrib, ptxdesc);

			ptxdesc->data_ratefb_lmt = 0x1F;

			if (pHalData->fw_ractrl == _FALSE) {
				ptxdesc->userate = 1;

				if (pHalData->INIDATA_RATE[pattrib->mac_id] & BIT(7))
					ptxdesc->data_short = 1;

				ptxdesc->datarate = pHalData->INIDATA_RATE[pattrib->mac_id] & 0x7F;
			}

			if (padapter->fix_rate != 0xFF) {
				ptxdesc->userate = 1;
				ptxdesc->data_short = (padapter->fix_rate & BIT(7))?1:0;
				ptxdesc->datarate = padapter->fix_rate & 0x7F;
				if (!padapter->data_fb)
					ptxdesc->disdatafb = 1;
			}

			if (pattrib->ldpc)
				ptxdesc->data_ldpc = 1;
			if (pattrib->stbc)
				ptxdesc->data_stbc = 1;
		}
		else
		{
			// EAP data packet and ARP and DHCP packet.
			// Use the 1M or 6M data rate to send the EAP/ARP packet.
			// This will maybe make the handshake smooth.

			ptxdesc->userate = 1;
			ptxdesc->bk = 1;

			if (pmlmeinfo->preamble_mode == PREAMBLE_SHORT)
				ptxdesc->data_short = 1;

			ptxdesc->datarate = MRateToHwRate(pmlmeext->tx_rate);
		}

		ptxdesc->usb_txagg_num = pxmitframe->agg_num;

#ifdef CONFIG_TDLS
#ifdef CONFIG_XMIT_ACK
		/* CCX-TXRPT ack for xmit mgmt frames. */
		if (pxmitframe->ack_report) {
			#ifdef DBG_CCX
			DBG_8192C("%s set spe_rpt\n", __func__);
			#endif
			ptxdesc->spe_rpt = 1;
		}
#endif /* CONFIG_XMIT_ACK */
#endif
	}
	else if (pxmitframe->frame_tag == MGNT_FRAMETAG)
	{
//		DBG_8192C("%s: MGNT_FRAMETAG\n", __FUNCTION__);

		ptxdesc->macid = pattrib->mac_id; // CAM_ID(MAC_ID)
		ptxdesc->rate_id = pattrib->raid; // Rate ID
		ptxdesc->qsel = pattrib->qsel;
		ptxdesc->seq = pattrib->seqnum;

		ptxdesc->mbssid = pattrib->mbssid & 0xF;

		ptxdesc->rty_lmt_en = 1; // retry limit enable
		if (pattrib->retry_ctrl == _TRUE) {
			ptxdesc->data_rt_lmt = 6;
		} else {
			ptxdesc->data_rt_lmt = 12;
		}

		ptxdesc->userate = 1; // driver uses rate, 1M

#ifdef CONFIG_INTEL_PROXIM
		if ((padapter->proximity.proxim_on == _TRUE)
			&& (pattrib->intel_proxim == _TRUE))
		{
			DBG_8192C("%s: pattrib->rate=%d\n", __FUNCTION__, pattrib->rate);
			ptxdesc->datarate = pattrib->rate;
		}
		else
#endif // CONFIG_INTEL_PROXIM
		{
			ptxdesc->datarate = MRateToHwRate(pmlmeext->tx_rate);
		}

#ifdef CONFIG_XMIT_ACK
		// CCX-TXRPT ack for xmit mgmt frames.
		if (pxmitframe->ack_report) {
			#ifdef DBG_CCX
			DBG_8192C("%s set spe_rpt\n", __FUNCTION__);
			#endif
			ptxdesc->spe_rpt = 1;
		}
#endif // CONFIG_XMIT_ACK
	}
	else if (pxmitframe->frame_tag == TXAGG_FRAMETAG)
	{
		DBG_8192C("%s: TXAGG_FRAMETAG\n", __FUNCTION__);
	}
#ifdef CONFIG_MP_INCLUDED
	else if ((pxmitframe->frame_tag == MP_FRAMETAG)
			&& (padapter->registrypriv.mp_mode == 1))
	{
		struct tx_desc *pdesc;

		pdesc = (struct tx_desc*)ptxdesc;
//		DBG_8192C("%s: MP_FRAMETAG\n", __FUNCTION__);
		fill_txdesc_for_mp(padapter, (u8 *)pdesc);

		pdesc->txdw0 = le32_to_cpu(pdesc->txdw0);
		pdesc->txdw1 = le32_to_cpu(pdesc->txdw1);
		pdesc->txdw2 = le32_to_cpu(pdesc->txdw2);
		pdesc->txdw3 = le32_to_cpu(pdesc->txdw3);
		pdesc->txdw4 = le32_to_cpu(pdesc->txdw4);
		pdesc->txdw5 = le32_to_cpu(pdesc->txdw5);
		pdesc->txdw6 = le32_to_cpu(pdesc->txdw6);
		pdesc->txdw7 = le32_to_cpu(pdesc->txdw7);
		pdesc->txdw8 = le32_to_cpu(pdesc->txdw8);
		pdesc->txdw9 = le32_to_cpu(pdesc->txdw9);
	}
#endif // CONFIG_MP_INCLUDED
	else
	{
		DBG_8192C("%s: frame_tag=0x%x\n", __FUNCTION__, pxmitframe->frame_tag);

		ptxdesc->macid = pattrib->mac_id; // CAM_ID(MAC_ID)
		ptxdesc->rate_id = pattrib->raid; // Rate ID
		ptxdesc->qsel = pattrib->qsel;
		ptxdesc->seq = pattrib->seqnum;
		ptxdesc->userate = 1; // driver uses rate
		ptxdesc->datarate = MRateToHwRate(pmlmeext->tx_rate);
	}

	ptxdesc->pktlen = pattrib->last_txcmdsz;

	ptxdesc->offset = TXDESC_SIZE;
	ptxdesc->pkt_offset = 0;
#ifdef CONFIG_TX_EARLY_MODE
	if (pxmitframe->frame_tag == DATA_FRAMETAG)
	{
		ptxdesc->offset += EARLY_MODE_INFO_SIZE;
		ptxdesc->pkt_offset = 0x01;
	}
#endif // CONFIG_TX_EARLY_MODE

	if (bmcst) ptxdesc->bmc = 1;

	// 2009.11.05. tynli_test. Suggested by SD4 Filen for FW LPS.
	// (1) The sequence number of each non-Qos frame / broadcast / multicast /
	// mgnt frame should be controled by Hw because Fw will also send null data
	// which we cannot control when Fw LPS enable.
	// --> default enable non-Qos data sequense number. 2010.06.23. by tynli.
	// (2) Enable HW SEQ control for beacon packet, because we use Hw beacon.
	// (3) Use HW Qos SEQ to control the seq num of Ext port non-Qos packets.
	// 2010.06.23. Added by tynli.
	if (!pattrib->qos_en)
	{
		// Hw set sequence number
		ptxdesc->en_hwseq = 1;
	}
}

/*
 *	Description:
 *
 *	Parameters:
 *		pxmitframe	xmitframe
 *		pdescbuf		where to fill tx desc
 */
void _updateTxdesc(struct xmit_frame *pxmitframe, u8 *pdescbuf)
{
	struct tx_desc *pdesc;


	pdesc = (struct tx_desc*)pdescbuf;
	_rtw_memset(pdesc, 0, sizeof(struct tx_desc));

	_fillDefaultTxdesc(pxmitframe, (PTXDESC_8821A)pdescbuf);

	pdesc->txdw0 = cpu_to_le32(pdesc->txdw0);
	pdesc->txdw1 = cpu_to_le32(pdesc->txdw1);
	pdesc->txdw2 = cpu_to_le32(pdesc->txdw2);
	pdesc->txdw3 = cpu_to_le32(pdesc->txdw3);
	pdesc->txdw4 = cpu_to_le32(pdesc->txdw4);
	pdesc->txdw5 = cpu_to_le32(pdesc->txdw5);
	pdesc->txdw6 = cpu_to_le32(pdesc->txdw6);
	pdesc->txdw7 = cpu_to_le32(pdesc->txdw7);
	pdesc->txdw8 = cpu_to_le32(pdesc->txdw8);
	pdesc->txdw9 = cpu_to_le32(pdesc->txdw9);

	rtl8812a_cal_txdesc_chksum(pdescbuf);

	_dbg_dump_tx_info(pxmitframe->padapter, pxmitframe->frame_tag, pdescbuf);
}

// translate fifo addr to queue index
static u8 _DeviceID2QueueIdx(u8 deviceID)
{
	u8 queueIdx = PUBLIC_QUEUE_IDX;


	switch (deviceID)
	{
		case WLAN_TX_HIQ_DEVICE_ID:
			queueIdx = HI_QUEUE_IDX;
			break;

		case WLAN_TX_MIQ_DEVICE_ID:
			queueIdx = MID_QUEUE_IDX;
			break;

		case WLAN_TX_LOQ_DEVICE_ID:
			queueIdx = LOW_QUEUE_IDX;
			break;

		default:
			queueIdx = HI_QUEUE_IDX;
			DBG_8192C("%s: [ERROR] undefined deviceID(%d), return HIGH queue Idx(%d)\n",
				__FUNCTION__, deviceID, queueIdx);
			break;
	}

	return queueIdx;
}

static u8 rtw_sdio_wait_enough_TxOQT_space(PADAPTER padapter, u8 agg_num)
{
	u32 n = 0;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);

	while (pHalData->SdioTxOQTFreeSpace < agg_num) 
	{
		if ((padapter->bSurpriseRemoved == _TRUE) 
			|| (padapter->bDriverStopped == _TRUE)
#ifdef CONFIG_CONCURRENT_MODE
			||((padapter->pbuddy_adapter) 
		&& ((padapter->pbuddy_adapter->bSurpriseRemoved) ||(padapter->pbuddy_adapter->bDriverStopped)))
#endif		
		){
			DBG_871X("%s: bSurpriseRemoved or bDriverStopped (wait TxOQT)\n", __func__);
			return _FALSE;
		}

		HalQueryTxOQTBufferStatus8821ASdio(padapter);
		
		if ((++n % 60) == 0) {
			if ((n % 300) == 0) {			
				DBG_871X("%s(%d): QOT free space(%d), agg_num: %d\n",
 				__func__, n, pHalData->SdioTxOQTFreeSpace, agg_num);
			}	
			rtw_msleep_os(1);
			//yield();
		}
	}

	pHalData->SdioTxOQTFreeSpace -= agg_num;
	
	//if (n > 1)
	//	++priv->pshare->nr_out_of_txoqt_space;

	return _TRUE;
}

/*
 * Dequeue xmit buffer and send to hardware
 *
 * Return:
 *	_TRUE	queue is empty, nothing to be send to HW
 *	_FALSE	queue is NOT empty
 */
static s32 _dequeue_writeport(PADAPTER padapter)
{
	struct mlme_priv *pmlmepriv;
	struct xmit_priv *pxmitpriv;
	struct dvobj_priv *pdvobjpriv;
	struct xmit_buf *pxmitbuf;
	PADAPTER pri_padapter;
	s32 ret = _FALSE;
	u8	PageIdx = 0;
	u32	deviceId;
#ifdef CONFIG_SDIO_TX_ENABLE_AVAL_INT
	u8	bUpdatePageNum = _FALSE;
#else
	u32	polling_num = 0;
#endif


	pmlmepriv = &padapter->mlmepriv;
	pxmitpriv = &padapter->xmitpriv;
	pdvobjpriv = adapter_to_dvobj(padapter);
	pri_padapter = padapter;

#ifdef CONFIG_CONCURRENT_MODE
	if (padapter->adapter_type > 0)
		pri_padapter = padapter->pbuddy_adapter;

	if (rtw_buddy_adapter_up(padapter))
		ret = check_buddy_fwstate(padapter, _FW_UNDER_SURVEY);
#endif

	ret = ret || check_fwstate(pmlmepriv, _FW_UNDER_SURVEY);

	if (_TRUE == ret)
		pxmitbuf = dequeue_pending_xmitbuf_under_survey(pxmitpriv);
	else
		pxmitbuf = dequeue_pending_xmitbuf(pxmitpriv);

	if (pxmitbuf == NULL)
		return _TRUE;

	deviceId = ffaddr2deviceId(pdvobjpriv, pxmitbuf->ff_hwaddr);

	// translate fifo addr to queue index
	switch (deviceId) {
		case WLAN_TX_HIQ_DEVICE_ID:
				PageIdx = HI_QUEUE_IDX;
				break;

		case WLAN_TX_MIQ_DEVICE_ID:
				PageIdx = MID_QUEUE_IDX;
				break;

		case WLAN_TX_LOQ_DEVICE_ID:
				PageIdx = LOW_QUEUE_IDX;
				break;
	}

query_free_page:
	// check if hardware tx fifo page is enough
	if( _FALSE == rtw_hal_sdio_query_tx_freepage(pri_padapter, PageIdx, pxmitbuf->pg_num))
	{
#ifdef CONFIG_SDIO_TX_ENABLE_AVAL_INT
		if (!bUpdatePageNum) {
			// Total number of page is NOT available, so update current FIFO status
			HalQueryTxBufferStatus8821AS(padapter);
			bUpdatePageNum = _TRUE;
			goto query_free_page;
		} else {
			bUpdatePageNum = _FALSE;
			enqueue_pending_xmitbuf_to_head(pxmitpriv, pxmitbuf);
			return _TRUE;
		}
#else //CONFIG_SDIO_TX_ENABLE_AVAL_INT
		polling_num++;
		if ((polling_num % 0x7F) == 0) {//or 80
			//DBG_871X("%s: FIFO starvation!(%d) len=%d agg=%d page=(R)%d(A)%d\n",
			//	__func__, n, pxmitbuf->len, pxmitbuf->agg_num, pframe->pg_num, freePage[PageIdx] + freePage[PUBLIC_QUEUE_IDX]);
			rtw_msleep_os(1);
		}

		// Total number of page is NOT available, so update current FIFO status
		HalQueryTxBufferStatus8821AS(padapter);
		goto query_free_page;
#endif //CONFIG_SDIO_TX_ENABLE_AVAL_INT
	}

	if ((padapter->bSurpriseRemoved == _TRUE)
		|| (padapter->bDriverStopped == _TRUE)
#ifdef CONFIG_CONCURRENT_MODE
		|| ((padapter->pbuddy_adapter)
			&& ((padapter->pbuddy_adapter->bSurpriseRemoved)
				|| (padapter->pbuddy_adapter->bDriverStopped)))
#endif
		)
	{
		DBG_8192C("%s: Removed=%d Stopped=%d, goto free xmitbuf!\n",
			__FUNCTION__, padapter->bSurpriseRemoved, padapter->bDriverStopped);
#ifdef CONFIG_CONCURRENT_MODE
		if (padapter->pbuddy_adapter)
		{
			PADAPTER pbuddy = padapter->pbuddy_adapter;
			DBG_8192C("%s: buddy_adapter Removed=%d Stopped=%d, goto free xmitbuf!\n",
				__FUNCTION__, pbuddy->bSurpriseRemoved, pbuddy->bDriverStopped);
		}
		else
		{
			DBG_8192C("%s: buddy_adapter not exist!!\n", __FUNCTION__);
		}
#endif

		goto free_xmitbuf;
	}


	if (rtw_sdio_wait_enough_TxOQT_space(padapter, pxmitbuf->agg_num) == _FALSE) 
	{
		goto free_xmitbuf;
	}

	rtw_write_port(padapter, deviceId, pxmitbuf->len, (u8 *)pxmitbuf);

	rtw_hal_sdio_update_tx_freepage(pri_padapter, PageIdx, pxmitbuf->pg_num);

free_xmitbuf:
	//rtw_free_xmitframe(pxmitpriv, pframe);
	//pxmitbuf->priv_data = NULL;
	rtw_free_xmitbuf(pxmitpriv, pxmitbuf);

#ifdef CONFIG_SDIO_TX_TASKLET
	tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
#endif

	return _FAIL;
}

/*
 * Description
 *	Transmit xmitbuf to hardware tx fifo
 *
 * Return
 *	_SUCCESS	ok
 *	_FAIL		something error
 */
s32 XmitBufHandler8821AS(PADAPTER padapter)
{
	struct xmit_priv *pxmitpriv;
	u8	queue_empty, queue_pending;
	s32 ret;


	pxmitpriv = &padapter->xmitpriv;

	ret = _rtw_down_sema(&pxmitpriv->xmit_sema);
	if (ret == _FAIL) {
		DBG_8192C("%s: down SdioXmitBufSema fail!\n", __FUNCTION__);
		return _FAIL;
	}

	if ((padapter->bSurpriseRemoved == _TRUE) ||
		(padapter->bDriverStopped == _TRUE))
	{
		DBG_8192C("%s: bDriverStopped(%d) bSurpriseRemoved(%d)\n",
				  __FUNCTION__, padapter->bDriverStopped, padapter->bSurpriseRemoved);
		return _FAIL;
	}

	queue_pending = check_pending_xmitbuf(pxmitpriv);

#ifdef CONFIG_CONCURRENT_MODE
	if(rtw_buddy_adapter_up(padapter))
		queue_pending |= check_pending_xmitbuf(&padapter->pbuddy_adapter->xmitpriv);
#endif

	if(queue_pending == _FALSE)
		return _SUCCESS;

#ifdef CONFIG_LPS_LCLK
	ret = rtw_register_tx_alive(padapter);
	if (ret != _SUCCESS) return _SUCCESS;
#endif

	do {
		queue_empty = _dequeue_writeport(padapter);

#ifdef CONFIG_CONCURRENT_MODE
		//	dump secondary adapter xmitbuf
		if (rtw_buddy_adapter_up(padapter))
			queue_empty &= _dequeue_writeport(padapter->pbuddy_adapter);
#endif

	} while (!queue_empty);

#ifdef CONFIG_LPS_LCLK
	rtw_unregister_tx_alive(padapter);
#endif
	return _SUCCESS;
}

#ifdef CONFIG_SDIO_TX_TASKLET
static s32 _sendXmitFrames(PADAPTER padapter, struct xmit_priv *pxmitpriv)
{
	s32 ret;
	_irqL irqL;
	struct xmit_buf *pxmitbuf;
	struct hw_xmit *phwxmit = pxmitpriv->hwxmits;
	struct tx_servq *ptxservq = NULL;
	_list *xmitframe_plist = NULL, *xmitframe_phead = NULL;
	struct xmit_frame *pxmitframe = NULL, *pfirstframe = NULL;
	u32	pbuf = 0; // next pkt address
	u32	pbuf_tail = 0; // last pkt tail
	u32	txlen = 0; //packet length, except TXDESC_SIZE and PKT_OFFSET
	u32	total_len = 0;
	u8	ac_index = 0;
	u8	bfirst = _TRUE;//first aggregation xmitframe
	u8	bulkstart = _FALSE;
#ifdef CONFIG_TX_EARLY_MODE
	u8	pkt_index=0;
#endif


	pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
	if (pxmitbuf == NULL) {
		RT_TRACE(_module_hal_xmit_c_, _drv_err_, ("%s: xmit_buf is not enough!\n", __FUNCTION__));
#ifdef CONFIG_SDIO_TX_ENABLE_AVAL_INT
	#ifdef CONFIG_CONCURRENT_MODE
		if (padapter->adapter_type > PRIMARY_ADAPTER)
			_rtw_up_sema(&(padapter->pbuddy_adapter->xmitpriv.xmit_sema));
		else
	#endif
			_rtw_up_sema(&(pxmitpriv->xmit_sema));
#endif
		return _FALSE;
	}

	do {
		//3 1. pick up first frame
		if(bfirst)
		{
			pxmitframe = rtw_dequeue_xframe(pxmitpriv, pxmitpriv->hwxmits, pxmitpriv->hwxmit_entry);
			if (pxmitframe == NULL) {
				// no more xmit frame, release xmit buffer
				rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
				return _FALSE;
			}

			pxmitframe->pxmitbuf = pxmitbuf;
			pxmitframe->buf_addr = pxmitbuf->pbuf;
			pxmitbuf->priv_data = pxmitframe;
			pxmitbuf->ff_hwaddr = rtw_get_ff_hwaddr(pxmitframe);

			pfirstframe = pxmitframe;

			_enter_critical_bh(&pxmitpriv->lock, &irqL);
			ptxservq = rtw_get_sta_pending(padapter, pfirstframe->attrib.psta, pfirstframe->attrib.priority, (u8 *)(&ac_index));
			_exit_critical_bh(&pxmitpriv->lock, &irqL);
		}
		//3 2. aggregate same priority and same DA(AP or STA) frames
		else
		{
			// dequeue same priority packet from station tx queue
			_enter_critical_bh(&pxmitpriv->lock, &irqL);

			if (_rtw_queue_empty(&ptxservq->sta_pending) == _FALSE)
			{
				xmitframe_phead = get_list_head(&ptxservq->sta_pending);
				xmitframe_plist = get_next(xmitframe_phead);

				pxmitframe = LIST_CONTAINOR(xmitframe_plist, struct xmit_frame, list);

				// check xmit_buf size enough or not
				txlen = TXDESC_SIZE + rtw_wlan_pkt_size(pxmitframe);
				#ifdef CONFIG_TX_EARLY_MODE
				txlen += EARLY_MODE_INFO_SIZE;
				#endif
				if(_FAIL == rtw_hal_busagg_qsel_check(padapter,pfirstframe->attrib.qsel,pxmitframe->attrib.qsel)){
					bulkstart = _TRUE;
				}
				else{
					if (pbuf + _RND8(txlen) > MAX_XMITBUF_SZ)
					{
						bulkstart = _TRUE;
					}
					else
					{
						rtw_list_delete(&pxmitframe->list);
						ptxservq->qcnt--;
						phwxmit[ac_index].accnt--;

						//Remove sta node when there is no pending packets.
						if (_rtw_queue_empty(&ptxservq->sta_pending) == _TRUE)
							rtw_list_delete(&ptxservq->tx_pending);
					}
				}
			}
			else
			{
				bulkstart = _TRUE;
			}

			_exit_critical_bh(&pxmitpriv->lock, &irqL);

			if(bulkstart)
			{
				break;
			}

			pxmitframe->buf_addr = pxmitbuf->pbuf + pbuf;

			pxmitframe->agg_num = 0; // not first frame of aggregation
		}

		ret = rtw_xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);
		if (ret == _FAIL) {
			DBG_871X("%s: coalesce FAIL!", __FUNCTION__);
			rtw_free_xmitframe(pxmitpriv, pxmitframe);
			continue;
		}

		// always return ndis_packet after rtw_xmitframe_coalesce
		//rtw_os_xmit_complete(padapter, pxmitframe);

#ifdef CONFIG_TX_EARLY_MODE
		pxmitpriv->agg_pkt[pkt_index].pkt_len = pxmitframe->attrib.last_txcmdsz;	//get from rtw_xmitframe_coalesce
		pxmitpriv->agg_pkt[pkt_index].offset = _RND8(pxmitframe->attrib.last_txcmdsz+ TXDESC_SIZE+EARLY_MODE_INFO_SIZE);
		pkt_index++;
#endif

		if (bfirst)
		{
			txlen = TXDESC_SIZE + pxmitframe->attrib.last_txcmdsz;
			#ifdef CONFIG_TX_EARLY_MODE
			txlen += EARLY_MODE_INFO_SIZE;
			#endif

			pxmitframe->pg_num = PageNum(txlen, TX_PAGE_SIZE);
			pxmitbuf->pg_num = PageNum(txlen, TX_PAGE_SIZE);

			total_len = txlen;

			pbuf_tail = txlen;
			pbuf = _RND8(pbuf_tail);

			bfirst = _FALSE;
		}
		else
		{
			_updateTxdesc(pxmitframe, pxmitframe->buf_addr);

			// don't need xmitframe any more
			rtw_free_xmitframe(pxmitpriv, pxmitframe);

			pxmitframe->pg_num = PageNum(txlen, TX_PAGE_SIZE);
			pxmitbuf->pg_num += PageNum(txlen, TX_PAGE_SIZE);

			total_len += txlen;

			// handle pointer and stop condition
			pbuf_tail = pbuf + txlen;
			pbuf = _RND8(pbuf_tail);

			pfirstframe->agg_num++;
			if (pfirstframe->agg_num >= (rtw_hal_sdio_max_txoqt_free_space(padapter)-1))
				break;

		}
	}while(1);

	//3 3. update first frame txdesc
	_updateTxdesc(pfirstframe, pfirstframe->buf_addr);
#ifdef CONFIG_TX_EARLY_MODE
	UpdateEarlyModeInfo8812(pxmitpriv,pxmitbuf );
#endif

	//
	pxmitbuf->agg_num = pfirstframe->agg_num;
	pxmitbuf->priv_data = NULL;

	//3 4. write xmit buffer to USB FIFO
	pxmitbuf->len = pbuf_tail;
	enqueue_pending_xmitbuf(pxmitpriv, pxmitbuf);

	//3 5. update statisitc
	rtw_count_tx_stats(padapter, pfirstframe, total_len);

	rtw_free_xmitframe(pxmitpriv, pfirstframe);

	//rtw_yield_os();

	return _TRUE;
}

void XmitTasklet8821AS(void *priv)
{
	int ret = _FALSE;
	PADAPTER padapter = (PADAPTER)priv;
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;

	while(1)
	{
		if (RTW_CANNOT_TX(padapter))
		{
			DBG_8192C("%s: bDriverStopped(%d) bSurpriseRemoved(%d) bWritePortCancel\n",
				__FUNCTION__, padapter->bDriverStopped, padapter->bSurpriseRemoved);
			break;
		}

		ret = _sendXmitFrames(padapter, pxmitpriv);
		if (ret == _FALSE)
			break;
	}
}
#else // !CONFIG_SDIO_TX_TASKLET
/*
 * Description:
 *	Aggregation packets and enqueue, ready to write to HW TX FIFO
 *
 * Return:
 *	0	Success
 *	-2	Software resource(xmitbuf) not ready
 */
static s32 _sendXmitFrames(PADAPTER padapter, struct xmit_priv *pxmitpriv)
{
	u32 err, agg_num=0;
	u8 pkt_index=0;
	struct hw_xmit *hwxmits, *phwxmit;
	u8 idx, hwentry;
	_irqL irql;
	struct tx_servq	*ptxservq;
	_list *sta_plist, *sta_phead, *frame_plist, *frame_phead;
	struct xmit_frame *pxmitframe;
	_queue *pframe_queue;
	struct xmit_buf *pxmitbuf;
	u32 txlen;
	s32 ret;
	int inx[4];
	u8 pre_qsel=0xFF,next_qsel=0xFF;

	err = 0;
	hwxmits = pxmitpriv->hwxmits;
	hwentry = pxmitpriv->hwxmit_entry;
	ptxservq = NULL;
	pxmitframe = NULL;
	pframe_queue = NULL;
	pxmitbuf = NULL;

	if (padapter->registrypriv.wifi_spec == 1) {
		for (idx=0; idx<4; idx++)
			inx[idx] = pxmitpriv->wmm_para_seq[idx];
	} else {
		inx[0] = 0; inx[1] = 1; inx[2] = 2; inx[3] = 3;
	}

	// 0(VO), 1(VI), 2(BE), 3(BK)
	for (idx = 0; idx < hwentry; idx++)
	{
		phwxmit = hwxmits + inx[idx];

		if((check_pending_xmitbuf(pxmitpriv) == _TRUE) && (padapter->mlmepriv.LinkDetectInfo.bHigherBusyTxTraffic == _TRUE)) {
			if ((phwxmit->accnt > 0) && (phwxmit->accnt < 5)) {
				err = -2;
				break;
			}
		}

		_enter_critical_bh(&pxmitpriv->lock, &irql);

		sta_phead = get_list_head(phwxmit->sta_queue);
		sta_plist = get_next(sta_phead);

		while (rtw_end_of_queue_search(sta_phead, sta_plist) == _FALSE)
		{
			ptxservq = LIST_CONTAINOR(sta_plist, struct tx_servq, tx_pending);

			sta_plist = get_next(sta_plist);

			pframe_queue = &ptxservq->sta_pending;

			frame_phead = get_list_head(pframe_queue);
			frame_plist = get_next(frame_phead);

			while (rtw_end_of_queue_search(frame_phead, frame_plist) == _FALSE)
			{
				pxmitframe = LIST_CONTAINOR(frame_plist, struct xmit_frame, list);

				// check xmit_buf size enough or not
				txlen = TXDESC_SIZE + rtw_wlan_pkt_size(pxmitframe);
#ifdef CONFIG_TX_EARLY_MODE
				txlen += EARLY_MODE_INFO_SIZE;
#endif // CONFIG_TX_EARLY_MODE
				next_qsel = pxmitframe->attrib.qsel;
				if ((NULL == pxmitbuf)
					|| ((pxmitbuf->ptail + txlen) > pxmitbuf->pend)
					|| (agg_num >= (rtw_hal_sdio_max_txoqt_free_space(padapter)-1))
					|| ((agg_num!=0) && (_FAIL == rtw_hal_busagg_qsel_check(padapter,pre_qsel,next_qsel)))
					)
				{
					if (pxmitbuf)
					{
						struct xmit_frame *pframe;
						pframe = (struct xmit_frame*)pxmitbuf->priv_data;

						pxmitbuf->agg_num = agg_num;
//						DBG_8192C("%s: agg_num=%d\n", __FUNCTION__, agg_num);

						pframe->agg_num = agg_num;
						_updateTxdesc(pframe, pframe->buf_addr);
#ifdef CONFIG_TX_EARLY_MODE
						UpdateEarlyModeInfo8812(pxmitpriv, pxmitbuf);
#endif // CONFIG_TX_EARLY_MODE

						rtw_free_xmitframe(pxmitpriv, pframe);
						pxmitbuf->priv_data = NULL;

//						DBG_8192C("%s: enqueue xmitbuf, len=%d agg=%d pg=%d\n",
//							__FUNCTION__, pxmitbuf->len, pxmitbuf->agg_num, pxmitbuf->pg_num);
						enqueue_pending_xmitbuf(pxmitpriv, pxmitbuf);
//						rtw_yield_os();
					}

					pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
					if (pxmitbuf == NULL) {
						//DBG_8192C("%s: xmit_buf is not enough!\n", __FUNCTION__);
						err = -2;
#ifdef CONFIG_SDIO_TX_ENABLE_AVAL_INT
	#ifdef CONFIG_CONCURRENT_MODE
						if (padapter->adapter_type > PRIMARY_ADAPTER)
							_rtw_up_sema(&(padapter->pbuddy_adapter->xmitpriv.xmit_sema));
						else
	#endif
							_rtw_up_sema(&(pxmitpriv->xmit_sema));
#endif
						break;
					}
					agg_num = 0;
					pkt_index = 0;
				}

				// Resource is enough to send this frame
				// remove frame from queue
				frame_plist = get_next(frame_plist); // pointer move to next
				rtw_list_delete(&pxmitframe->list); // remove frame from queue
				ptxservq->qcnt--;
				phwxmit->accnt--;
#ifdef CONFIG_DEBUG // Lucas debug 2013/05/06. This can be removed anytime.
				if (ptxservq->qcnt < 0)
				{
					DBG_8192C("%s:[ERROR] ptxservq->qcnt=%d\n", __FUNCTION__, ptxservq->qcnt);
				}
				if (phwxmit->accnt < 0)
				{
					DBG_8192C("%s:[ERROR] phwxmit->accnt=%d\n", __FUNCTION__, phwxmit->accnt);
				}
#endif // CONFIG_DEBUG

				if (agg_num == 0) {
					pxmitbuf->ff_hwaddr = rtw_get_ff_hwaddr(pxmitframe);
					pxmitbuf->priv_data = (u8*)pxmitframe;
				}

				// coalesce the xmitframe to xmitbuf
				pxmitframe->pxmitbuf = pxmitbuf;
				pxmitframe->buf_addr = pxmitbuf->ptail;

				ret = rtw_xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);
				if (ret == _FAIL)
				{
					DBG_8192C("%s: coalesce FAIL!\n", __FUNCTION__);
				}
				else
				{
					agg_num++;
					if (agg_num != 1)
						_updateTxdesc(pxmitframe, pxmitframe->buf_addr);
					
					rtw_count_tx_stats(padapter, pxmitframe, pxmitframe->attrib.last_txcmdsz);
					pre_qsel = pxmitframe->attrib.qsel;
					txlen = TXDESC_SIZE + pxmitframe->attrib.last_txcmdsz;
#ifdef CONFIG_TX_EARLY_MODE
					txlen += EARLY_MODE_INFO_SIZE;
#endif // CONFIG_TX_EARLY_MODE
					pxmitframe->pg_num = PageNum(txlen, TX_PAGE_SIZE);
					pxmitbuf->pg_num += PageNum(txlen, TX_PAGE_SIZE);

#ifdef CONFIG_TX_EARLY_MODE
					pxmitpriv->agg_pkt[pkt_index].pkt_len = pxmitframe->attrib.last_txcmdsz;	//get from rtw_xmitframe_coalesce
					pxmitpriv->agg_pkt[pkt_index].offset = _RND8(pxmitframe->attrib.last_txcmdsz + TXDESC_SIZE + EARLY_MODE_INFO_SIZE);
#endif // CONFIG_TX_EARLY_MODE

					pkt_index++;
					pxmitbuf->ptail += _RND(txlen, 8); // round to 8 bytes alignment
					pxmitbuf->len = _RND(pxmitbuf->len, 8) + txlen;
				}

				if (agg_num != 1)
					rtw_free_xmitframe(pxmitpriv, pxmitframe);
				pxmitframe = NULL;
			}

			if (_rtw_queue_empty(pframe_queue) == _TRUE) {
				rtw_list_delete(&ptxservq->tx_pending);
			}
		}

		_exit_critical_bh(&pxmitpriv->lock, &irql);

		// dump xmit_buf to hw tx fifo
		if (pxmitbuf)
		{
			if (pxmitbuf->len > 0)
			{
				struct xmit_frame *pframe;
				pframe = (struct xmit_frame*)pxmitbuf->priv_data;

				pxmitbuf->agg_num = agg_num;

				pframe->agg_num = agg_num;
				_updateTxdesc(pframe, pframe->buf_addr);
#ifdef CONFIG_TX_EARLY_MODE
				UpdateEarlyModeInfo8812(pxmitpriv, pxmitbuf);
#endif
				rtw_free_xmitframe(pxmitpriv, pframe);
				pxmitbuf->priv_data = NULL;

//				DBG_8192C("%s: enqueue xmitbuf, len=%d agg=%d pg=%d\n",
//					__FUNCTION__, pxmitbuf->len, pxmitbuf->agg_num, pxmitbuf->pg_num);
				enqueue_pending_xmitbuf(pxmitpriv, pxmitbuf);
//				rtw_yield_os();
			}
			else
				rtw_free_xmitbuf(pxmitpriv, pxmitbuf);

			pxmitbuf = NULL;
		}
	}

	return err;
}

/*
 * Description
 *	Transmit xmitframe from queue
 *
 * Return
 *	_SUCCESS	ok
 *	_FAIL		something error
 */
static s32 _xmitHandler(PADAPTER padapter)
{
	struct xmit_priv *pxmitpriv;
	s32 ret;
	_irqL irql;


	pxmitpriv = &padapter->xmitpriv;

wait:
	ret = _rtw_down_sema(&pxmitpriv->SdioXmitSema);
	if (_FAIL == ret) {
		DBG_8192C("%s: down sema fail!\n", __FUNCTION__);
		return _FAIL;
	}

next:
	if ((padapter->bSurpriseRemoved == _TRUE) ||
		(padapter->bDriverStopped == _TRUE))
	{
		DBG_8192C("%s: bDriverStopped(%d) bSurpriseRemoved(%d)\n",
				  __FUNCTION__, padapter->bDriverStopped, padapter->bSurpriseRemoved);
		return _FAIL;
	}

	_enter_critical_bh(&pxmitpriv->lock, &irql);
	ret = rtw_txframes_pending(padapter);
	_exit_critical_bh(&pxmitpriv->lock, &irql);
	if (ret == 0) {
		return _SUCCESS;
	}

	// dequeue frame and write to hardware
	ret = _sendXmitFrames(padapter, pxmitpriv);
	if (ret == -2) {
		rtw_yield_os();
		goto next;
	}

	_enter_critical_bh(&pxmitpriv->lock, &irql);
	ret = rtw_txframes_pending(padapter);
	_exit_critical_bh(&pxmitpriv->lock, &irql);
	if (ret == 1) {
		goto next;
	}

	return _SUCCESS;
}

thread_return XmitThread8821AS(thread_context context)
{
	s32 ret;
	PADAPTER padapter;
	struct xmit_priv *pxmitpriv;


	ret = _SUCCESS;
	padapter = (PADAPTER)context;
	pxmitpriv = &padapter->xmitpriv;

	thread_enter("RTW_XMIT_SDIO_THREAD");

	DBG_8192C("+%s\n", __FUNCTION__);

	do {
		ret = _xmitHandler(padapter);
		if (signal_pending(current))
		{
			flush_signals(current);
		}
	} while (_SUCCESS == ret);

	_rtw_up_sema(&pxmitpriv->SdioXmitTerminateSema);

	DBG_8192C("-%s\n", __FUNCTION__);

	thread_exit();
}
#endif // !CONFIG_SDIO_TX_TASKLET

s32 MgntXmit8821AS(PADAPTER padapter, struct xmit_frame *pmgntframe)
{
	s32 ret = _SUCCESS;
	struct pkt_attrib	*pattrib;
	struct xmit_buf	*pxmitbuf;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	u8	*pframe = (u8 *)(pmgntframe->buf_addr) + TXDESC_OFFSET;
	u8 	 pattrib_subtype;


//	DBG_8192C("+%s\n", __FUNCTION__);

	pattrib = &pmgntframe->attrib;
	pxmitbuf = pmgntframe->pxmitbuf;

	_updateTxdesc(pmgntframe, pmgntframe->buf_addr);

	pxmitbuf->len = TXDESC_SIZE + pattrib->last_txcmdsz;
	pxmitbuf->pg_num = PageNum(pxmitbuf->len, TX_PAGE_SIZE);
	pxmitbuf->ptail = pmgntframe->buf_addr + pxmitbuf->len;
	pxmitbuf->ff_hwaddr = rtw_get_ff_hwaddr(pmgntframe);

	rtw_count_tx_stats(padapter, pmgntframe, pattrib->last_txcmdsz);
	pattrib_subtype = pattrib->subtype;
	rtw_free_xmitframe(pxmitpriv, pmgntframe);

	pxmitbuf->priv_data = NULL;

	if ((pattrib_subtype == WIFI_BEACON)
		|| (GetFrameSubType(pframe) == WIFI_BEACON))
	{
		// dump beacon directly
		u32 addr;

		addr = ffaddr2deviceId(pdvobjpriv, pxmitbuf->ff_hwaddr);
		ret = rtw_write_port(padapter, addr, pxmitbuf->len, (u8*)pxmitbuf);
		if (ret != _SUCCESS)
			rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_WRITE_PORT_ERR);

		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
	}
	else
	{
		enqueue_pending_xmitbuf(pxmitpriv, pxmitbuf);
	}

	return ret;
}

/*
 * Description:
 *	Handle xmitframe(packet) come from rtw_xmit()
 *	Doesn't acquire xmitpriv->lock here!
 *
 * Return:
 *	_TRUE	dump packet directly ok
 *	_FALSE	enqueue, temporary can't transmit packets to hardware
 */
s32	HalXmitNoLock8821AS(PADAPTER padapter, struct xmit_frame *pxmitframe)
{
	struct xmit_priv *pxmitpriv;
	s32 err;


	pxmitpriv = &padapter->xmitpriv;

	err = rtw_xmitframe_enqueue(padapter, pxmitframe);
	if (err != _SUCCESS)
	{
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		pxmitpriv->tx_drop++;
	}
	else
	{
#ifdef CONFIG_SDIO_TX_TASKLET
		tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
#else
		_rtw_up_sema(&pxmitpriv->SdioXmitSema);
#endif
	}

	return err;
}

/*
 * Description:
 *	Handle xmitframe(packet) come from rtw_xmit()
 *
 * Return:
 *	_TRUE	dump packet directly ok
 *	_FALSE	enqueue, temporary can't transmit packets to hardware
 */
s32 HalXmit8821AS(PADAPTER padapter, struct xmit_frame *pxmitframe)
{
	struct xmit_priv *pxmitpriv;
	_irqL irql;
	s32 err;


	pxmitpriv = &padapter->xmitpriv;

#ifdef CONFIG_80211N_HT
	if ((pxmitframe->frame_tag == DATA_FRAMETAG) &&
		(pxmitframe->attrib.ether_type != 0x0806) &&
		(pxmitframe->attrib.ether_type != 0x888e) &&
		(pxmitframe->attrib.dhcp_pkt != 1))
	{
		if (padapter->mlmepriv.LinkDetectInfo.bBusyTraffic == _TRUE)
			rtw_issue_addbareq_cmd(padapter, pxmitframe);
	}
#endif

	_enter_critical_bh(&pxmitpriv->lock, &irql);
	err = HalXmitNoLock8821AS(padapter, pxmitframe);
	_exit_critical_bh(&pxmitpriv->lock, &irql);

	return err;
}

/*
 * Return
 *	_SUCCESS	everything is fine
 *	_FAIL		something wrong
 *
 */
s32 InitXmitPriv8821AS(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;
	struct xmit_priv *pxmitpriv;


	pHalData = GET_HAL_DATA(padapter);
	pxmitpriv = &padapter->xmitpriv;

#ifdef CONFIG_SDIO_TX_TASKLET
#ifdef PLATFORM_LINUX
	tasklet_init(&pxmitpriv->xmit_tasklet,
	     (void(*)(unsigned long))XmitTasklet8821AS,
	     (unsigned long)padapter);
#endif
#else // !CONFIG_SDIO_TX_TASKLET
	_rtw_init_sema(&pxmitpriv->SdioXmitSema, 0);
	_rtw_init_sema(&pxmitpriv->SdioXmitTerminateSema, 0);
#endif // !CONFIG_SDIO_TX_TASKLET

	_rtw_spinlock_init(&pHalData->SdioTxFIFOFreePageLock);

#ifdef CONFIG_TX_EARLY_MODE
	pHalData->bEarlyModeEnable = padapter->registrypriv.early_mode;
#endif // CONFIG_TX_EARLY_MODE

	return _SUCCESS;
}

void FreeXmitPriv8821AS(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;


	pHalData = GET_HAL_DATA(padapter);

	_rtw_spinlock_free(&pHalData->SdioTxFIFOFreePageLock);
}

