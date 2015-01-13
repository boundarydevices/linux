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
#define _SDIO_HALINIT_C_

#include <rtl8812a_hal.h>

#ifndef CONFIG_SDIO_HCI
#error "CONFIG_SDIO_HCI shall be on!\n"
#endif

#define CONFIG_POWER_ON_CHECK
#ifdef CONFIG_POWER_ON_CHECK
/*
 * Description:
 *	Call this function to make sure power on successfully
 *
 * Return:
 *	_SUCCESS	enable success
 *	_FAIL	enable fail
 */
static s32 PowerOnCheck(PADAPTER padapter)
{
	u32	val_offset0, val_offset1, val_offset2, val_offset3;
	u32 val_mix = 0;
	u32 res = 0;
	u8	ret = _FAIL;
	s32 index = 0;


	val_offset0 = rtw_read8(padapter, REG_CR);
	val_offset1 = rtw_read8(padapter, REG_CR+1);
	val_offset2 = rtw_read8(padapter, REG_CR+2);
	val_offset3 = rtw_read8(padapter, REG_CR+3);

	if (val_offset0 == 0xEA || val_offset1 == 0xEA ||
			val_offset2 == 0xEA || val_offset3 ==0xEA)
	{
		DBG_871X("%s: power on fail, do Power on again\n", __FUNCTION__);
		return ret;
	}

	val_mix = val_offset0;
	val_mix |= (val_offset1 << 8);
	val_mix |= (val_offset2 << 16);
	val_mix |= (val_offset3 << 24);

	do {
		res = rtw_read32(padapter, REG_CR);

		DBG_871X("%s: REG_CR=(cmd52)0x%08x (cmd53)0x%08x, times=%d\n", __FUNCTION__, val_mix, res, index);

		if (res == val_mix)
		{
			DBG_871X("%s: 0x100 the result of cmd52 and cmd53 is the same.\n", __FUNCTION__);
			ret = _SUCCESS;
			break;
		}

		index++;
	} while (index < 100);

	if (_SUCCESS == ret)
	{
		ret = _FAIL;
		index = 0;
		while (index < 100)
		{
			rtw_write32(padapter, 0x1B8, 0x12345678);
			res = rtw_read32(padapter, 0x1B8);
			if (res == 0x12345678)
			{
				DBG_871X("%s: 0x1B8 test Pass.\n", __FUNCTION__);
				ret = _SUCCESS;
				break;
			}

			index++;
			DBG_871X("%s: 0x1B8 test Fail(index: %d).\n", __FUNCTION__, index);
		}
	}
	else
	{
		DBG_871X("%s: fail at cmd52, cmd53 check\n", __FUNCTION__);
	}

	if (ret == _FAIL) {
		DBG_871X_LEVEL(_drv_err_, "Dump MAC Page0 register:\n");
		/* Dump Page0 for check cystal*/
		for (index = 0 ; index < 0xff ; index++) {
			if(index%16==0)
				printk("0x%02x ",index);

			printk("%02x ", rtw_read8(padapter, index)); 

			if(index%16==15)
				printk("\n");
			else if(index%8==7)
				printk("\t");
		}
		printk("\n");
	}

	return ret;
}
#endif /* CONFIG_POWER_ON_CHECK */

/*
 * Description:
 *	Call power on sequence to enable card
 *
 * Return:
 *	_SUCCESS	enable success
 *	_FAIL		enable fail
 */
static u32 _CardEnable(PADAPTER padapter)
{
	u8 bMacPwrCtrlOn;
	u32 ret;


	ret = _SUCCESS;

	rtw_hal_get_hwreg(padapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if (bMacPwrCtrlOn == _FALSE)
	{
		u8 power_ok;

		// RSV_CTRL 0x1C[7:0] = 0x00
		// unlock ISO/CLK/Power control register
		rtw_write8(padapter, REG_RSV_CTRL, 0x0);

		power_ok = HalPwrSeqCmdParsing(padapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK, Rtl8821A_NIC_ENABLE_FLOW);
		if (_TRUE == power_ok)
		{
			bMacPwrCtrlOn = _TRUE;
			rtw_hal_set_hwreg(padapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
		}
		else
		{
			ret = _FAIL;
		}
	}

	return ret;
}

//
// Description:
//	RTL8723e card disable power sequence v003 which suggested by Scott.
//
// First created by tynli. 2011.01.28.
//
static void hal_poweroff_8812as(PADAPTER padapter)
{
	u8		u1bTmp;
	u16		u2bTmp;
	u32		u4bTmp;
	u8		bMacPwrCtrlOn = _FALSE;
	u8		ret;

	rtw_hal_get_hwreg(padapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if(bMacPwrCtrlOn == _FALSE)	
		return ;
	
	// Run LPS WL RFOFF flow
	ret = HalPwrSeqCmdParsing(padapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK, Rtl8821A_NIC_LPS_ENTER_FLOW);
	if (ret == _FALSE) {
		DBG_8192C(KERN_ERR "%s: run RF OFF flow fail!\n", __func__);
	}

	//	==== Reset digital sequence   ======

	u1bTmp = rtw_read8(padapter, REG_MCUFWDL);
	if ((u1bTmp & RAM_DL_SEL) && padapter->bFWReady) //8051 RAM code
		_8051Reset8812(padapter);

	// Reset MCU 0x2[10]=0. Suggested by Filen. 2011.01.26. by tynli.
	u1bTmp = rtw_read8(padapter, REG_SYS_FUNC_EN+1);
	u1bTmp &= ~BIT(2);	// 0x2[10], FEN_CPUEN
	rtw_write8(padapter, REG_SYS_FUNC_EN+1, u1bTmp);

	// MCUFWDL 0x80[1:0]=0
	// reset MCU ready status
	rtw_write8(padapter, REG_MCUFWDL, 0);

	//	==== Reset digital sequence end ======

	// Power down.
	bMacPwrCtrlOn = _FALSE;	// Disable CMD53 R/W
	rtw_hal_set_hwreg(padapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	ret = HalPwrSeqCmdParsing(padapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK, Rtl8821A_NIC_DISABLE_FLOW);
	if (ret == _FALSE) {
		DBG_8192C(KERN_ERR "%s: run CARD DISABLE flow fail!\n", __FUNCTION__);
	}

	// Reset MCU IO Wrapper, added by Roger, 2011.08.30
	u1bTmp = rtw_read8(padapter, REG_RSV_CTRL+1);
	u1bTmp &= ~BIT(0);
	rtw_write8(padapter, REG_RSV_CTRL+1, u1bTmp);
	u1bTmp = rtw_read8(padapter, REG_RSV_CTRL+1);
	u1bTmp |= BIT(0);
	rtw_write8(padapter, REG_RSV_CTRL+1, u1bTmp);

	// RSV_CTRL 0x1C[7:0]=0x0E
	// lock ISO/CLK/Power control register
	rtw_write8(padapter, REG_RSV_CTRL, 0x0E);
	
	bMacPwrCtrlOn = _FALSE;
	rtw_hal_set_hwreg(padapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	
}

/*
 * Description:
 *	Run power on flow to enable IC
 *
 * Return:
 *	_SUCCESS	Power on success
 *	_FAIL		Power on fail
 */
static u32 _InitPowerOn_8812AS(PADAPTER padapter)
{
	u8 value8;
	u16 value16;
	u32 value32;
	u32 ret;


	ret = _CardEnable(padapter);
	if (ret == _FAIL) {
		RT_TRACE(_module_hci_hal_init_c_, _drv_emerg_,
				("%s: run power on flow fail\n", __FUNCTION__));
		return _FAIL;
	}

	// Radio-Off Pin Trigger
	value8 = rtw_read8(padapter, REG_GPIO_INTM+1);
	value8 |= BIT(1); // Enable falling edge triggering interrupt
	rtw_write8(padapter, REG_GPIO_INTM+1, value8);
	value8 = rtw_read8(padapter, REG_GPIO_IO_SEL_2+1);
	value8 |= BIT(1);
	rtw_write8(padapter, REG_GPIO_IO_SEL_2+1, value8);

	// Enable power down and GPIO interrupt
	value16 = rtw_read16(padapter, REG_APS_FSMCO);
	value16 |= EnPDN; // Enable HW power down and RF on
	rtw_write16(padapter, REG_APS_FSMCO, value16);

	/* disable */
	rtw_write8(padapter, REG_CR, 0x00);

	// Enable MAC DMA/WMAC/SCHEDULE/SEC block
	value16 = rtw_read16(padapter, REG_CR);
	value16 |= (HCI_TXDMA_EN | HCI_RXDMA_EN | TXDMA_EN | RXDMA_EN
				| PROTOCOL_EN | SCHEDULE_EN | ENSEC | CALTMR_EN);
	rtw_write16(padapter, REG_CR, value16);

	return _SUCCESS;
}

//Tx Page FIFO threshold
static void _init_available_page_threshold(PADAPTER padapter, u8 numHQ, u8 numNQ, u8 numLQ, u8 numPubQ)
{
	u16	HQ_threshold, NQ_threshold, LQ_threshold;

	HQ_threshold = (numPubQ + numHQ + 1) >> 1;
	HQ_threshold |= (HQ_threshold<<8);

	NQ_threshold = (numPubQ + numNQ + 1) >> 1;
	NQ_threshold |= (NQ_threshold<<8);

	LQ_threshold = (numPubQ + numLQ + 1) >> 1;
	LQ_threshold |= (LQ_threshold<<8);

	rtw_write16(padapter, 0x218, HQ_threshold);
	rtw_write16(padapter, 0x21A, NQ_threshold);
	rtw_write16(padapter, 0x21C, LQ_threshold);
	DBG_8192C("%s(): Enable Tx FIFO Page Threshold H:0x%x,N:0x%x,L:0x%x\n", __FUNCTION__, HQ_threshold, NQ_threshold, LQ_threshold);
}

static void _InitQueueReservedPage(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;
	u32 outEPNum;
	u32 numHQ, numLQ, numNQ, numPubQ;
	u8 bWiFiConfig;
	u8 value8;
	u32 value32;


	pHalData = GET_HAL_DATA(padapter);
	outEPNum = (u32)pHalData->OutEpNumber;
	numHQ = numLQ = numNQ = numPubQ = 0;
	bWiFiConfig	= padapter->registrypriv.wifi_spec;

	if (pHalData->OutEpQueueSel & TX_SELE_HQ)
		numHQ = bWiFiConfig ? WMM_NORMAL_PAGE_NUM_HPQ_8821 : NORMAL_PAGE_NUM_HPQ_8821;
	if (pHalData->OutEpQueueSel & TX_SELE_LQ)
		numLQ = bWiFiConfig ? WMM_NORMAL_PAGE_NUM_LPQ_8821 : NORMAL_PAGE_NUM_LPQ_8821;
	if (pHalData->OutEpQueueSel & TX_SELE_NQ)
		numNQ = bWiFiConfig ? WMM_NORMAL_PAGE_NUM_NPQ_8821 : NORMAL_PAGE_NUM_NPQ_8821;

	numPubQ = TX_TOTAL_PAGE_NUMBER_8821 - numHQ - numLQ - numNQ;

	value8 = (u8)_NPQ(numNQ);
	rtw_write8(padapter, REG_RQPN_NPQ, value8);
	value32 = _HPQ(numHQ) | _LPQ(numLQ) | _PUBQ(numPubQ) | LD_RQPN;
	rtw_write32(padapter, REG_RQPN, value32);

#ifdef CONFIG_SDIO_TX_ENABLE_AVAL_INT
	_init_available_page_threshold(padapter, numHQ, numNQ, numLQ, numPubQ);
#endif
}

static void
_SetQueuePriority(
	PADAPTER	padapter,
	u16		beQ,
	u16		bkQ,
	u16		viQ,
	u16		voQ,
	u16		mgtQ,
	u16		hiQ
	)
{
	u16 value16;


	value16 = rtw_read16(padapter, REG_TRXDMA_CTRL) & 0x7;
	value16 |= _TXDMA_BEQ_MAP(beQ) | _TXDMA_BKQ_MAP(bkQ) |
				_TXDMA_VIQ_MAP(viQ) | _TXDMA_VOQ_MAP(voQ) |
				_TXDMA_MGQ_MAP(mgtQ) | _TXDMA_HIQ_MAP(hiQ);
	rtw_write16(padapter, REG_TRXDMA_CTRL, value16);

	DBG_8192C("%s: BE=%d BK=%d VI=%d VO=%d MGT=%d HI=%d\n",
		__FUNCTION__, beQ, bkQ, viQ, voQ, mgtQ, hiQ);
}

static u8 _DeviceID2HWQueue(u8 deviceID)
{
	u8 queueID = QUEUE_EXTRA;


	switch (deviceID)
	{
		case WLAN_TX_HIQ_DEVICE_ID:
			queueID = QUEUE_HIGH;
			break;

		case WLAN_TX_MIQ_DEVICE_ID:
			queueID = QUEUE_NORMAL;
			break;

		case WLAN_TX_LOQ_DEVICE_ID:
			queueID = QUEUE_LOW;
			break;

		default:
			queueID = QUEUE_HIGH;
			DBG_8192C("%s: [ERROR] undefined deviceID(%d), return HIGH queue(%d)\n", __FUNCTION__, deviceID, queueID);
			break;
	}

	return queueID;
}

static void _InitQueuePriority(PADAPTER padapter)
{
	struct dvobj_priv *pdvobjpriv;
	u8 beQ, bkQ, viQ, voQ, mgtQ, hiQ;


	pdvobjpriv = adapter_to_dvobj(padapter);

	beQ = _DeviceID2HWQueue(pdvobjpriv->Queue2Pipe[BE_QUEUE_INX]);
	bkQ = _DeviceID2HWQueue(pdvobjpriv->Queue2Pipe[BK_QUEUE_INX]);
	viQ = _DeviceID2HWQueue(pdvobjpriv->Queue2Pipe[VI_QUEUE_INX]);
	voQ = _DeviceID2HWQueue(pdvobjpriv->Queue2Pipe[VO_QUEUE_INX]);
	mgtQ = _DeviceID2HWQueue(pdvobjpriv->Queue2Pipe[MGT_QUEUE_INX]);
	hiQ = _DeviceID2HWQueue(pdvobjpriv->Queue2Pipe[HIGH_QUEUE_INX]);

	_SetQueuePriority(padapter, beQ, bkQ, viQ, voQ, mgtQ, hiQ);
}

static void _InitTxBufferBoundary(PADAPTER padapter)
{
	u8 txpktbuf_bndy;
#ifdef CONFIG_CONCURRENT_MODE
	u8 val8;
#endif // CONFIG_CONCURRENT_MODE


	rtw_hal_get_def_var(padapter, HAL_DEF_TX_PAGE_BOUNDARY, &txpktbuf_bndy);

	rtw_write8(padapter, REG_BCNQ_BDNY, txpktbuf_bndy);
	rtw_write8(padapter, REG_MGQ_BDNY, txpktbuf_bndy);
	rtw_write8(padapter, REG_WMAC_LBK_BF_HD, txpktbuf_bndy);
	rtw_write8(padapter, REG_TRXFF_BNDY, txpktbuf_bndy);
	rtw_write8(padapter, REG_TDECTRL+1, txpktbuf_bndy);

#ifdef CONFIG_CONCURRENT_MODE
	val8 = txpktbuf_bndy + 8;
	rtw_write8(padapter, REG_BCNQ1_BDNY, val8);
	rtw_write8(padapter, REG_DWBCN1_CTRL_8812+1, val8); // BCN1_HEAD

	val8 = rtw_read8(padapter, REG_DWBCN1_CTRL_8812+2);
	val8 |= BIT(1); // BIT1- BIT_SW_BCN_SEL_EN
	rtw_write8(padapter, REG_DWBCN1_CTRL_8812+2, val8);
#endif // CONFIG_CONCURRENT_MODE
}

static void _InitRxBufferBoundary(PADAPTER padapter)
{
	u16 rxff_bndy;


	rxff_bndy = MAX_RX_DMA_BUFFER_SIZE_8821-1;
	rtw_write16(padapter, REG_TRXFF_BNDY + 2, rxff_bndy);
}

static void _InitPageBoundary(PADAPTER padapter)
{
	_InitTxBufferBoundary(padapter);
	_InitRxBufferBoundary(padapter);
}

static u8 _PageSize2Index(u32 pagesize)
{
	u8 pbp_idx;


	switch (pagesize)
	{
		case 64:
			pbp_idx = PBP_64;
			break;

		case 128:
			pbp_idx = PBP_128;
			break;

		case 256:
			pbp_idx = PBP_256;
			break;

		case 512:
			pbp_idx = PBP_512;
			break;

		case 1024:
			pbp_idx = PBP_1024;
			break;

		default:
			pbp_idx = PBP_256;
			DBG_8192C("%s: [ERROR] page size=%d not support, set to default index(%d)\n",
				__FUNCTION__, pagesize, pbp_idx);
			DBG_8192C("%s: index 0/1/2/3/4 = Size 64/128/256/512/1024\n",
				__FUNCTION__);
			break;
	}

	return pbp_idx;
}

static void _InitTransferPageSize(PADAPTER padapter)
{
	u8 value8;
	u8 tx_pbp_idx, rx_pbp_idx;


	tx_pbp_idx = _PageSize2Index(PAGE_SIZE_TX_8821A);
	rx_pbp_idx = _PageSize2Index(PAGE_SIZE_RX_8821A);

	value8 = _PSRX(rx_pbp_idx) | _PSTX(tx_pbp_idx);
	rtw_write8(padapter, REG_PBP, value8);
}

void _InitDriverInfoSize(PADAPTER padapter, u8 drvInfoSize)
{
	rtw_write8(padapter, REG_RX_DRVINFO_SZ, drvInfoSize);
}

void _InitNetworkType(PADAPTER padapter)
{
	u32 netType;


	netType = rtw_read32(padapter, REG_CR);

	netType &= ~MASK_NETTYPE;
	netType |= _NETTYPE(NT_LINK_AP);

	rtw_write32(padapter, REG_CR, netType);
}

void _InitWMACSetting(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;
	u16 value16;


	pHalData = GET_HAL_DATA(padapter);

	// don't turn on AAP, it will allow all packets to driver
	pHalData->ReceiveConfig = 0;
	pHalData->ReceiveConfig |= RCR_APM | RCR_AM | RCR_AB;
	pHalData->ReceiveConfig |= RCR_CBSSID_DATA | RCR_CBSSID_BCN | RCR_AMF;
	pHalData->ReceiveConfig |= RCR_HTC_LOC_CTRL;
	pHalData->ReceiveConfig |= RCR_APP_PHYST_RXFF | RCR_APP_ICV | RCR_APP_MIC;
#ifdef CONFIG_MAC_LOOPBACK_DRIVER
	pHalData->ReceiveConfig |= RCR_ADD3 | RCR_APWRMGT | RCR_ACRC32 | RCR_ADF;
#endif

	if(IS_HARDWARE_TYPE_8812(padapter) || IS_HARDWARE_TYPE_8821(padapter))
		pHalData->ReceiveConfig |= FORCEACK;

	rtw_write32(padapter, REG_RCR, pHalData->ReceiveConfig);

	// Accept all multicast address
	rtw_write32(padapter, REG_MAR, 0xFFFFFFFF);
	rtw_write32(padapter, REG_MAR + 4, 0xFFFFFFFF);

	// Accept all data frames
	value16 = 0xFFFF;
	rtw_write16(padapter, REG_RXFLTMAP2, value16);

	// 2010.09.08 hpfan
	// Since ADF is removed from RCR, ps-poll will not be indicate to driver,
	// RxFilterMap should mask ps-poll to gurantee AP mode can rx ps-poll.
	value16 = BIT10;
#ifdef CONFIG_BEAMFORMING
	// NDPA packet subtype is 0x0101
	value16 |= BIT5;
#endif
	rtw_write16(padapter, REG_RXFLTMAP1, value16);

	// Accept all management frames
	value16 = 0xFFFF;
	rtw_write16(padapter, REG_RXFLTMAP0, value16);
}

void _InitAdaptiveCtrl(PADAPTER padapter)
{
	u16	value16;
	u32	value32;


	// Response Rate Set
	value32 = rtw_read32(padapter, REG_RRSR);
	value32 &= ~RATE_BITMAP_ALL;
	value32 |= RATE_RRSR_CCK_ONLY_1M;
	rtw_write32(padapter, REG_RRSR, value32);

	// CF-END Threshold
	//m_spIoBase->rtw_write8(REG_CFEND_TH, 0x1);

	// SIFS (used in NAV)
	value16 = _SPEC_SIFS_CCK(0x10) | _SPEC_SIFS_OFDM(0x10);
	rtw_write16(padapter, REG_SPEC_SIFS, value16);

	// Retry Limit
	value16 = _LRL(0x30) | _SRL(0x30);
	rtw_write16(padapter, REG_RL, value16);
}

void _InitEDCA(PADAPTER padapter)
{
	// Set Spec SIFS (used in NAV)
	rtw_write16(padapter, REG_SPEC_SIFS, 0x100a);
	rtw_write16(padapter, REG_MAC_SPEC_SIFS, 0x100a);

	// Set SIFS for CCK
	rtw_write16(padapter, REG_SIFS_CTX, 0x100a);

	// Set SIFS for OFDM
	rtw_write16(padapter, REG_SIFS_TRX, 0x100a);

	// TXOP
	rtw_write32(padapter, REG_EDCA_BE_PARAM, 0x005EA42B);
	rtw_write32(padapter, REG_EDCA_BK_PARAM, 0x0000A44F);
	rtw_write32(padapter, REG_EDCA_VI_PARAM, 0x005EA324);
	rtw_write32(padapter, REG_EDCA_VO_PARAM, 0x002FA226);
}

void _InitRateFallback(PADAPTER padapter)
{
	// Set Data Auto Rate Fallback Retry Count register.
	rtw_write32(padapter, REG_DARFRC, 0x00000000);
	rtw_write32(padapter, REG_DARFRC+4, 0x10080404);
	rtw_write32(padapter, REG_RARFRC, 0x04030201);
	rtw_write32(padapter, REG_RARFRC+4, 0x08070605);

}

void _InitRetryFunction(PADAPTER padapter)
{
	u8	value8;

	value8 = rtw_read8(padapter, REG_FWHW_TXQ_CTRL);
	value8 |= EN_AMPDU_RTY_NEW;
	rtw_write8(padapter, REG_FWHW_TXQ_CTRL, value8);

	// Set ACK timeout
	rtw_write8(padapter, REG_ACKTO, 0x40);
}

static void _InitRxAggr(PADAPTER padapter)
{
	u8 valueDMATimeout;
	u8 valueDMAPageCount;


	if (padapter->registrypriv.wifi_spec)
	{
		// 2010.04.27 hpfan
		// Adjust RxAggrTimeout to close to zero disable RxAggr, suggested by designer
		// Timeout value is calculated by 34 / (2^n)
		valueDMATimeout = 0x0f;
		valueDMAPageCount = 0x01;
	}
	else
	{
		// 20130530, Isaac@SD1 suggest 3 kinds of parameter
#if 0
		// TX/RX Balance
		valueDMATimeout = 0x06;
		valueDMAPageCount = 0x06;
#endif
#if 0
		// TX/RX Balance, but TCP ack may be late
		valueDMATimeout = 0x16;
		valueDMAPageCount = 0x06;
#endif
#if 0
		// RX Best
		valueDMATimeout = 0x16;
		valueDMAPageCount = 0x08;
#endif

		valueDMATimeout = 0x01;
		valueDMAPageCount = 0x0f;
	}

	rtw_write8(padapter, REG_RXDMA_AGG_PG_TH+1, valueDMATimeout); // unit: 32us
	rtw_write8(padapter, REG_RXDMA_AGG_PG_TH, valueDMAPageCount); // unit: 1KB
}

static void _RXAggrSwitch(PADAPTER padapter, u8 enable)
{
	PHAL_DATA_TYPE pHalData;
	u8 valueDMA;
	u8 valueRxAggCtrl;


	pHalData = GET_HAL_DATA(padapter);

	valueDMA = rtw_read8(padapter, REG_TRXDMA_CTRL);
	valueRxAggCtrl = rtw_read8(padapter, REG_RXDMA_PRO_8812);

	if (_TRUE == enable)
	{
		valueDMA |= RXDMA_AGG_EN;
		valueRxAggCtrl |= BIT(1);
	}
	else
	{
		valueDMA &= ~RXDMA_AGG_EN;
		valueRxAggCtrl &= ~BIT(1);
	}

	rtw_write8(padapter, REG_TRXDMA_CTRL, valueDMA);
	rtw_write8(padapter, REG_RXDMA_PRO_8812, valueRxAggCtrl);
}

static void _EnableRxAggr(PADAPTER padapter)
{
	_RXAggrSwitch(padapter, _TRUE);
}

void _InitTRxAggregationSetting(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;


	pHalData = GET_HAL_DATA(padapter);

	// Tx aggregation setting
//	sdio_AggSettingTxUpdate(padapter);

	// Rx aggregation setting
	_InitRxAggr(padapter);
	_EnableRxAggr(padapter);

	// 201/12/10 MH Add for USB agg mode dynamic switch.
	pHalData->UsbRxHighSpeedMode = _FALSE;
}

static void _InitBeaconParameters(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;


	pHalData = GET_HAL_DATA(padapter);

	rtw_write16(padapter, REG_BCN_CTRL, 0x1010);

	rtw_write16(padapter, REG_TBTT_PROHIBIT, 0x6404); // ms
	rtw_write8(padapter, REG_DRVERLYINT, DRIVER_EARLY_INT_TIME_8812); // 5ms
	rtw_write8(padapter, REG_BCNDMATIM, BCN_DMA_ATIME_INT_TIME_8812); // 2ms

	// Suggested by designer timchen. Change beacon AIFS to the largest number
	// beacause test chip does not contension before sending beacon. by tynli. 2009.11.03
	rtw_write16(padapter, REG_BCNTCFG, 0x660F);

	pHalData->RegBcnCtrlVal = rtw_read8(padapter, REG_BCN_CTRL);
	pHalData->RegTxPause = rtw_read8(padapter, REG_TXPAUSE);
	pHalData->RegFwHwTxQCtrl = rtw_read8(padapter, REG_FWHW_TXQ_CTRL+2);
	pHalData->RegReg542 = rtw_read8(padapter, REG_TBTT_PROHIBIT+2);
	pHalData->RegCR_1 = rtw_read8(padapter, REG_CR+1);
}

static void _InitBeaconMaxError(PADAPTER padapter, u8 InfraMode)
{
}

void _InitInterrupt(PADAPTER padapter)
{
	// HISR - turn all off
	rtw_write32(padapter, REG_HISR, 0);
	// HIMR - turn all off
	rtw_write32(padapter, REG_HIMR, 0);

	InitInterrupt8821AS(padapter);
}

#if (MP_DRIVER == 1)
static void _InitRxSetting(PADAPTER padapter)
{
	rtw_write32(padapter, REG_MACID, 0x87654321);
	rtw_write32(padapter, 0x0700, 0x87654321);
}
#endif // (MP_DRIVER == 1)

#if 0
// Set CCK and OFDM Block "ON"
static void _BBTurnOnBlock(PADAPTER padapter)
{
#if (DISABLE_BB_RF)
	return;
#endif

	PHY_SetBBReg(padapter, rFPGA0_RFMOD, bCCKEn, 0x1);
	PHY_SetBBReg(padapter, rFPGA0_RFMOD, bOFDMEn, 0x1);
}
#endif

static void _RfPowerSave(PADAPTER padapter)
{
}

static void _InitAntenna_Selection(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;
	u32 val32;


	pHalData = GET_HAL_DATA(padapter);
	if (pHalData->AntDivCfg == 0)
		return;

	DBG_8192C("+%s\n",__FUNCTION__);

	rtw_write8(padapter, REG_LEDCFG2, 0x82);

	PHY_SetBBReg(padapter, rFPGA0_XAB_RFParameter, BIT(13), 0x01);
	val32 = PHY_QueryBBReg(padapter, rFPGA0_XA_RFInterfaceOE, 0x300);
	if (val32 == MAIN_ANT)
		pHalData->CurAntenna = MAIN_ANT;
	else
		pHalData->CurAntenna = AUX_ANT;

	DBG_8192C("-%s: Cur_ant:(%x)%s\n", __FUNCTION__, pHalData->CurAntenna, (pHalData->CurAntenna==MAIN_ANT)?"MAIN_ANT":"AUX_ANT");
}

static void _InitBurstPktLen(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;
	u8 val8;


	pHalData = GET_HAL_DATA(padapter);

	val8 = rtw_read8(padapter, REG_HT_SINGLE_AMPDU_8812);
	val8 |= BIT(7); //enable single pkt ampdu
	rtw_write8(padapter, REG_HT_SINGLE_AMPDU_8812, val8);

	//for VHT packet length 11K
	rtw_write8(padapter, REG_RX_PKT_LIMIT, 0x18);
	rtw_write16(padapter, REG_MAX_AGGR_NUM, 0x1F1F);
	rtw_write8(padapter, REG_PIFS, 0x00);

	val8 = rtw_read8(padapter, REG_FWHW_TXQ_CTRL);
	val8 &= ~BIT(7);
	rtw_write8(padapter, REG_FWHW_TXQ_CTRL, val8);

	if (pHalData->AMPDUBurstMode)
	{
		rtw_write8(padapter, REG_AMPDU_BURST_MODE_8812,  0x5F);
	}
	rtw_write8(padapter, REG_AMPDU_MAX_TIME_8812, 0x5e);

	// ARFB table 9 for 11ac 5G 2SS
	rtw_write32(padapter, REG_ARFR0_8812, 0x00000010);
	rtw_write32(padapter, REG_ARFR0_8812+4, 0xfffff000);

	// ARFB table 10 for 11ac 5G 1SS
	rtw_write32(padapter, REG_ARFR1_8812, 0x00000010);
	if (IS_HARDWARE_TYPE_8821S(padapter))
		rtw_write32(padapter, REG_ARFR1_8812+4, 0x003ff000);
	else
		rtw_write32(padapter, REG_ARFR1_8812+4, 0x000ff000);

	// ARFB table 11 for 11ac 24G 1SS
	rtw_write32(padapter, REG_ARFR2_8812, 0x000ff015);
	rtw_write32(padapter, REG_ARFR2_8812+4, 0x000ff005);
	// ARFB table 12 for 11ac 24G 2SS
	rtw_write32(padapter, REG_ARFR3_8812, 0x0ffff015);
	rtw_write32(padapter, REG_ARFR3_8812+4, 0xc0000000);
}

static u32 _HalInit(PADAPTER padapter)
{
	s32 ret;
	u8 boundary;
	PHAL_DATA_TYPE pHalData;
	struct pwrctrl_priv *pwrctrlpriv;
	struct registry_priv *pregistrypriv;
	rt_rf_power_state eRfPowerStateToSet;
	u32 NavUpper = WiFiNavUpperUs;
	u8 val8, val8_CR;
	u16 val16;


	ret = _SUCCESS;
	pHalData = GET_HAL_DATA(padapter);
	pwrctrlpriv = adapter_to_pwrctl(padapter);
	pregistrypriv = &padapter->registrypriv;

	if (pwrctrlpriv->bkeepfwalive)
	{
//		_ps_open_RF(padapter);

		if (pHalData->odmpriv.RFCalibrateInfo.bIQKInitialized){
			//PHY_IQCalibrate_8812A(Adapter,_TRUE);
		}
		else
		{
			//PHY_IQCalibrate_8812A(Adapter,_FALSE);
			pHalData->odmpriv.RFCalibrateInfo.bIQKInitialized = _TRUE;
		}

		//ODM_TXPowerTrackingCheck(&pHalData->odmpriv );
		//PHY_LCCalibrate_8812A(Adapter);

		goto exit;
	}

	// Check if MAC has already power on. by tynli. 2011.05.27.
	val8 = rtw_read8(padapter, REG_SYS_CLKR+1);
	val8_CR = rtw_read8(padapter, REG_CR);
	DBG_8192C("%s: REG_SYS_CLKR 0x09=0x%02x REG_CR 0x100=0x%02x\n", __FUNCTION__, val8, val8_CR);
	if ((val8&BIT(3))  && ((val8_CR != 0) && (val8_CR != 0xEA)))
	{
		//pHalData->bMACFuncEnable = TRUE;
		DBG_8192C("%s: MAC has already power on\n", __FUNCTION__);
	}
	else
	{
		//pHalData->bMACFuncEnable = FALSE;
		// Set FwPSState to ALL_ON mode to prevent from the I/O be return because of 32k
		// state which is set before sleep under wowlan mode. 2012.01.04. by tynli.
		//pHalData->FwPSState = FW_PS_STATE_ALL_ON_88E;
		DBG_871X("%s: MAC has not been powered on yet\n", __FUNCTION__);
	}

	ret = _InitPowerOn_8812AS(padapter);
	if (_FAIL == ret) {
		RT_TRACE(_module_hci_hal_init_c_, _drv_err_, ("Failed to init Power On!\n"));
		return _FAIL;
	}

#ifdef CONFIG_POWER_ON_CHECK
	ret = PowerOnCheck(padapter);
	if (_FAIL == ret ) {
		DBG_871X("Power on Fail! do it again\n");
		ret = _InitPowerOn_8812AS(padapter);
		if (_FAIL == ret) {
			DBG_871X("Failed to init Power On!\n");
			return _FAIL;
		}
	}
	DBG_871X("Power on ok!\n");
#endif

	rtw_hal_get_def_var(padapter, HAL_DEF_TX_PAGE_BOUNDARY, (u8*)&boundary);
	ret = InitLLTTable8812A(padapter, boundary);
	if (_SUCCESS != ret) {
		RT_TRACE(_module_hci_hal_init_c_, _drv_err_, ("Failed to init LLT Table!\n"));
		return _FAIL;
	}

#if (MP_DRIVER == 1)
	if (padapter->registrypriv.mp_mode == 1)
	{
		_InitRxSetting(padapter);
	}
	else
#endif
	{
		ret = FirmwareDownload8812(padapter, _FALSE);
		if (ret != _SUCCESS) {
			DBG_8192C("%s: Download Firmware failed!!\n", __FUNCTION__);
			padapter->bFWReady = _FALSE;
			pHalData->fw_ractrl = _FALSE;
			return ret;
		} else {
			DBG_8192C("%s: Download Firmware Success\n", __FUNCTION__);
			padapter->bFWReady = _TRUE;
			pHalData->fw_ractrl = _TRUE;
		}
	}

	InitializeFirmwareVars8812(padapter);

	if (pwrctrlpriv->reg_rfoff == _TRUE) {
		pwrctrlpriv->rf_pwrstate = rf_off;
	}

	// Save target channel
	pHalData->CurrentChannel = 6;

#if (HAL_MAC_ENABLE == 1)
	ret = PHY_MACConfig8812(padapter);
	if (ret != _SUCCESS) {
		return ret;
	}
#endif	

	_InitQueueReservedPage(padapter);
	_InitQueuePriority(padapter);

	_InitPageBoundary(padapter);
	_InitTransferPageSize(padapter);

	// Get Rx PHY status in order to report RSSI and others.
	_InitDriverInfoSize(padapter, DRVINFO_SZ);

	_InitTRxAggregationSetting(padapter);

	hal_init_macaddr(padapter);
	_InitNetworkType(padapter);
	_InitWMACSetting(padapter);

	_InitAdaptiveCtrl(padapter);
	_InitEDCA(padapter);
	_InitRateFallback(padapter);
	_InitRetryFunction(padapter);

	_InitBeaconParameters(padapter);
	_InitBeaconMaxError(padapter, _TRUE);

	_InitInterrupt(padapter);

	_InitBurstPktLen(padapter);

#ifdef CONFIG_TX_EARLY_MODE
	if (pHalData->bEarlyModeEnable)
	{
		DBG_8192C("%s: EarlyMode Enabled\n", __FUNCTION__);

		val8 = rtw_read8(padapter, REG_EARLY_MODE_CONTROL);
		val8 |= 0xF;
		rtw_write8(padapter, REG_EARLY_MODE_CONTROL, val8);
		rtw_write8(padapter, REG_EARLY_MODE_CONTROL+3, 0x80);

		val8 = rtw_read8(padapter, REG_TCR+1);
		val8 |= 0x40;
		rtw_write8(padapter,REG_TCR+1, val8);
	}
	else
#endif
	{
		rtw_write8(padapter, REG_EARLY_MODE_CONTROL, 0);
	}

	if (pHalData->bRDGEnable) {
		InitRDGSetting8812A(padapter);
	}


#if defined(CONFIG_CONCURRENT_MODE) || defined(CONFIG_TX_MCAST2UNI)
#ifdef CONFIG_CHECK_AC_LIFETIME
	// Enable lifetime check for the four ACs
	rtw_write8(padapter, REG_LIFETIME_CTRL, 0x0F);
#endif	// CONFIG_CHECK_AC_LIFETIME

#ifdef CONFIG_TX_MCAST2UNI
	rtw_write16(padapter, REG_PKT_VO_VI_LIFE_TIME, 0x0400);	// unit: 256us. 256ms
	rtw_write16(padapter, REG_PKT_BE_BK_LIFE_TIME, 0x0400);	// unit: 256us. 256ms
#else	// CONFIG_TX_MCAST2UNI
	rtw_write16(padapter, REG_PKT_VO_VI_LIFE_TIME, 0x3000);	// unit: 256us. 3s
	rtw_write16(padapter, REG_PKT_BE_BK_LIFE_TIME, 0x3000);	// unit: 256us. 3s
#endif	// CONFIG_TX_MCAST2UNI
#endif	// CONFIG_CONCURRENT_MODE || CONFIG_TX_MCAST2UNI


#ifdef CONFIG_LED
	_InitHWLed(padapter);
#endif // CONFIG_LED

	//
	// Initialize BB related configurations.
	//
#if (HAL_BB_ENABLE == 1)
	ret = PHY_BBConfig8812(padapter);
	if (ret == _FAIL) {
		goto exit;
	}
#endif

	// If RF is on, we need to init RF. Otherwise, skip the procedure.
	// We need to follow SU method to change the RF cfg.txt. Default disable RF TX/RX mode.
	// if(pHalData->eRFPowerState == eRfOn)
#if (HAL_RF_ENABLE == 1)
	ret = PHY_RFConfig8812(padapter);
	if (ret != _SUCCESS) {
		goto exit;
	}
#endif

	rtw_write8(padapter, REG_SECONDARY_CCA_CTRL_8812, 0x3);	// CCA 
	rtw_write8(padapter, 0x976, 0);	// hpfan_todo: 2nd CCA related

	if (padapter->registrypriv.channel <= 14)
		PHY_SwitchWirelessBand8812(padapter, BAND_ON_2_4G);
	else
		PHY_SwitchWirelessBand8812(padapter, BAND_ON_5G);

	rtw_hal_set_chnl_bw(padapter, padapter->registrypriv.channel,
		CHANNEL_WIDTH_20, HAL_PRIME_CHNL_OFFSET_DONT_CARE, HAL_PRIME_CHNL_OFFSET_DONT_CARE);

	//
	// Joseph Note: Keep RfRegChnlVal for later use.
	//
	pHalData->RfRegChnlVal[0] = PHY_QueryRFReg(padapter, 0, RF_CHNLBW, bRFRegOffsetMask);
	pHalData->RfRegChnlVal[1] = PHY_QueryRFReg(padapter, 1, RF_CHNLBW, bRFRegOffsetMask);


	invalidate_cam_all(padapter);

	_InitAntenna_Selection(padapter);

	// HW SEQ CTRL
	//set 0x0 to 0xFF by tynli. Default enable HW SEQ NUM.
	rtw_write8(padapter, REG_HWSEQ_CTRL, 0xFF);

	//
	// Disable BAR, suggested by Scott
	// 2010.04.09 add by hpfan
	//
//	rtw_write32(padapter, REG_BAR_MODE_CTRL, 0x0201ffff);
	rtw_write32(padapter, REG_BAR_MODE_CTRL, 0x0001ffff);

	if (pregistrypriv->wifi_spec)
		rtw_write16(padapter, REG_FAST_EDCA_CTRL ,0);

#ifdef CONFIG_MAC_LOOPBACK_DRIVER
	val8 = rtw_read8(padapter, REG_SYS_FUNC_EN);
	val8 &= ~(FEN_BBRSTB|FEN_BB_GLB_RSTn);
	rtw_write8(padapter, REG_SYS_FUNC_EN,val8);

	rtw_write8(padapter, REG_RD_CTRL, 0x0F);
	rtw_write8(padapter, REG_RD_CTRL+1, 0xCF);
	rtw_write8(padapter, REG_TXPKTBUF_WMAC_LBK_BF_HD_8812, 0x80);
	rtw_write32(padapter, REG_CR, 0x0b0202ff);
#endif

	// Clear SDIO_TX_CTRL and SDIO_INT_TIMEOUT
	rtw_write32(padapter, SDIO_LOCAL_BASE|SDIO_REG_TX_CTRL, 0);

//	_RfPowerSave(padapter);

#if (MP_DRIVER == 1)
	if (padapter->registrypriv.mp_mode == 1)
	{
		padapter->mppriv.channel = pHalData->CurrentChannel;
		MPT_InitializeAdapter(padapter, padapter->mppriv.channel);
	}
	else
#endif // (MP_DRIVER == 1)
	{
		pwrctrlpriv->rf_pwrstate = rf_on;

#if 0
		//0x4c6[3] 1: RTS BW = Data BW
		//0: RTS BW depends on CCA / secondary CCA result.
		val8 = rtw_read8(padapter, REG_QUEUE_CTRL);
		val8 &= 0xF7;
		rtw_write8(padapter, REG_QUEUE_CTRL, val8);

		// enable Tx report.
		rtw_write8(padapter,  REG_FWHW_TXQ_CTRL+1, 0x0F);

		// Suggested by SD1 pisa. Added by tynli. 2011.10.21.
		// Pretx_en, for WEP/TKIP SEC
		rtw_write8(padapter, REG_EARLY_MODE_CONTROL_8812+3, 0x01);
#endif
	}

#ifdef CONFIG_BT_COEXIST
	// Init BT hw config.
	rtw_btcoex_HAL_Initialize(padapter, _FALSE);
#endif

	// Enable MACTXEN/MACRXEN block
	val8 = rtw_read8(padapter, REG_CR);
	val8 |= MACTXEN|MACRXEN;
	rtw_write8(padapter, REG_CR, val8);

	rtw_hal_set_hwreg(padapter, HW_VAR_NAV_UPPER, (u8*)&NavUpper);

	//
	// Update current Tx FIFO page status.
	//
	HalQueryTxBufferStatus8821AS(padapter);
	HalQueryTxOQTBufferStatus8821ASdio(padapter);
	pHalData->SdioTxOQTMaxFreeSpace = pHalData->SdioTxOQTFreeSpace;


#ifdef CONFIG_XMIT_ACK
	// ack for xmit mgmt frames.
	{
		u32 txq_ctrl;
		txq_ctrl = rtw_read32(padapter, REG_FWHW_TXQ_CTRL);
		txq_ctrl |= BIT(12);
		rtw_write32(padapter, REG_FWHW_TXQ_CTRL, txq_ctrl);
	}
#endif //CONFIG_XMIT_ACK

	rtl8812_InitHalDm(padapter);

exit:

	RT_TRACE(_module_hci_hal_init_c_, _drv_info_, ("-%s\n", __FUNCTION__));

	return ret;
}

static u32 _HalDeinit(PADAPTER padapter)
{
#ifdef CONFIG_MP_INCLUDED
	if (padapter->registrypriv.mp_mode == 1)
		MPT_DeInitAdapter(padapter);
#endif

	if (padapter->hw_init_completed == _TRUE)
		hal_poweroff_8812as(padapter);

	return _SUCCESS;
}

static void _InterfaceConfigure(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;
	struct dvobj_priv *pdvobjpriv;
	struct registry_priv *pregistrypriv;
	u8 bWiFiConfig;


	pHalData = GET_HAL_DATA(padapter);
	pdvobjpriv = adapter_to_dvobj(padapter);
	pregistrypriv = &padapter->registrypriv;
	bWiFiConfig	= pregistrypriv->wifi_spec;

	pdvobjpriv->RtOutPipe[0] = WLAN_TX_HIQ_DEVICE_ID;
	pdvobjpriv->RtOutPipe[1] = WLAN_TX_MIQ_DEVICE_ID;
	pdvobjpriv->RtOutPipe[2] = WLAN_TX_LOQ_DEVICE_ID;

	if (bWiFiConfig)
		pHalData->OutEpNumber = 2;
	else
		pHalData->OutEpNumber = SDIO_MAX_TX_QUEUE;

	switch (pHalData->OutEpNumber)
	{
		case 3:
			pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_LQ | TX_SELE_NQ;
			break;
		case 2:
			pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_NQ;
			break;
		case 1:
			pHalData->OutEpQueueSel = TX_SELE_HQ;
			break;
		default:
			break;
	}

	Hal_MappingOutPipe(padapter, pHalData->OutEpNumber);

	pHalData->SdioRxFIFOCnt = 0;
}

static void
_EfuseParseMACAddr(PADAPTER padapter, PEEPROM_EFUSE_PRIV pEEPROM)
{
	u16 i, usValue;
	u8 *hwinfo;
#ifdef CONFIG_RTL8821A
	u8 sMacAddr[6] = {0x00, 0xE0, 0x4C, 0x88, 0x21, 0x00};
#else // CONFIG_RTL8812A
	u8 sMacAddr[6] = {0x00, 0xE0, 0x4C, 0x88, 0x12, 0x00};
#endif // CONFIG_RTL8812A


	pEEPROM = GET_EEPROM_EFUSE_PRIV(padapter);
	hwinfo = pEEPROM->efuse_eeprom_data;

	if (pEEPROM->bautoload_fail_flag)
	{
//		sMacAddr[5] = (u1Byte)GetRandomNumber(1, 254);
		for (i=0; i<6; i++)
			pEEPROM->mac_addr[i] = sMacAddr[i];
	}
	else
	{
		_rtw_memcpy(pEEPROM->mac_addr, &hwinfo[EEPROM_MAC_ADDR_8821AS], ETH_ALEN);
	}

	DBG_8192C("%s: Permanent Address=" MAC_FMT "\n",
		  __FUNCTION__, MAC_ARG(pEEPROM->mac_addr));
}

static void _ParsePROMContent(PADAPTER padapter)
{
	PEEPROM_EFUSE_PRIV pEEPROM = GET_EEPROM_EFUSE_PRIV(padapter);
	u8 *hwinfo;
	u8 balfail;


	DBG_8192C("+%s\n", __FUNCTION__);

	pEEPROM = GET_EEPROM_EFUSE_PRIV(padapter);
	hwinfo = pEEPROM->efuse_eeprom_data;

	//
	// Read eeprom/efuse content
	//
	InitPGData8812A(padapter);

	//
	// Parse the eeprom/efuse content
	//
	Hal_EfuseParseIDCode8812A(padapter, hwinfo);

	balfail = pEEPROM->bautoload_fail_flag;
	Hal_ReadPROMVersion8812A(padapter, hwinfo, balfail);
	Hal_ReadTxPowerInfo8812A(padapter, hwinfo, balfail);
	Hal_ReadBoardType8812A(padapter, hwinfo, balfail);
	Hal_ReadChannelPlan8812A(padapter, hwinfo, balfail);
	Hal_EfuseParseXtal_8812A(padapter, hwinfo, balfail);
	Hal_ReadThermalMeter_8812A(padapter, hwinfo, balfail);
	Hal_ReadAntennaDiversity8812A(padapter, hwinfo, balfail);
	if(IS_HARDWARE_TYPE_8821(padapter)) {
		Hal_ReadPAType_8821A(padapter, hwinfo, balfail);
	} else {
		Hal_ReadAmplifierType_8812A(padapter, hwinfo, balfail);
		Hal_ReadRFEType_8812A(padapter, hwinfo, balfail);
	}

	//
	// Read Bluetooth co-exist and initialize
	//
	Hal_EfuseParseBTCoexistInfo8812A(padapter, hwinfo, balfail);

	//
	// Parse the eeprom/efuse interface content
	//
	_EfuseParseMACAddr(padapter, pEEPROM);

	//
	// The following part initialize some vars by PG info.
	//
//	_CustomizeByCustomerID8821AS(padapter);

	DBG_8192C("-%s\n", __FUNCTION__);
}

static void _ReadPROMContent(PADAPTER padapter)
{
	CheckAutoloadState8812A(padapter);
	_ParsePROMContent(padapter);
}

static void _InitOtherVariable(PADAPTER padapter)
{
}

/*
 * Description:
 * 	Read HW adapter information by E-Fuse or EEPROM according CR9346 reported.
 *
 * Assumption:
 * 	PASSIVE_LEVEL (SDIO interface)
 */
static void _ReadAdapterInfo(PADAPTER padapter)
{
	u32 start;


//	DBG_8192C("+%s\n", __FUNCTION__);

	// Read EEPROM size before call any EEPROM function
	padapter->EepromAddressSize = GetEEPROMSize8812A(padapter);

	// before access eFuse, make sure card enable has been called
	_CardEnable(padapter);

	start = rtw_get_current_time();

	_ReadPROMContent(padapter);

	ReadRFType8812A(padapter);

	_InitOtherVariable(padapter);

	DBG_8192C("-%s: cost %d ms\n", __FUNCTION__, rtw_get_passing_time_ms(start));
}

static void _startThread8821AS(PADAPTER padapter)
{
	struct xmit_priv *pxmitpriv;


	pxmitpriv = &padapter->xmitpriv;

	rtl8812_start_thread(padapter);

#ifndef CONFIG_SDIO_TX_TASKLET
	pxmitpriv->SdioXmitThread = kthread_run(XmitThread8821AS, padapter, "RTW_XMIT_SDIO_THREAD");
	if (pxmitpriv->SdioXmitThread < 0) {
		DBG_8192C("%s: start rtl8188es_xmit_thread FAIL!!\n", __FUNCTION__);
	}
#endif
}

static void _stopThread8821AS(PADAPTER padapter)
{
	struct xmit_priv *pxmitpriv;


	pxmitpriv = &padapter->xmitpriv;

#ifndef CONFIG_SDIO_TX_TASKLET
	// stop xmit_buf_thread
	if (pxmitpriv->SdioXmitThread)
	{
		_rtw_up_sema(&pxmitpriv->SdioXmitSema);
		_rtw_down_sema(&pxmitpriv->SdioXmitTerminateSema);
		pxmitpriv->SdioXmitThread = 0;
	}
#endif

	rtl8812_stop_thread(padapter);
}


/*
 * If variable not handled here,
 * some variables will be processed in SetHwReg8812A()
 */
static void SetHwReg8812AS(PADAPTER padapter, u8 variable, u8 *pval)
{
//	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);
	u8 val8;

_func_enter_;

	switch (variable)
	{
		case HW_VAR_SET_RPWM:
			// rpwm value only use BIT0(clock bit) ,BIT6(Ack bit), and BIT7(Toggle bit)
			// BIT0 value - 1: 32k, 0:40MHz.
			// BIT6 value - 1: report cpwm value after success set, 0:do not report.
			// BIT7 value - Toggle bit change.
			{
				val8 = *pval;
				val8 &= 0xC1;
				rtw_write8(padapter, SDIO_LOCAL_BASE|SDIO_REG_HRPWM1, val8);
			}
			break;

		case HW_VAR_RXDMA_AGG_PG_TH:
			val8 = *pval;

			// TH=1 => invalidate RX DMA aggregation
			// TH=0 => validate RX DMA aggregation, use init value.
			if (val8 == 0)
			{
				// enable RXDMA aggregation
				_RXAggrSwitch(padapter, _TRUE);
			}
			else
			{
				// disable RXDMA aggregation
				_RXAggrSwitch(padapter, _FALSE);
			}
			break;

		default:
			SetHwReg8812A(padapter, variable, pval);
			break;
	}

_func_exit_;
}

/*
 * If variable not handled here,
 * some variables will be processed in GetHwReg8812A()
 */
static void GetHwReg8812AS(PADAPTER padapter, u8 variable, u8 *val)
{
//	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);

_func_enter_;

	switch (variable)
	{
		default:
			GetHwReg8812A(padapter, variable, val);
			break;
	}

_func_exit_;
}

/*
 * Description:
 * 	Change default setting of specified variable.
 */
static u8 SetHalDefVar8812AS(PADAPTER padapter, HAL_DEF_VARIABLE eVariable, void *pValue)
{
//	PHAL_DATA_TYPE pHalData;
	u8 bResult;


//	pHalData = GET_HAL_DATA(padapter);
	bResult = _SUCCESS;

	switch (eVariable)
	{
		default:
			bResult = SetHalDefVar8812A(padapter, eVariable, pValue);
			break;
	}

	return bResult;
}

/*
 * Description:
 * 	Query setting of specified variable.
 */
static u8 GetHalDefVar8812AS(PADAPTER padapter, HAL_DEF_VARIABLE eVariable, void *pval)
{
	PHAL_DATA_TYPE pHalData;
	u8 bResult;


	pHalData = GET_HAL_DATA(padapter);
	bResult = _SUCCESS;

	switch (eVariable)
	{
		case HAL_DEF_TX_PAGE_BOUNDARY:
			if (!padapter->registrypriv.wifi_spec)
			{
				*(u8*)pval = TX_PAGE_BOUNDARY_8821;
			}
			else
			{
				*(u8*)pval = WMM_NORMAL_TX_PAGE_BOUNDARY_8821;
			}
			break;

		case HAL_DEF_TX_PAGE_BOUNDARY_WOWLAN:
#if 0
			*(u8*)pval = TX_PAGE_BOUNDARY_WOWLAN_8821;
#else
			*(u8*)pval = TX_PAGE_BOUNDARY_8821;
			DBG_8192C("%s: [ERROR] TX_PAGE_BOUNDARY_WOWLAN not defined!!\n", __FUNCTION__);
#endif
			break;

		default:
			bResult = GetHalDefVar8812A(padapter, eVariable, pval);
			break;
	}

	return bResult;
}

void rtl8821as_set_hal_ops(PADAPTER padapter)
{
	struct hal_ops *pHalFunc;

_func_enter_;

	pHalFunc = &padapter->HalFunc;
	rtl8812_set_hal_ops(pHalFunc);

	pHalFunc->hal_power_on = &_InitPowerOn_8812AS;
	pHalFunc->hal_power_off = &hal_poweroff_8812as;
	
	pHalFunc->hal_init = &_HalInit;
	pHalFunc->hal_deinit = &_HalDeinit;

	pHalFunc->init_xmit_priv = &InitXmitPriv8821AS;
	pHalFunc->free_xmit_priv = &FreeXmitPriv8821AS;

	pHalFunc->init_recv_priv = &InitRecvPriv8821AS;
	pHalFunc->free_recv_priv = &FreeRecvPriv8821AS;

#ifdef CONFIG_SW_LED
	pHalFunc->InitSwLeds = &rtl8812s_InitSwLeds;
	pHalFunc->DeInitSwLeds = &rtl8812s_DeInitSwLeds;
#else // !CONFIG_SW_LED, case of hw led or no led
	pHalFunc->InitSwLeds = NULL;
	pHalFunc->DeInitSwLeds = NULL;
#endif // !CONFIG_SW_LED

	pHalFunc->init_default_value = &InitDefaultValue8821A;
	pHalFunc->intf_chip_configure = &_InterfaceConfigure;
	pHalFunc->read_adapter_info = &_ReadAdapterInfo;

	pHalFunc->enable_interrupt = &EnableInterrupt8821AS;
	pHalFunc->disable_interrupt = &DisableInterrupt8821AS;

	pHalFunc->SetHwRegHandler = &SetHwReg8812AS;
	pHalFunc->GetHwRegHandler = &GetHwReg8812AS;
 	pHalFunc->SetHalDefVarHandler = &SetHalDefVar8812AS;
	pHalFunc->GetHalDefVarHandler = &GetHalDefVar8812AS;

	pHalFunc->hal_xmit = &HalXmit8821AS;
	pHalFunc->mgnt_xmit = &MgntXmit8821AS;
	pHalFunc->hal_xmitframe_enqueue = &HalXmitNoLock8821AS;

	pHalFunc->run_thread = &_startThread8821AS;
	pHalFunc->cancel_thread = &_stopThread8821AS;

#ifdef CONFIG_HOSTAPD_MLME
	pHalFunc->hostap_mgnt_xmit_entry = NULL;
#endif

#ifdef CONFIG_XMIT_THREAD_MODE
	pHalFunc->xmit_thread_handler = &XmitBufHandler8821AS;
#endif

_func_exit_;
}

