/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_pktinfo.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

/* Local include */
#include "hdmi_rx_pktinfo.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"

/*this parameter moved from wrapper.c */
/*
 * bool hdr_enable = true;
 */
static struct rxpkt_st rxpktsts;
struct packet_info_s rx_pkt;
int dv_nopacket_timeout = 30;
uint32_t gpkt_fifo_pri;
/*struct mutex pktbuff_lock;*/


static struct pkt_typeregmap_st pktmaping[] = {
	/*infoframe pkt*/
	{PKT_TYPE_INFOFRAME_VSI,	PFIFO_VS_EN},
	{PKT_TYPE_INFOFRAME_AVI,	PFIFO_AVI_EN},
	{PKT_TYPE_INFOFRAME_SPD,	PFIFO_SPD_EN},
	{PKT_TYPE_INFOFRAME_AUD,	PFIFO_AUD_EN},
	{PKT_TYPE_INFOFRAME_MPEGSRC,	PFIFO_MPEGS_EN},
	{PKT_TYPE_INFOFRAME_NVBI,	PFIFO_NTSCVBI_EN},
	{PKT_TYPE_INFOFRAME_DRM,	PFIFO_DRM_EN},
	/*other pkt*/
	{PKT_TYPE_ACR,				PFIFO_ACR_EN},
	{PKT_TYPE_GCP,				PFIFO_GCP_EN},
	{PKT_TYPE_ACP,				PFIFO_ACP_EN},
	{PKT_TYPE_ISRC1,			PFIFO_ISRC1_EN},
	{PKT_TYPE_ISRC2,			PFIFO_ISRC2_EN},
	{PKT_TYPE_GAMUT_META,		PFIFO_GMT_EN},
	{PKT_TYPE_AUD_META,			PFIFO_AMP_EN},
	/*end of the table*/
	{K_FLAG_TAB_END,			K_FLAG_TAB_END},
};

/*
 * uint32_t timerbuff[50];
 * uint32_t timerbuff_idx = 0;
 */

struct st_pkt_test_buff *pkt_testbuff;

void rx_pkt_status(void)
{
	/*uint32_t i;*/
	rx_pr("pkt module ver: 1.1\n");
	rx_pr("packet_fifo_cfg=0x%x\n", packet_fifo_cfg);
	/*rx_pr("pdec_ists_en=0x%x\n\n", pdec_ists_en);*/

	rx_pr("fifo_Int_cnt=%d\n", rxpktsts.fifo_Int_cnt);
	rx_pr("fifo_pkt_num=%d\n", rxpktsts.fifo_pkt_num);

	rx_pr("pkt_cnt_avi=%d\n", rxpktsts.pkt_cnt_avi);
	rx_pr("pkt_cnt_vsi=%d\n", rxpktsts.pkt_cnt_vsi);
	rx_pr("pkt_cnt_drm=%d\n", rxpktsts.pkt_cnt_drm);
	rx_pr("pkt_cnt_spd=%d\n", rxpktsts.pkt_cnt_spd);
	rx_pr("pkt_cnt_audif=%d\n", rxpktsts.pkt_cnt_audif);
	rx_pr("pkt_cnt_mpeg=%d\n", rxpktsts.pkt_cnt_mpeg);
	rx_pr("pkt_cnt_nvbi=%d\n", rxpktsts.pkt_cnt_nvbi);

	rx_pr("pkt_cnt_acr=%d\n", rxpktsts.pkt_cnt_acr);
	rx_pr("pkt_cnt_gcp=%d\n", rxpktsts.pkt_cnt_gcp);
	rx_pr("pkt_cnt_acp=%d\n", rxpktsts.pkt_cnt_acp);
	rx_pr("pkt_cnt_isrc1=%d\n", rxpktsts.pkt_cnt_isrc1);
	rx_pr("pkt_cnt_isrc2=%d\n", rxpktsts.pkt_cnt_isrc2);
	rx_pr("pkt_cnt_gameta=%d\n", rxpktsts.pkt_cnt_gameta);
	rx_pr("pkt_cnt_amp=%d\n", rxpktsts.pkt_cnt_amp);

	rx_pr("pkt_cnt_vsi_ex=%d\n", rxpktsts.pkt_cnt_vsi_ex);
	rx_pr("pkt_cnt_drm_ex=%d\n", rxpktsts.pkt_cnt_drm_ex);
	rx_pr("pkt_cnt_gmd_ex=%d\n", rxpktsts.pkt_cnt_gmd_ex);
	rx_pr("pkt_cnt_aif_ex=%d\n", rxpktsts.pkt_cnt_aif_ex);
	rx_pr("pkt_cnt_avi_ex=%d\n", rxpktsts.pkt_cnt_avi_ex);
	rx_pr("pkt_cnt_acr_ex=%d\n", rxpktsts.pkt_cnt_acr_ex);
	rx_pr("pkt_cnt_gcp_ex=%d\n", rxpktsts.pkt_cnt_gcp_ex);
	rx_pr("pkt_cnt_amp_ex=%d\n", rxpktsts.pkt_cnt_amp_ex);
	rx_pr("pkt_cnt_nvbi_ex=%d\n", rxpktsts.pkt_cnt_nvbi_ex);

	rx_pr("pkt_chk_flg=%d\n", rxpktsts.pkt_chk_flg);

	rx_pr("FIFO_STS=0x%x(b31-b16/b15-b0)\n",
			hdmirx_rd_dwc(DWC_PDEC_FIFO_STS));
	rx_pr("FIFO_STS1=0x%x(b15-b0)\n",
			hdmirx_rd_dwc(DWC_PDEC_FIFO_STS1));
}

void rx_pkt_debug(void)
{
	uint32_t data32;

	rx_pr("\npktinfo size=%d\n", sizeof(union pktinfo));
	rx_pr("infoframe_st size=%d\n", sizeof(union infoframe_u));
	rx_pr("fifo_rawdata_st size=%d\n", sizeof(struct fifo_rawdata_st));
	rx_pr("vsi_infoframe_st size=%d\n", sizeof(struct vsi_infoframe_st));
	rx_pr("avi_infoframe_st size=%d\n", sizeof(struct avi_infoframe_st));
	rx_pr("spd_infoframe_st size=%d\n", sizeof(struct spd_infoframe_st));
	rx_pr("aud_infoframe_st size=%d\n", sizeof(struct aud_infoframe_st));
	rx_pr("ms_infoframe_st size=%d\n", sizeof(struct ms_infoframe_st));
	rx_pr("vbi_infoframe_st size=%d\n", sizeof(struct vbi_infoframe_st));
	rx_pr("drm_infoframe_st size=%d\n", sizeof(struct drm_infoframe_st));

	rx_pr("acr_ptk_st size=%d\n",
				sizeof(struct acr_ptk_st));
	rx_pr("aud_sample_pkt_st size=%d\n",
				sizeof(struct aud_sample_pkt_st));
	rx_pr("gcp_pkt_st size=%d\n", sizeof(struct gcp_pkt_st));
	rx_pr("acp_pkt_st size=%d\n", sizeof(struct acp_pkt_st));
	rx_pr("isrc_pkt_st size=%d\n", sizeof(struct isrc_pkt_st));
	rx_pr("onebit_aud_pkt_st size=%d\n",
				sizeof(struct obasmp_pkt_st));
	rx_pr("dst_aud_pkt_st size=%d\n",
				sizeof(struct dstaud_pkt_st));
	rx_pr("hbr_aud_pkt_st size=%d\n",
				sizeof(struct hbraud_pkt_st));
	rx_pr("gamut_Meta_pkt_st size=%d\n",
				sizeof(struct gamutmeta_pkt_st));
	rx_pr("aud_3d_smppkt_st size=%d\n",
				sizeof(struct a3dsmp_pkt_st));
	rx_pr("oneb3d_smppkt_st size=%d\n",
				sizeof(struct ob3d_smppkt_st));
	rx_pr("aud_mtdata_pkt_st size=%d\n",
				sizeof(struct audmtdata_pkt_st));
	rx_pr("mulstr_audsamp_pkt_st size=%d\n",
				sizeof(struct msaudsmp_pkt_st));
	rx_pr("onebmtstr_smaud_pkt_st size=%d\n",
				sizeof(struct obmaudsmp_pkt_st));

	memset(&rxpktsts, 0, sizeof(struct rxpkt_st));

	data32 = hdmirx_rd_dwc(DWC_PDEC_CTRL);
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_VSI));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_AVI));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_SPD));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_AUD));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_VSI));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_MPEGSRC));
	if (rx.chip_id != CHIP_ID_TXHD) {
		data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_NVBI));
		data32 |= (rx_pkt_type_mapping(PKT_TYPE_INFOFRAME_DRM));
		data32 |= (rx_pkt_type_mapping(PKT_TYPE_AUD_META));
	}
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_ACR));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_GCP));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_ACP));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_ISRC1));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_ISRC2));
	data32 |= (rx_pkt_type_mapping(PKT_TYPE_GAMUT_META));
	hdmirx_wr_dwc(DWC_PDEC_CTRL, data32);
	rx_pr("enable fifo\n");

}


void rx_get_pd_fifo_param(enum pkt_type_e pkt_type,
		struct pd_infoframe_s *pkt_info)
{
	switch (pkt_type) {
	case PKT_TYPE_INFOFRAME_VSI:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.vs_info; */
			memcpy(&pkt_info, &rx_pkt.vs_info,
				sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_vsi_ex(&pkt_info);
		break;
	case PKT_TYPE_INFOFRAME_AVI:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.avi_info; */
			memcpy(&pkt_info, &rx_pkt.avi_info,
				sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_avi_ex(&pkt_info);
		break;
	case PKT_TYPE_INFOFRAME_SPD:
		/* srcbuff = &rx.spd_info; */
		memcpy(&pkt_info, &rx_pkt.spd_info,
			sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_INFOFRAME_AUD:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.aud_pktinfo; */
			memcpy(&pkt_info, &rx_pkt.aud_pktinfo,
				sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_audif_ex(&pkt_info);
		break;
	case PKT_TYPE_INFOFRAME_MPEGSRC:
		/* srcbuff = &rx.mpegs_info; */
		memcpy(&pkt_info, &rx_pkt.mpegs_info,
			sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_INFOFRAME_NVBI:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.ntscvbi_info; */
			memcpy(&pkt_info, &rx_pkt.ntscvbi_info,
				sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_ntscvbi_ex(&pkt_info);
		break;
	case PKT_TYPE_INFOFRAME_DRM:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.drm_info; */
			memcpy(&pkt_info, &rx_pkt.drm_info,
				sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_drm_ex(&pkt_info);
		break;
	case PKT_TYPE_ACR:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.acr_info; */
			memcpy(&pkt_info, &rx_pkt.acr_info,
				sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_acr_ex(&pkt_info);
		break;
	case PKT_TYPE_GCP:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.gcp_info; */
			memcpy(&pkt_info, &rx_pkt.gcp_info,
				sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_gcp_ex(&pkt_info);
		break;
	case PKT_TYPE_ACP:
		/* srcbuff = &rx.acp_info; */
		memcpy(&pkt_info, &rx_pkt.acp_info,
			sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_ISRC1:
		/* srcbuff = &rx.isrc1_info; */
		memcpy(&pkt_info, &rx_pkt.isrc1_info,
			sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_ISRC2:
		/* srcbuff = &rx.isrc2_info; */
		memcpy(&pkt_info, &rx_pkt.isrc2_info,
			sizeof(struct pd_infoframe_s));
		break;
	case PKT_TYPE_GAMUT_META:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.gameta_info; */
			memcpy(&pkt_info, &rx_pkt.gameta_info,
				sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_gmd_ex(&pkt_info);
		break;
	case PKT_TYPE_AUD_META:
		if (rx_pkt_get_fifo_pri())
			/* srcbuff = &rx.amp_info; */
			memcpy(&pkt_info, &rx_pkt.amp_info,
				sizeof(struct pd_infoframe_s));
		else
			rx_pkt_get_amp_ex(&pkt_info);
		break;
	default:
		if (pd_fifo_buf != NULL)
			/* srcbuff = pd_fifo_buf; */
			memcpy(&pkt_info, &pd_fifo_buf,
				sizeof(struct pd_infoframe_s));
		else
			pr_err("err:pd_fifo_buf is empty\n");
		break;
		}
}
#if 0
void rx_debug_dump_vs_status(void)
{
	struct vsi_info_s *vs;

	vs = &rx.vsi_info;

	rx_pr("dobv:%d\n", vs->dolby_vision);
	rx_pr("ieee:%0x\n", vs->identifier);
	rx_pr("dobvsts:%d\n", vs->dolby_vision_sts);
	rx_pr("3dst:0x%x\n", vs->_3d_structure);
	rx_pr("3dext:0x%x\n", vs->_3d_ext_data);
}
#endif

/*
 * hdmi rx packet module debug interface, if system not
 * enable pkt fifo module, you need first fun "debugfifo" cmd
 * to enable hw de-code pkt. and then rum "status", you will
 * see the pkt_xxx_cnt value is increasing. means have received
 * pkt info.
 * @param  input [are group of string]
 *  cmd style: pktinfo [param1] [param2]
 *  cmd ex:pktinfo dump 0x82
 *  ->means dump avi infofram pkt content
 * @return [no]
 */
void rx_debug_pktinfo(char input[][20])
{
	uint32_t sts = 0;
	uint32_t enable = 0;
	long res = 0;

	if (strncmp(input[1], "debugfifo", 9) == 0) {
		/*open all pkt interrupt source for debug*/
		rx_pkt_debug();
		enable |= (PD_FIFO_START_PASS | PD_FIFO_OVERFL);
		pdec_ists_en |= enable;
		rx_pr("pdec_ists_en=0x%x\n", pdec_ists_en);
		rx_irq_en(1);
	} else if (strncmp(input[1], "debugext", 8) == 0) {
		if ((rx.chip_id == CHIP_ID_TXLX) ||
			(rx.chip_id == CHIP_ID_TXHD))
			enable |= _BIT(30);/* DRC_RCV*/
		else
			enable |= _BIT(9);/* DRC_RCV*/
		enable |= _BIT(20);/* GMD_RCV */
		enable |= _BIT(19);/* AIF_RCV */
		enable |= _BIT(18);/* AVI_RCV */
		enable |= _BIT(17);/* ACR_RCV */
		enable |= _BIT(16);/* GCP_RCV */
		enable |= _BIT(15);/* VSI_RCV CHG*/
		enable |= _BIT(14);/* AMP_RCV*/
		pdec_ists_en |= enable;
		rx_pr("pdec_ists_en=0x%x\n", pdec_ists_en);
		rx_irq_en(1);
	} else if (strncmp(input[1], "status", 6) == 0)
		rx_pkt_status();
	else if (strncmp(input[1], "dump", 7) == 0) {
		/*check input type*/
		if (kstrtol(input[2], 16, &res) < 0)
			rx_pr("error input:fmt is 0xValue\n");
		rx_pkt_dump(res);
	} else if (strncmp(input[1], "irqdisable", 10) == 0) {
		if (strncmp(input[2], "fifo", 4) == 0)
			sts = (PD_FIFO_START_PASS|PD_FIFO_OVERFL);
		else if (strncmp(input[2], "drm", 3) == 0) {
			if ((rx.chip_id == CHIP_ID_TXLX) ||
				(rx.chip_id == CHIP_ID_TXHD))
				sts = _BIT(30);
			else
				sts = _BIT(9);
		} else if (strncmp(input[2], "gmd", 3) == 0)
			sts = _BIT(20);
		else if (strncmp(input[2], "aif", 3) == 0)
			sts = _BIT(19);
		else if (strncmp(input[2], "avi", 3) == 0)
			sts = _BIT(18);
		else if (strncmp(input[2], "acr", 3) == 0)
			sts = _BIT(17);
		else if (strncmp(input[2], "gcp", 3) == 0)
			sts = GCP_RCV;
		else if (strncmp(input[2], "vsi", 3) == 0)
			sts = VSI_RCV;
		else if (strncmp(input[2], "amp", 3) == 0)
			sts = _BIT(14);

		pdec_ists_en &= ~sts;
		rx_pr("pdec_ists_en=0x%x\n", pdec_ists_en);
		/*disable irq*/
		hdmirx_wr_dwc(DWC_PDEC_IEN_CLR, sts);
	} else if (strncmp(input[1], "irqenable", 9) == 0) {
		sts = hdmirx_rd_dwc(DWC_PDEC_IEN);
		if (strncmp(input[2], "fifo", 4) == 0)
			enable |= (PD_FIFO_START_PASS|PD_FIFO_OVERFL);
		else if (strncmp(input[2], "drm", 3) == 0) {
			if ((rx.chip_id == CHIP_ID_TXLX) ||
				(rx.chip_id == CHIP_ID_TXHD))
				enable |= _BIT(30);
			else
				enable |= _BIT(9);
		} else if (strncmp(input[2], "gmd", 3) == 0)
			enable |= _BIT(20);
		else if (strncmp(input[2], "aif", 3) == 0)
			enable |= _BIT(19);
		else if (strncmp(input[2], "avi", 3) == 0)
			enable |= _BIT(18);
		else if (strncmp(input[2], "acr", 3) == 0)
			enable |= _BIT(17);
		else if (strncmp(input[2], "gcp", 3) == 0)
			enable |= GCP_RCV;
		else if (strncmp(input[2], "vsi", 3) == 0)
			enable |= VSI_RCV;
		else if (strncmp(input[2], "amp", 3) == 0)
			enable |= _BIT(14);
		pdec_ists_en = enable|sts;
		rx_pr("pdec_ists_en=0x%x\n", pdec_ists_en);
		/*open irq*/
		hdmirx_wr_dwc(DWC_PDEC_IEN_SET, pdec_ists_en);
		/*hdmirx_irq_open()*/
	} else if (strncmp(input[1], "fifopkten", 9) == 0) {
		/*check input*/
		if (kstrtol(input[2], 16, &res) < 0)
			return;
		rx_pr("pkt ctl disable:0x%x", res);
		/*check pkt enable ctl bit*/
		sts = rx_pkt_type_mapping((uint32_t)res);
		if (sts == 0)
			return;

		packet_fifo_cfg |= sts;
		/* not work immediately ?? meybe int is not open*/
		enable = hdmirx_rd_dwc(DWC_PDEC_CTRL);
		enable |= sts;
		hdmirx_wr_dwc(DWC_PDEC_CTRL, enable);
	} else if (strncmp(input[1], "fifopktdis", 10) == 0) {
		/*check input*/
		if (kstrtol(input[2], 16, &res) < 0)
			return;
		rx_pr("pkt ctl disable:0x%x", res);
		/*check pkt enable ctl bit*/
		sts = rx_pkt_type_mapping((uint32_t)res);
		if (sts == 0)
			return;

		packet_fifo_cfg &= (~sts);
		/* not work immediately ?? meybe int is not open*/
		enable = hdmirx_rd_dwc(DWC_PDEC_CTRL);
		enable &= (~sts);
		hdmirx_wr_dwc(DWC_PDEC_CTRL, enable);
	} else if (strncmp(input[1], "contentchk", 10) == 0) {
		/*check input*/
		if (kstrtol(input[2], 16, &res) < 0) {
			rx_pr("error input:fmt is 0xXX\n");
			return;
		}
		rx_pkt_content_chk_en(res);
	} else if (strncmp(input[1], "pdfifopri", 9) == 0) {
		/*check input*/
		if (kstrtol(input[2], 16, &res) < 0) {
			rx_pr("error input:fmt is 0xXX\n");
			return;
		}
		rx_pkt_set_fifo_pri(res);
	}
}

static void rx_pktdump_raw(void *pdata)
{
	uint8_t i;
	union infoframe_u *pktdata = pdata;

	rx_pr(">---raw data detail------>\n");
	rx_pr("HB0:0x%x\n", pktdata->raw_infoframe.pkttype);
	rx_pr("HB1:0x%x\n", pktdata->raw_infoframe.version);
	rx_pr("HB2:0x%x\n", pktdata->raw_infoframe.length);
	rx_pr("RSD:0x%x\n", pktdata->raw_infoframe.rsd);

	for (i = 0; i < 28; i++)
		rx_pr("PB%d:0x%x\n", i, pktdata->raw_infoframe.PB[i]);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_vsi(void *pdata)
{
	struct vsi_infoframe_st *pktdata = pdata;
	uint32_t i;

	rx_pr(">---vsi infoframe detail -------->\n");
	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->ver_st.version);
	rx_pr("chgbit: %d\n", pktdata->ver_st.chgbit);
	rx_pr("length: %d\n", pktdata->length);

	rx_pr("ieee: 0x%x\n", pktdata->ieee);
	rx_pr("3d vdfmt: 0x%x\n", pktdata->sbpkt.vsi.vdfmt);

	if (pktdata->length == E_DV_LENGTH_24) {
		/*dobly version v0 pkt*/

	} else {
		if (pktdata->sbpkt.vsi.vdfmt == 0) {
			rx_pr("no additional vd fmt\n");
		} else if (pktdata->sbpkt.vsi.vdfmt == 1) {
			/*extended resolution format*/
			rx_pr("hdmi vic: 0x%x\n",
				pktdata->sbpkt.vsi.hdmi_vic);
		} else if (pktdata->sbpkt.vsi.vdfmt == 2) {
			/*3D format*/
			rx_pr("3d struct: 0x%x\n",
				pktdata->sbpkt.vsi_3Dext.threeD_st);
			rx_pr("3d ext_data : 0x%x\n",
				pktdata->sbpkt.vsi_3Dext.threeD_ex);
		} else {
			rx_pr("unknown vd fmt\n");
		}
	}

	for (i = 0; i < 6; i++)
		rx_pr("payload %d : 0x%x\n", i,
			pktdata->sbpkt.payload.data[i]);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_avi(void *pdata)
{
	struct avi_infoframe_st *pktdata = pdata;

	rx_pr(">---avi infoframe detail -------->\n");
	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->version);
	rx_pr("length: %d\n", pktdata->length);

	if (pktdata->version == 1) {
		/*ver 1*/
		rx_pr("scaninfo S: 0x%x\n", pktdata->cont.v1.scaninfo);
		rx_pr("barinfo B : 0x%x\n", pktdata->cont.v1.barinfo);
		rx_pr("activeinfo A: 0x%x\n", pktdata->cont.v1.activeinfo);
		rx_pr("colorimetry Y: 0x%x\n",
					pktdata->cont.v1.colorindicator);

		rx_pr("fmt_ration R: 0x%x\n", pktdata->cont.v1.fmt_ration);
		rx_pr("pic_ration M: 0x%x\n", pktdata->cont.v1.pic_ration);
		rx_pr("colorimetry C: 0x%x\n", pktdata->cont.v1.colorimetry);

		rx_pr("colorimetry SC: 0x%x\n", pktdata->cont.v1.pic_scaling);
	} else {
		/*ver 2/3*/
		rx_pr("scaninfo S: 0x%x\n", pktdata->cont.v2v3.scaninfo);
		rx_pr("barinfo B: 0x%x\n", pktdata->cont.v2v3.barinfo);
		rx_pr("activeinfo A: 0x%x\n", pktdata->cont.v2v3.activeinfo);
		rx_pr("colorimetry Y: 0x%x\n",
					pktdata->cont.v2v3.colorindicator);

		rx_pr("fmt_ration R: 0x%x\n", pktdata->cont.v2v3.fmt_ration);
		rx_pr("pic_ration M: 0x%x\n", pktdata->cont.v2v3.pic_ration);
		rx_pr("colorimetry C: 0x%x\n", pktdata->cont.v2v3.colorimetry);

		rx_pr("pic_scaling SC: 0x%x\n", pktdata->cont.v2v3.pic_scaling);
		rx_pr("qt_range Q: 0x%x\n", pktdata->cont.v2v3.qt_range);
		rx_pr("ext_color EC : 0x%x\n", pktdata->cont.v2v3.ext_color);
		rx_pr("it_content ITC: 0x%x\n", pktdata->cont.v2v3.it_content);

		rx_pr("vic: 0x%x\n", pktdata->cont.v2v3.vic);

		rx_pr("pix_repeat PR: 0x%x\n",
				pktdata->cont.v2v3.pix_repeat);
		rx_pr("content_type CN: 0x%x\n",
				pktdata->cont.v2v3.content_type);
		rx_pr("ycc_range YQ: 0x%x\n",
				pktdata->cont.v2v3.ycc_range);
	}
	rx_pr("line_end_topbar: 0x%x\n",
				pktdata->line_num_end_topbar);
	rx_pr("line_start_btmbar: 0x%x\n",
				pktdata->line_num_start_btmbar);
	rx_pr("pix_num_left_bar: 0x%x\n",
				pktdata->pix_num_left_bar);
	rx_pr("pix_num_right_bar: 0x%x\n",
				pktdata->pix_num_right_bar);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_spd(void *pdata)
{
	struct spd_infoframe_st *pktdata = pdata;

	static const char * const spd_source_info[] = {
		/*0x00 */"unknown",
		/*0x01 */"Digital STB",
		/*0x02 */"DVD player",
		/*0x03 */"D-VHS",
		/*0x04*/ "HDD Videorecorder",
		/*0x05 */"DVC",
		/*0x06 */"DSC",
		/*0x07 */"Video CD",
		/*0x08 */"Game",
		/*0x09 */"PC general",
		/*0x0A */"Blu-Ray Disc (BD)",
		/*0x0B */"Super Audio CD",
		/*0x0C */"HD DVD",
		/*0x0D */"PMP",
	};

	rx_pr(">---spd infoframe detail ---->\n");

	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->version);
	rx_pr("length: %d\n", pktdata->length);

	rx_pr("vendor name: %s\n", pktdata->vendor_name);
	rx_pr("product des: %s\n", pktdata->product_des);
	rx_pr("source info: 0x%x\n", pktdata->source_info);
	if (pktdata->source_info <= 0x0d)
		rx_pr("source info: %s\n",
			spd_source_info[pktdata->source_info]);
	else
		rx_pr("unknown\n");

	rx_pr(">------------------>end\n");
}

static void rx_pktdump_aud(void *pdata)
{
	struct aud_infoframe_st *pktdata = pdata;

	rx_pr(">---audio infoframe detail ---->\n");

	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->version);
	rx_pr("length: %d\n", pktdata->length);

	rx_pr("ch_count(CC):0x%x\n", pktdata->ch_count);
	rx_pr("coding_type(CT):0x%x\n", pktdata->coding_type);
	rx_pr("sample_size(SS):0x%x\n", pktdata->sample_size);
	rx_pr("sample_frq(SF):0x%x\n", pktdata->sample_frq);

	rx_pr("fromat(PB3):0x%x\n", pktdata->fromat);
	rx_pr("mul_ch(CA):0x%x\n", pktdata->ca);
	rx_pr("lfep(BL):0x%x\n", pktdata->lfep);
	rx_pr("level_shift_value(LSV):0x%x\n", pktdata->level_shift_value);
	rx_pr("down_mix(DM_INH):0x%x\n", pktdata->down_mix);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_drm(void *pdata)
{
	struct drm_infoframe_st *pktdata = pdata;
	union infoframe_u pktraw;

	memset(&pktraw, 0, sizeof(union infoframe_u));

	rx_pr(">---drm infoframe detail -------->\n");
	rx_pr("type: 0x%x\n", pktdata->pkttype);
	rx_pr("ver: %d\n", pktdata->version);
	rx_pr("length: %d\n", pktdata->length);

	rx_pr("b1 eotf: 0x%x\n", pktdata->des_u.tp1.eotf);
	rx_pr("b2 meta des id: 0x%x\n", pktdata->des_u.tp1.meta_des_id);

	rx_pr("dis_pri_x0: %d\n", pktdata->des_u.tp1.dis_pri_x0);
	rx_pr("dis_pri_y0: %d\n", pktdata->des_u.tp1.dis_pri_y0);
	rx_pr("dis_pri_x1: %d\n", pktdata->des_u.tp1.dis_pri_x1);
	rx_pr("dis_pri_y1: %d\n", pktdata->des_u.tp1.dis_pri_y1);
	rx_pr("dis_pri_x2: %d\n", pktdata->des_u.tp1.dis_pri_x2);
	rx_pr("dis_pri_y2: %d\n", pktdata->des_u.tp1.dis_pri_y2);
	rx_pr("white_points_x: %d\n",
		pktdata->des_u.tp1.white_points_x);
	rx_pr("white_points_y: %d\n",
		pktdata->des_u.tp1.white_points_y);
	rx_pr("max_dislum: %d\n", pktdata->des_u.tp1.max_dislum);
	rx_pr("min_dislum: %d\n", pktdata->des_u.tp1.min_dislum);
	rx_pr("max_light_lvl: %d\n", pktdata->des_u.tp1.max_light_lvl);
	rx_pr("max_fa_light_lvl: %d\n", pktdata->des_u.tp1.max_fa_light_lvl);
	rx_pr(">------------------>end\n");
}

static void rx_pktdump_acr(void *pdata)
{
	struct acr_ptk_st *pktdata = pdata;
	uint32_t CTS;
	uint32_t N;

	rx_pr(">---audio clock regeneration detail ---->\n");

	rx_pr("type: 0x%0x\n", pktdata->pkttype);

	CTS = (pktdata->sbpkt1.SB1_CTS_H << 16) |
			(pktdata->sbpkt1.SB2_CTS_M << 8) |
			pktdata->sbpkt1.SB3_CTS_L;

	N = (pktdata->sbpkt1.SB4_N_H << 16) |
			(pktdata->sbpkt1.SB5_N_M << 8) |
			pktdata->sbpkt1.SB6_N_L;
	rx_pr("sbpkt1 CTS: %d\n", CTS);
	rx_pr("sbpkt1 N : %d\n", N);

	#if 0
	CTS = (pktdata->sbpkt2.SB1_CTS_H << 16) |
			(pktdata->sbpkt2.SB2_CTS_M << 8) |
			pktdata->sbpkt2.SB3_CTS_L;

	N = (pktdata->sbpkt2.SB4_N_H << 16) |
			(pktdata->sbpkt2.SB5_N_M << 8) |
			pktdata->sbpkt2.SB6_N_L;
	rx_pr("sbpkt2 CTS: %d\n", CTS);
	rx_pr("sbpkt2 N : %d\n", N);


	CTS = (pktdata->sbpkt3.SB1_CTS_H << 16) |
			(pktdata->sbpkt3.SB2_CTS_M << 8) |
			pktdata->sbpkt3.SB3_CTS_L;

	N = (pktdata->sbpkt3.SB4_N_H << 16) |
			(pktdata->sbpkt3.SB5_N_M << 8) |
			pktdata->sbpkt3.SB6_N_L;
	rx_pr("sbpkt3 CTS: %d\n", CTS);
	rx_pr("sbpkt3 N : %d\n", N);


	CTS = (pktdata->sbpkt4.SB1_CTS_H << 16) |
			(pktdata->sbpkt4.SB2_CTS_M << 8) |
			pktdata->sbpkt4.SB3_CTS_L;

	N = (pktdata->sbpkt4.SB4_N_H << 16) |
			(pktdata->sbpkt4.SB5_N_M << 8) |
			pktdata->sbpkt4.SB6_N_L;
	rx_pr("sbpkt4 CTS: %d\n", CTS);
	rx_pr("sbpkt4 N : %d\n", N);
	#endif
	rx_pr(">------------------>end\n");
}

void rx_pkt_dump(enum pkt_type_e typeID)
{
	struct packet_info_s *prx = &rx_pkt;
	union infoframe_u pktdata;

	rx_pr("dump cmd:0x%x\n", typeID);

	/*mutex_lock(&pktbuff_lock);*/
	memset(&pktdata, 0, sizeof(pktdata));
	switch (typeID) {
	/*infoframe pkt*/
	case PKT_TYPE_INFOFRAME_VSI:
		rx_pktdump_raw(&prx->vs_info);
		rx_pktdump_vsi(&prx->vs_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_vsi_ex(&pktdata);
		rx_pktdump_vsi(&pktdata);
		break;
	case PKT_TYPE_INFOFRAME_AVI:
		rx_pktdump_raw(&prx->avi_info);
		rx_pktdump_avi(&prx->avi_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_avi_ex(&pktdata);
		rx_pktdump_avi(&pktdata);
		break;
	case PKT_TYPE_INFOFRAME_SPD:
		rx_pktdump_raw(&prx->spd_info);
		rx_pktdump_spd(&prx->spd_info);
		break;
	case PKT_TYPE_INFOFRAME_AUD:
		rx_pktdump_raw(&prx->aud_pktinfo);
		rx_pktdump_aud(&prx->aud_pktinfo);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_audif_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
		break;
	case PKT_TYPE_INFOFRAME_MPEGSRC:
		rx_pktdump_raw(&prx->mpegs_info);
		/*rx_pktdump_mpeg(&prx->mpegs_info);*/
		break;
	case PKT_TYPE_INFOFRAME_NVBI:
		rx_pktdump_raw(&prx->ntscvbi_info);
		/*rx_pktdump_ntscvbi(&prx->ntscvbi_info);*/
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_ntscvbi_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
		break;
	case PKT_TYPE_INFOFRAME_DRM:
		rx_pktdump_raw(&prx->drm_info);
		rx_pktdump_drm(&prx->drm_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_drm_ex(&pktdata);
		rx_pktdump_drm(&pktdata);
		break;
	/*other pkt*/
	case PKT_TYPE_ACR:
		rx_pktdump_raw(&prx->acr_info);
		rx_pktdump_acr(&prx->acr_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_acr_ex(&pktdata);
		rx_pktdump_acr(&pktdata);
		break;
	case PKT_TYPE_GCP:
		rx_pktdump_raw(&prx->gcp_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_gcp_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
		break;
	case PKT_TYPE_ACP:
		rx_pktdump_raw(&prx->acp_info);
		break;
	case PKT_TYPE_ISRC1:
		rx_pktdump_raw(&prx->isrc1_info);
		break;
	case PKT_TYPE_ISRC2:
		rx_pktdump_raw(&prx->isrc2_info);
		break;
	case PKT_TYPE_GAMUT_META:
		rx_pktdump_raw(&prx->gameta_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_gmd_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
		break;
	case PKT_TYPE_AUD_META:
		rx_pktdump_raw(&prx->amp_info);
		rx_pr("-------->ex register set >>\n");
		rx_pkt_get_amp_ex(&pktdata);
		rx_pktdump_raw(&pktdata);
		break;

	default:
		rx_pr("warning: not support\n");
		rx_pr("vsi->0x81:Vendor-Specific infoframe\n");
		rx_pr("avi->0x82:Auxiliary video infoframe\n");
		rx_pr("spd->0x83:Source Product Description infoframe\n");
		rx_pr("aud->0x84:Audio infoframe\n");
		rx_pr("mpeg->0x85:MPEG infoframe\n");
		rx_pr("nvbi->0x86:NTSC VBI infoframe\n");
		rx_pr("drm->0x87:DRM infoframe\n");
		rx_pr("acr->0x01:audio clk regeneration\n");
		rx_pr("gcp->0x03\n");
		rx_pr("acp->0x04\n");
		rx_pr("isrc1->0x05\n");
		rx_pr("isrc2->0x06\n");
		rx_pr("gmd->0x0a\n");
		rx_pr("amp->0x0d\n");
		/*rx_pktdump_raw(&prx->dbg_info);*/
		break;
	}

	/*mutex_unlock(&pktbuff_lock);*/
}

uint32_t rx_pkt_type_mapping(enum pkt_type_e pkt_type)
{
	struct pkt_typeregmap_st *ptab = pktmaping;
	uint32_t i = 0;
	uint32_t rt = 0;

	while (ptab[i].pkt_type != K_FLAG_TAB_END) {
		if (ptab[i].pkt_type == pkt_type)
			rt = ptab[i].reg_bit;
		i++;
	}
	return rt;
}

#if 0
/*this function for debug*/
static void rx_pkt_enable(uint32_t type_regbit)
{
	uint32_t data32;

	data32 = hdmirx_rd_dwc(DWC_PDEC_CTRL);
	data32 |= type_regbit;
	hdmirx_wr_dwc(DWC_PDEC_CTRL, data32);

	/*add for enable pd fifo start int for debug*/
	if ((pdec_ists_en & PD_FIFO_START_PASS) == 0) {
		pdec_ists_en |= PD_FIFO_START_PASS;
		hdmirx_irq_enable(true);
	}
}

/*this function for debug*/
static void rx_pkt_disable(uint32_t type_regbit)
{
	uint32_t data32;

	data32 = hdmirx_rd_dwc(DWC_PDEC_CTRL);
	data32 &= ~type_regbit;
	hdmirx_wr_dwc(DWC_PDEC_CTRL, data32);
}
#endif

void rx_pkt_initial(void)
{
	memset(&rxpktsts, 0, sizeof(struct rxpkt_st));

	memset(&rx_pkt.vs_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.avi_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.spd_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.aud_pktinfo, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.mpegs_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.ntscvbi_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.drm_info, 0, sizeof(struct pd_infoframe_s));

	memset(&rx_pkt.acr_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.gcp_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.acp_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.isrc1_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.isrc2_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.gameta_info, 0, sizeof(struct pd_infoframe_s));
	memset(&rx_pkt.amp_info, 0, sizeof(struct pd_infoframe_s));

}

/*please ignore checksum byte*/
void rx_pkt_get_audif_ex(void *pktinfo)
{
	struct aud_infoframe_st *pkt = pktinfo;

	if (pktinfo == NULL) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct aud_infoframe_st));*/

	pkt->pkttype = PKT_TYPE_INFOFRAME_AUD;
	pkt->version =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, MSK(8, 0));
	pkt->length =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, MSK(5, 8));
	pkt->checksum =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, MSK(8, 16));
	pkt->rsd = 0;

	/*get AudioInfo */
	pkt->coding_type =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CODING_TYPE);
	pkt->ch_count =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CHANNEL_COUNT);
	pkt->sample_frq =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_FREQ);
	pkt->sample_size =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_SIZE);
	pkt->fromat =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, AIF_DATA_BYTE_3);
	pkt->ca =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CH_SPEAK_ALLOC);
	pkt->down_mix =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, DWNMIX_INHIBIT);
	pkt->level_shift_value =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, LEVEL_SHIFT_VAL);
	pkt->lfep =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, MSK(2, 8));
}

void rx_pkt_get_acr_ex(void *pktinfo)
{
	struct acr_ptk_st *pkt = pktinfo;
	uint32_t N, CTS;

	if (pktinfo == NULL) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct acr_ptk_st));*/

	pkt->pkttype = PKT_TYPE_ACR;
	pkt->zero0 = 0x0;
	pkt->zero1 = 0x0;

	CTS = hdmirx_rd_dwc(DWC_PDEC_ACR_CTS);
	N = hdmirx_rd_dwc(DWC_PDEC_ACR_N);

	pkt->sbpkt1.SB1_CTS_H = ((CTS >> 16) & 0xf);
	pkt->sbpkt1.SB2_CTS_M = ((CTS >> 8) & 0xff);
	pkt->sbpkt1.SB3_CTS_L = (CTS & 0xff);
	pkt->sbpkt1.SB4_N_H = ((N >> 16) & 0xf);
	pkt->sbpkt1.SB5_N_M = ((N >> 8) & 0xff);
	pkt->sbpkt1.SB6_N_L = (N & 0xff);

	pkt->sbpkt2.SB1_CTS_H = ((CTS >> 16) & 0xf);
	pkt->sbpkt2.SB2_CTS_M = ((CTS >> 8) & 0xff);
	pkt->sbpkt2.SB3_CTS_L = (CTS & 0xff);
	pkt->sbpkt2.SB4_N_H = ((N >> 16) & 0xf);
	pkt->sbpkt2.SB5_N_M = ((N >> 8) & 0xff);
	pkt->sbpkt2.SB6_N_L = (N & 0xff);

	pkt->sbpkt3.SB1_CTS_H = ((CTS >> 16) & 0xf);
	pkt->sbpkt3.SB2_CTS_M = ((CTS >> 8) & 0xff);
	pkt->sbpkt3.SB3_CTS_L = (CTS & 0xff);
	pkt->sbpkt3.SB4_N_H = ((N >> 16) & 0xf);
	pkt->sbpkt3.SB5_N_M = ((N >> 8) & 0xff);
	pkt->sbpkt3.SB6_N_L = (N & 0xff);

	pkt->sbpkt4.SB1_CTS_H = ((CTS >> 16) & 0xf);
	pkt->sbpkt4.SB2_CTS_M = ((CTS >> 8) & 0xff);
	pkt->sbpkt4.SB3_CTS_L = (CTS & 0xff);
	pkt->sbpkt4.SB4_N_H = ((N >> 16) & 0xf);
	pkt->sbpkt4.SB5_N_M = ((N >> 8) & 0xff);
	pkt->sbpkt4.SB6_N_L = (N & 0xff);

}

/*please ignore checksum byte*/
void rx_pkt_get_avi_ex(void *pktinfo)
{
	struct avi_infoframe_st *pkt = pktinfo;

	if (pktinfo == NULL) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct avi_infoframe_st));*/

	pkt->pkttype = PKT_TYPE_INFOFRAME_AVI;
	pkt->version =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, MSK(8, 0));
	pkt->length =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, MSK(5, 8));

	pkt->checksum =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, MSK(8, 16));
	/* AVI parameters */
	pkt->cont.v2v3.vic =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, VID_IDENT_CODE);
	pkt->cont.v2v3.pix_repeat =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, PIX_REP_FACTOR);
	pkt->cont.v2v3.colorindicator =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, VIDEO_FORMAT);
	pkt->cont.v2v3.it_content =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, IT_CONTENT);
	pkt->cont.v2v3.pic_scaling =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, RGB_QUANT_RANGE);
	pkt->cont.v2v3.content_type =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, MSK(2, 28));
	pkt->cont.v2v3.qt_range =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_HB, YUV_QUANT_RANGE);
	pkt->cont.v2v3.activeinfo =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, ACT_INFO_PRESENT);
	pkt->cont.v2v3.barinfo =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, BAR_INFO_VALID);
	pkt->cont.v2v3.scaninfo =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, SCAN_INFO);
	pkt->cont.v2v3.colorimetry =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, COLORIMETRY);
	pkt->cont.v2v3.pic_ration =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, PIC_ASPECT_RATIO);
	pkt->cont.v2v3.fmt_ration =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, ACT_ASPECT_RATIO);
	pkt->cont.v2v3.it_content =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, IT_CONTENT);
	pkt->cont.v2v3.ext_color =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, EXT_COLORIMETRY);
	pkt->cont.v2v3.pic_scaling =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_PB, NON_UNIF_SCALE);

	pkt->line_num_end_topbar =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_TBB, LIN_END_TOP_BAR);
	pkt->line_num_start_btmbar =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_TBB, LIN_ST_BOT_BAR);
	pkt->pix_num_left_bar =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_LRB, PIX_END_LEF_BAR);
	pkt->pix_num_right_bar =
		hdmirx_rd_bits_dwc(DWC_PDEC_AVI_LRB, PIX_ST_RIG_BAR);
}

void rx_pkt_get_vsi_ex(void *pktinfo)
{
	struct vsi_infoframe_st *pkt = pktinfo;
	uint32_t st0;
	uint32_t st1;

	if (pktinfo == NULL) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct vsi_infoframe_st));*/

	/*length = 0x18,PB6-PB24 = 0x00*/
	st0 = hdmirx_rd_dwc(DWC_PDEC_VSI_ST0);
	st1 = hdmirx_rd_dwc(DWC_PDEC_VSI_ST1);

	pkt->pkttype = PKT_TYPE_INFOFRAME_VSI;
	pkt->length = st1 & 0x1f;

	pkt->checksum = (st0 >> 24) & 0xff;
	pkt->ieee = st0 & 0xffffff;

	pkt->ver_st.version = 0;
	pkt->ver_st.chgbit = 0;

	if (rx.chip_id != CHIP_ID_TXHD) {
		pkt->sbpkt.payload.data[0] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0);
		pkt->sbpkt.payload.data[1] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD1);
		pkt->sbpkt.payload.data[2] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD2);
		pkt->sbpkt.payload.data[3] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD3);
		pkt->sbpkt.payload.data[4] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD4);
		pkt->sbpkt.payload.data[5] =
			hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD5);
	}
}

/*return 32 byte data , data struct see register spec*/
void rx_pkt_get_amp_ex(void *pktinfo)
{
	struct pd_infoframe_s *pkt = pktinfo;
	uint32_t HB;

	if (pktinfo == NULL) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct pd_infoframe_s));*/
	if (rx.chip_id != CHIP_ID_TXHD) {
		HB = hdmirx_rd_dwc(DWC_PDEC_AMP_HB);
		pkt->HB = (HB << 8) | PKT_TYPE_AUD_META;
		pkt->PB0 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB0);
		pkt->PB1 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB1);
		pkt->PB2 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB2);
		pkt->PB3 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB3);
		pkt->PB4 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB4);
		pkt->PB5 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB5);
		pkt->PB6 = hdmirx_rd_dwc(DWC_PDEC_AMP_PB6);
	}
}

void rx_pkt_get_gmd_ex(void *pktinfo)
{
	struct gamutmeta_pkt_st *pkt = pktinfo;
	uint32_t HB, PB;

	if (pktinfo == NULL) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct gamutmeta_pkt_st));*/

	pkt->pkttype = PKT_TYPE_GAMUT_META;

	HB = hdmirx_rd_dwc(DWC_PDEC_GMD_HB);
	PB = hdmirx_rd_dwc(DWC_PDEC_GMD_PB);

	/*3:0*/
	pkt->affect_seq_num = (HB & 0xf);
	/*6:4*/
	pkt->gbd_profile = ((HB >> 4) & 0x7);
	/*7*/
	pkt->next_feild = ((HB >> 7) & 0x1);
	/*11:8*/
	pkt->cur_seq_num = ((HB >> 8) & 0xf);
	/*13:12*/
	pkt->pkt_seq = ((HB >> 12) & 0x3);
	/*15*/
	pkt->no_cmt_gbd = ((HB >> 15) & 0x1);

	pkt->sbpkt.p1_profile.gbd_length_l = (PB & 0xff);
	pkt->sbpkt.p1_profile.gbd_length_h = ((PB >> 8) & 0xff);
	pkt->sbpkt.p1_profile.checksum = ((PB >> 16) & 0xff);
}

void rx_pkt_get_gcp_ex(void *pktinfo)
{
	struct gcp_pkt_st *pkt = pktinfo;
	uint32_t gcpAVMUTE;

	if (pktinfo == NULL) {
		rx_pr("pkinfo null\n");
		return;
	}

	/*memset(pkt, 0, sizeof(struct gcp_pkt_st));*/

	gcpAVMUTE = hdmirx_rd_dwc(DWC_PDEC_GCP_AVMUTE);

	pkt->pkttype = PKT_TYPE_GCP;
	pkt->sbpkt.clr_avmute = (gcpAVMUTE & 0x01);
	pkt->sbpkt.set_avmute = ((gcpAVMUTE >> 1) & 0x01);
	pkt->sbpkt.colordepth = ((gcpAVMUTE >> 4) & 0xf);
	pkt->sbpkt.pixelpkgphase = ((gcpAVMUTE >> 8) & 0xf);
	pkt->sbpkt.def_phase = ((gcpAVMUTE >> 12) & 0x01);
}

void rx_pkt_get_drm_ex(void *pktinfo)
{
	struct drm_infoframe_st *drmpkt = pktinfo;

	if (pktinfo == NULL) {
		rx_pr("pkinfo null\n");
		return;
	}

	drmpkt->pkttype = PKT_TYPE_INFOFRAME_DRM;
	if (rx.chip_id != CHIP_ID_TXHD) {
		drmpkt->length = (hdmirx_rd_dwc(DWC_PDEC_DRM_HB) >> 8);
		drmpkt->version = hdmirx_rd_dwc(DWC_PDEC_DRM_HB);

		drmpkt->des_u.payload[0] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD0);
		drmpkt->des_u.payload[1] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD1);
		drmpkt->des_u.payload[2] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD2);
		drmpkt->des_u.payload[3] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD3);
		drmpkt->des_u.payload[4] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD4);
		drmpkt->des_u.payload[5] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD5);
		drmpkt->des_u.payload[6] =
			hdmirx_rd_dwc(DWC_PDEC_DRM_PAYLOAD6);
	}
}

/*return 32 byte data , data struct see register spec*/
void rx_pkt_get_ntscvbi_ex(void *pktinfo)
{
	struct pd_infoframe_s *pkt = pktinfo;

	if (pktinfo == NULL) {
		rx_pr("pkinfo null\n");
		return;
	}

	if (rx.chip_id != CHIP_ID_TXHD) {
		/*byte 0 , 1*/
		pkt->HB = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_HB);
		pkt->PB0 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB0);
		pkt->PB1 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB1);
		pkt->PB2 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB2);
		pkt->PB3 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB3);
		pkt->PB4 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB4);
		pkt->PB5 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB5);
		pkt->PB6 = hdmirx_rd_dwc(DWC_PDEC_NTSCVBI_PB6);
	}
}

uint32_t rx_pkt_chk_attach_vsi(void)
{
	if (rxpktsts.pkt_attach_vsi)
		return 1;
	else
		return 0;
}

void rx_pkt_clr_attach_vsi(void)
{
	rxpktsts.pkt_attach_vsi = 0;
}

uint32_t rx_pkt_chk_attach_drm(void)
{
	if (rxpktsts.pkt_attach_drm)
		return 1;
	else
		return 0;
}

void rx_pkt_clr_attach_drm(void)
{
	rxpktsts.pkt_attach_drm = 0;
}


uint32_t rx_pkt_chk_busy_vsi(void)
{
	if (rxpktsts.pkt_op_flag & PKT_OP_VSI)
		return 1;
	else
		return 0;
}

uint32_t rx_pkt_chk_busy_drm(void)
{
	if (rxpktsts.pkt_op_flag & PKT_OP_DRM)
		return 1;
	else
		return 0;
}

#if 0
static int vsi_handler(struct hdmi_rx_ctrl *ctx)
{
	struct vendor_specific_info_s *vs_info = &rx.vendor_specific_info;

	rx_pkt_getvsinfo(vs_info);
	if (log_level & VSI_LOG)
		rx_pr("dolby vision:%d,dolby_vision_sts %d)\n",
		vs_info->dolby_vision, vs_info->dolby_vision_sts);
	return true;
}
#endif

/*  version2.86 ieee-0x00d046, length 0x1B
 *	pb4 bit[0]: Low_latency
 *	pb4 bit[1]: Dolby_vision_signal
 *	pb5 bit[7]: Backlt_Ctrl_MD_Present
 *	pb5 bit[3:0] | pb6 bit[7:0]: Eff_tmax_PQ
 *
 *	version2.6 ieee-0x000c03,
 *	start: length 0x18
 *	stop: 0x05,0x04
 *
 */
void rx_get_vsi_info(void)
{
	struct vsi_infoframe_st *pkt;
	unsigned int tmp;

	pkt = (struct vsi_infoframe_st *)&(rx_pkt.vs_info);

	rx.vs_info_details._3d_structure = 0;
	rx.vs_info_details._3d_ext_data = 0;
	rx.vs_info_details.low_latency = false;
	rx.vs_info_details.backlt_md_bit = false;
	rx.vs_info_details.dolby_timeout = 0xffff;
	if ((pkt->length == E_DV_LENGTH_27) &&
		(pkt->ieee == 0x00d046)) {
		/* dolby1.5 */
		tmp = pkt->sbpkt.payload.data[0] & _BIT(1);
		rx.vs_info_details.dolby_vision = tmp ? true : false;
		tmp = pkt->sbpkt.payload.data[0] & _BIT(0);
		rx.vs_info_details.low_latency = tmp ? true : false;
		tmp = pkt->sbpkt.payload.data[0] >> 15 & 0x01;
		rx.vs_info_details.backlt_md_bit = tmp ? true : false;
		if (tmp) {
			tmp = (pkt->sbpkt.payload.data[0] >> 16 & 0x0f) |
				(pkt->sbpkt.payload.data[0] & 0xf00);
			rx.vs_info_details.eff_tmax_pq = tmp;
		}
	} else if (pkt->ieee == 0x000c03) {
		/* dobly10 */
		if (pkt->length == E_DV_LENGTH_24) {
			rx.vs_info_details.dolby_vision = true;
			if ((pkt->sbpkt.payload.data[0] & 0xffff) == 0)
				rx.vs_info_details.dolby_timeout =
					dv_nopacket_timeout;
		} else if ((pkt->length == E_DV_LENGTH_5) &&
			(pkt->sbpkt.payload.data[0] & 0xffff)) {
			rx.vs_info_details.dolby_vision = false;
		} else if ((pkt->length == E_DV_LENGTH_4) &&
			((pkt->sbpkt.payload.data[0] & 0xff) == 0)) {
			rx.vs_info_details.dolby_vision = false;
		}
	} else {
		/*3d VSI*/
		if (pkt->sbpkt.vsi_3Dext.vdfmt == VSI_FORMAT_3D_FORMAT) {
			rx.vs_info_details._3d_structure =
				pkt->sbpkt.vsi_3Dext.threeD_st;
			rx.vs_info_details._3d_ext_data =
				pkt->sbpkt.vsi_3Dext.threeD_ex;
			if (log_level & VSI_LOG)
				rx_pr("struct_3d:%d, struct_3d_ext:%d\n",
					pkt->sbpkt.vsi_3Dext.threeD_st,
					pkt->sbpkt.vsi_3Dext.threeD_ex);
		}
		rx.vs_info_details.dolby_vision = false;
	}
}

#if 0
static int rx_pkt_get_drminfo(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	struct drm_infoframe_st *drmpkt;

	if (rx.state != FSM_SIG_READY)
		return 0;
	/*
	 * if (hdr_enable == false)
	 *		return 0;
	 */
	if (ctx == 0)
		return -EINVAL;

	/* waiting, before send the hdr data to post modules */
	if (rx.hdr_info.hdr_state != HDR_STATE_NULL)
		return -EBUSY;

	/*rx_pkt_get_drm_ex(&drmpkt);*/
	drmpkt = (struct drm_infoframe_st *)&(rx.drm_info);

	rx.hdr_info.hdr_state = HDR_STATE_GET;
	rx.hdr_info.hdr_data.length = drmpkt->length;
	rx.hdr_info.hdr_data.eotf = drmpkt->des_u.tp1.eotf;
	rx.hdr_info.hdr_data.metadata_id = drmpkt->des_u.tp1.meta_des_id;
	rx.hdr_info.hdr_data.primaries[0].x = drmpkt->des_u.tp1.dis_pri_x0;
	rx.hdr_info.hdr_data.primaries[0].y = drmpkt->des_u.tp1.dis_pri_y0;
	rx.hdr_info.hdr_data.primaries[1].x = drmpkt->des_u.tp1.dis_pri_x1;
	rx.hdr_info.hdr_data.primaries[1].y = drmpkt->des_u.tp1.dis_pri_y1;
	rx.hdr_info.hdr_data.primaries[2].x = drmpkt->des_u.tp1.dis_pri_x2;
	rx.hdr_info.hdr_data.primaries[2].y = drmpkt->des_u.tp1.dis_pri_y2;
	rx.hdr_info.hdr_data.white_points.x = drmpkt->des_u.tp1.white_points_x;
	rx.hdr_info.hdr_data.white_points.y = drmpkt->des_u.tp1.white_points_y;
	rx.hdr_info.hdr_data.master_lum.x = drmpkt->des_u.tp1.max_dislum;
	rx.hdr_info.hdr_data.master_lum.y = drmpkt->des_u.tp1.min_dislum;
	rx.hdr_info.hdr_data.mcll = drmpkt->des_u.tp1.max_light_lvl;
	rx.hdr_info.hdr_data.mfall = drmpkt->des_u.tp1.max_fa_light_lvl;

	rx.hdr_info.hdr_state = HDR_STATE_SET;

	return error;
}
#endif

void rx_pkt_buffclear(enum pkt_type_e pkt_type)
{
	struct packet_info_s *prx = &rx_pkt;
	void *pktinfo;

	if (pkt_type == PKT_TYPE_INFOFRAME_VSI)
		pktinfo = &prx->vs_info;
	else if (pkt_type == PKT_TYPE_INFOFRAME_AVI)
		pktinfo = &prx->avi_info;
	else if (pkt_type == PKT_TYPE_INFOFRAME_SPD)
		pktinfo = &prx->spd_info;
	else if (pkt_type == PKT_TYPE_INFOFRAME_AUD)
		pktinfo = &prx->aud_pktinfo;
	else if (pkt_type == PKT_TYPE_INFOFRAME_MPEGSRC)
		pktinfo = &prx->mpegs_info;
	else if (pkt_type == PKT_TYPE_INFOFRAME_NVBI)
		pktinfo = &prx->ntscvbi_info;
	else if (pkt_type == PKT_TYPE_INFOFRAME_DRM)
		pktinfo = &prx->drm_info;
	else if (pkt_type == PKT_TYPE_ACR)
		pktinfo = &prx->acr_info;
	else if (pkt_type == PKT_TYPE_GCP)
		pktinfo = &prx->gcp_info;
	else if (pkt_type == PKT_TYPE_ACP)
		pktinfo = &prx->acp_info;
	else if (pkt_type == PKT_TYPE_ISRC1)
		pktinfo = &prx->isrc1_info;
	else if (pkt_type == PKT_TYPE_ISRC2)
		pktinfo = &prx->isrc2_info;
	else if (pkt_type == PKT_TYPE_GAMUT_META)
		pktinfo = &prx->gameta_info;
	else if (pkt_type == PKT_TYPE_AUD_META)
		pktinfo = &prx->amp_info;
	else {
		rx_pr("err type:0x%x\n", pkt_type);
		return;
	}
	memset(pktinfo, 0, sizeof(struct pd_infoframe_s));
}

void rx_pkt_content_chk_en(uint32_t enable)
{
	rx_pr("content chk:%d\n", enable);
	if (enable) {
		if (pkt_testbuff == NULL)
			pkt_testbuff = kmalloc(sizeof(struct st_pkt_test_buff),
						GFP_KERNEL);
		if (pkt_testbuff) {
			memset(pkt_testbuff, 0,
				sizeof(struct st_pkt_test_buff));
			rxpktsts.pkt_chk_flg = true;
		} else
			rx_pr("kmalloc fail for pkt chk\n");
	} else {
		if (rxpktsts.pkt_chk_flg)
			kfree(pkt_testbuff);
		rxpktsts.pkt_chk_flg = false;
		pkt_testbuff = NULL;
	}
}

/*
 * read pkt data from pkt fifo or external register set
 */
void rx_pkt_set_fifo_pri(uint32_t pri)
{
	gpkt_fifo_pri = pri;
	rx_pr("gpkt_fifo_pri=%d\n", pri);
}

uint32_t rx_pkt_get_fifo_pri(void)
{
	return gpkt_fifo_pri;
}

void rx_pkt_check_content(void)
{
	struct packet_info_s *cur_pkt = &rx_pkt;
	struct st_pkt_test_buff *pre_pkt;
	struct rxpkt_st *pktsts;
	static uint32_t acr_pkt_cnt;
	static uint32_t ex_acr_pkt_cnt;
	struct pd_infoframe_s pktdata;

	pre_pkt = pkt_testbuff;
	pktsts = &rxpktsts;

	if (pre_pkt == NULL)
		return;

	if (rxpktsts.pkt_chk_flg) {
		/*compare vsi*/
		if (pktsts->pkt_cnt_vsi) {
			if (memcmp((char *)&pre_pkt->vs_info,
				(char *)&cur_pkt->vs_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" vsi chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&(pre_pkt->vs_info));
				/*dump current data*/
				rx_pktdump_raw(&(cur_pkt->vs_info));
				/*save current*/
				memcpy(&(pre_pkt->vs_info), &(cur_pkt->vs_info),
						sizeof(struct pd_infoframe_s));
			}
		}

		/*compare avi*/
		if (pktsts->pkt_cnt_avi) {
			if (memcmp((char *)&pre_pkt->avi_info,
				(char *)&cur_pkt->avi_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" avi chg\n");
				rx_pktdump_raw(&(pre_pkt->avi_info));
				rx_pktdump_raw(&(cur_pkt->avi_info));
				memcpy(&(pre_pkt->avi_info),
						&(cur_pkt->avi_info),
						sizeof(struct pd_infoframe_s));
			}
		}

		/*compare spd*/
		if (pktsts->pkt_cnt_spd) {
			if (memcmp((char *)&pre_pkt->spd_info,
				(char *)&cur_pkt->spd_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" spd chg\n");
				rx_pktdump_raw(&(pre_pkt->spd_info));
				rx_pktdump_raw(&(cur_pkt->spd_info));
				memcpy(&(pre_pkt->spd_info),
						&(cur_pkt->spd_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare aud_pktinfo*/
		if (pktsts->pkt_cnt_audif) {
			if (memcmp((char *)&pre_pkt->aud_pktinfo,
				(char *)&cur_pkt->aud_pktinfo,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" audif chg\n");
				rx_pktdump_raw(&(pre_pkt->aud_pktinfo));
				rx_pktdump_raw(&(cur_pkt->aud_pktinfo));
				memcpy(&(pre_pkt->aud_pktinfo),
						&(cur_pkt->aud_pktinfo),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare mpegs_info*/
		if (pktsts->pkt_cnt_mpeg) {
			if (memcmp((char *)&pre_pkt->mpegs_info,
				(char *)&cur_pkt->mpegs_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pktdump_raw(&(pre_pkt->mpegs_info));
				rx_pktdump_raw(&(cur_pkt->mpegs_info));
				memcpy(&(pre_pkt->mpegs_info),
						&(cur_pkt->mpegs_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare ntscvbi_info*/
		if (pktsts->pkt_cnt_nvbi) {
			if (memcmp((char *)&pre_pkt->ntscvbi_info,
				(char *)&cur_pkt->ntscvbi_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" nvbi chg\n");
				rx_pktdump_raw(&(pre_pkt->ntscvbi_info));
				rx_pktdump_raw(&(cur_pkt->ntscvbi_info));
				memcpy(&(pre_pkt->ntscvbi_info),
						&(cur_pkt->ntscvbi_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare drm_info*/
		if (pktsts->pkt_cnt_drm) {
			if (memcmp((char *)&pre_pkt->drm_info,
				(char *)&cur_pkt->drm_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" drm chg\n");
				rx_pktdump_raw(&(pre_pkt->drm_info));
				rx_pktdump_raw(&(cur_pkt->drm_info));
				memcpy(&(pre_pkt->drm_info),
						&(cur_pkt->drm_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare acr_info*/
		if (pktsts->pkt_cnt_acr) {
			if (memcmp((char *)&pre_pkt->acr_info,
				(char *)&cur_pkt->acr_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				if (acr_pkt_cnt++ > 100) {
					acr_pkt_cnt = 0;
					rx_pr(" acr chg\n");
					rx_pktdump_acr(&(cur_pkt->acr_info));
				}
				/*save current*/
				memcpy(&(pre_pkt->acr_info),
						&(cur_pkt->acr_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare gcp_info*/
		if (pktsts->pkt_cnt_gcp) {
			if (memcmp((char *)&pre_pkt->gcp_info,
				(char *)&cur_pkt->gcp_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" gcp chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&(pre_pkt->gcp_info));
				/*dump current data*/
				rx_pktdump_raw(&(cur_pkt->gcp_info));
				/*save current*/
				memcpy(&(pre_pkt->gcp_info),
						&(cur_pkt->gcp_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare acp_info*/
		if (pktsts->pkt_cnt_acp) {
			if (memcmp((char *)&pre_pkt->acp_info,
				(char *)&cur_pkt->acp_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" acp chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&(pre_pkt->acp_info));
				/*dump current data*/
				rx_pktdump_raw(&(cur_pkt->acp_info));
				/*save current*/
				memcpy(&(pre_pkt->acp_info),
						&(cur_pkt->acp_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare isrc1_info*/
		if (pktsts->pkt_cnt_isrc1) {
			if (memcmp((char *)&pre_pkt->isrc1_info,
				(char *)&cur_pkt->isrc1_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" isrc2 chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&(pre_pkt->isrc1_info));
				/*dump current data*/
				rx_pktdump_raw(&(cur_pkt->isrc1_info));
				/*save current*/
				memcpy(&(pre_pkt->isrc1_info),
						&(cur_pkt->isrc1_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare isrc2_info*/
		if (pktsts->pkt_cnt_isrc2) {
			if (memcmp((char *)&pre_pkt->isrc2_info,
				(char *)&cur_pkt->isrc2_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" isrc1 chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&(pre_pkt->isrc2_info));
				/*dump current data*/
				rx_pktdump_raw(&(cur_pkt->isrc2_info));
				/*save current*/
				memcpy(&(pre_pkt->isrc2_info),
						&(cur_pkt->isrc2_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare gameta_info*/
		if (pktsts->pkt_cnt_gameta) {
			if (memcmp((char *)&pre_pkt->gameta_info,
				(char *)&cur_pkt->gameta_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" gmd chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&(pre_pkt->gameta_info));
				/*dump current data*/
				rx_pktdump_raw(&(cur_pkt->gameta_info));
				/*save current*/
				memcpy(&(pre_pkt->gameta_info),
						&(cur_pkt->gameta_info),
						sizeof(struct pd_infoframe_s));
			}
		}
		/*compare amp_info*/
		if (pktsts->pkt_cnt_amp) {
			if (memcmp((char *)&pre_pkt->amp_info,
				(char *)&cur_pkt->amp_info,
				sizeof(struct pd_infoframe_s)) != 0) {
				rx_pr(" amp chg\n");
				/*dump pre data*/
				rx_pktdump_raw(&(pre_pkt->amp_info));
				/*dump current data*/
				rx_pktdump_raw(&(cur_pkt->amp_info));
				/*save current*/
				memcpy(&(pre_pkt->amp_info),
						&(cur_pkt->amp_info),
						sizeof(struct pd_infoframe_s));
			}
		}

		rx_pkt_get_audif_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_audif,
			(char *)&pktdata,
			sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_audif chg\n");
			memcpy(&(pre_pkt->ex_audif), &pktdata,
					sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_acr_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_acr,
			(char *)&pktdata,
			sizeof(struct pd_infoframe_s)) != 0) {
			if (ex_acr_pkt_cnt++ > 100) {
				ex_acr_pkt_cnt = 0;
				rx_pr(" ex_acr chg\n");
			}
			memcpy(&(pre_pkt->ex_acr), &pktdata,
				sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_avi_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_avi,
			(char *)&pktdata,
			sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_avi chg\n");
			memcpy(&(pre_pkt->ex_avi), &pktdata,
					sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_vsi_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_vsi,
			(char *)&pktdata,
			sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_vsi chg\n");
			memcpy(&(pre_pkt->ex_vsi), &pktdata,
					sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_amp_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_amp,
			(char *)&pktdata,
			sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_amp chg\n");
			memcpy(&(pre_pkt->ex_amp), &pktdata,
					sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_gmd_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_gmd,
			(char *)&pktdata,
			sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_gmd chg\n");
			memcpy(&(pre_pkt->ex_gmd), &pktdata,
					sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_gcp_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_gcp,
			(char *)&pktdata,
			sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_gcp chg\n");
			memcpy(&(pre_pkt->ex_gcp), &pktdata,
					sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_drm_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_drm,
			(char *)&pktdata,
			sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_drm chg\n");
			memcpy(&(pre_pkt->ex_drm), &pktdata,
					sizeof(struct pd_infoframe_s));
		}

		rx_pkt_get_ntscvbi_ex(&pktdata);
		if (memcmp((char *)&pre_pkt->ex_nvbi,
			(char *)&pktdata,
			sizeof(struct pd_infoframe_s)) != 0) {
			rx_pr(" ex_nvbi chg\n");
			memcpy(&(pre_pkt->ex_nvbi), &pktdata,
					sizeof(struct pd_infoframe_s));
		}
	}
}

int rx_pkt_fifodecode(struct packet_info_s *prx,
				union infoframe_u *pktdata,
				struct rxpkt_st *pktsts)
{
	switch (pktdata->raw_infoframe.pkttype) {
	case PKT_TYPE_INFOFRAME_VSI:
		pktsts->pkt_attach_vsi++;
		pktsts->pkt_cnt_vsi++;
		pktsts->pkt_op_flag |= PKT_OP_VSI;
		/*memset(&prx->vs_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->vs_info, pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_VSI;
		break;
	case PKT_TYPE_INFOFRAME_AVI:
		pktsts->pkt_cnt_avi++;
		pktsts->pkt_op_flag |= PKT_OP_AVI;
		/*memset(&prx->avi_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->avi_info, pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_AVI;
		break;
	case PKT_TYPE_INFOFRAME_SPD:
		pktsts->pkt_cnt_spd++;
		pktsts->pkt_op_flag |= PKT_OP_SPD;
		/*memset(&prx->spd_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->spd_info, pktdata,
					sizeof(struct spd_infoframe_st));
		pktsts->pkt_op_flag &= ~PKT_OP_SPD;
		break;
	case PKT_TYPE_INFOFRAME_AUD:
		pktsts->pkt_cnt_audif++;
		pktsts->pkt_op_flag |= PKT_OP_AIF;
		/*memset(&prx->aud_pktinfo, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->aud_pktinfo,
				pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_AIF;
		break;
	case PKT_TYPE_INFOFRAME_MPEGSRC:
		pktsts->pkt_cnt_mpeg++;
		pktsts->pkt_op_flag |= PKT_OP_MPEGS;
		/*memset(&prx->mpegs_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->mpegs_info, pktdata,
					sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_MPEGS;
		break;
	case PKT_TYPE_INFOFRAME_NVBI:
		pktsts->pkt_cnt_nvbi++;
		pktsts->pkt_op_flag |= PKT_OP_NVBI;
		/* memset(&prx->ntscvbi_info, 0, */
			/* sizeof(struct pd_infoframe_s)); */
		memcpy(&prx->ntscvbi_info, pktdata,
					sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_NVBI;
		break;
	case PKT_TYPE_INFOFRAME_DRM:
		pktsts->pkt_attach_drm++;
		pktsts->pkt_cnt_drm++;
		pktsts->pkt_op_flag |= PKT_OP_DRM;
		/*memset(&prx->drm_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->drm_info, pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_DRM;
		break;

	case PKT_TYPE_ACR:
		pktsts->pkt_cnt_acr++;
		pktsts->pkt_op_flag |= PKT_OP_ACR;
		/*memset(&prx->acr_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->acr_info,
			pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_ACR;
		break;
	case PKT_TYPE_GCP:
		pktsts->pkt_cnt_gcp++;
		pktsts->pkt_op_flag |= PKT_OP_GCP;
		/*memset(&prx->gcp_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->gcp_info,
			pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_GCP;
		break;
	case PKT_TYPE_ACP:
		pktsts->pkt_cnt_acp++;
		pktsts->pkt_op_flag |= PKT_OP_ACP;
		/*memset(&prx->acp_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->acp_info,
			pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_ACP;
		break;
	case PKT_TYPE_ISRC1:
		pktsts->pkt_cnt_isrc1++;
		pktsts->pkt_op_flag |= PKT_OP_ISRC1;
		/*memset(&prx->isrc1_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->isrc1_info,
			pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_ISRC1;
		break;
	case PKT_TYPE_ISRC2:
		pktsts->pkt_cnt_isrc2++;
		pktsts->pkt_op_flag |= PKT_OP_ISRC2;
		/*memset(&prx->isrc2_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->isrc2_info,
			pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_ISRC2;
		break;
	case PKT_TYPE_GAMUT_META:
		pktsts->pkt_cnt_gameta++;
		pktsts->pkt_op_flag |= PKT_OP_GMD;
		/*memset(&prx->gameta_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->gameta_info,
			pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_GMD;
		break;
	case PKT_TYPE_AUD_META:
		pktsts->pkt_cnt_amp++;
		pktsts->pkt_op_flag |= PKT_OP_AMP;
		/*memset(&prx->amp_info, 0, sizeof(struct pd_infoframe_s));*/
		memcpy(&prx->amp_info,
			pktdata, sizeof(struct pd_infoframe_s));
		pktsts->pkt_op_flag &= ~PKT_OP_AMP;
		break;
	default:
		break;
	}

	return 0;
}


int rx_pkt_handler(enum pkt_decode_type pkt_int_src)
{
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t pkt_num = 0;
	uint32_t pkt_cnt = 0;
	union infoframe_u *pktdata;
	struct packet_info_s *prx = &rx_pkt;
	/*uint32_t t1, t2;*/

	if (pkt_int_src == PKT_BUFF_SET_FIFO) {
		/*from pkt fifo*/
		if (!pd_fifo_buf)
			return 0;

		/*t1 = sched_clock();*/
		/*for recode interrupt cnt*/
		rxpktsts.fifo_Int_cnt++;

		/*ps:read 10 packet from fifo cost time < less than 200 us */
		if (hdmirx_rd_dwc(DWC_PDEC_STS) & PD_TH_START) {
readpkt:
			/*how many packet number need read from fifo*/
			/* If software tries to read more packets from the */
			/* FIFO than what is stored already, an underflow */
			/* occurs. */
			pkt_num = hdmirx_rd_dwc(DWC_PDEC_FIFO_STS1) >> 3;
			rxpktsts.fifo_pkt_num = pkt_num;

			for (pkt_cnt = 0; pkt_cnt < pkt_num; pkt_cnt++) {
				/*read one pkt from fifo*/
				for (j = 0; j < K_ONEPKT_BUFF_SIZE; j++) {
					/*8 word per packet size*/
					pd_fifo_buf[i+j] =
					hdmirx_rd_dwc(DWC_PDEC_FIFO_DATA);
				}

				if (log_level & PACKET_LOG)
					rx_pr("PD[%d]=%x\n", i, pd_fifo_buf[i]);

				pktdata = (union infoframe_u *)&pd_fifo_buf[i];
				/*mutex_lock(&pktbuff_lock);*/
				/*pkt decode*/
				rx_pkt_fifodecode(prx, pktdata, &rxpktsts);
				/*mutex_unlock(&pktbuff_lock);*/
				i += 8;
				if (i > (PFIFO_SIZE - 8))
					i = 0;
			}
			/*while read from buff, other more packets attach*/
			pkt_num = hdmirx_rd_dwc(DWC_PDEC_FIFO_STS1) >> 3;
			if (pkt_num > K_PKT_REREAD_SIZE)
				goto readpkt;
		}
	} else if (pkt_int_src == PKT_BUFF_SET_VSI) {
		rxpktsts.pkt_attach_vsi++;
		rxpktsts.pkt_op_flag |= PKT_OP_VSI;
		rx_pkt_get_vsi_ex(&prx->vs_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_VSI;
		rxpktsts.pkt_cnt_vsi_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_DRM) {
		rxpktsts.pkt_attach_drm++;
		rxpktsts.pkt_op_flag |= PKT_OP_DRM;
		rx_pkt_get_drm_ex(&prx->drm_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_DRM;
		rxpktsts.pkt_cnt_drm_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_GMD) {
		rxpktsts.pkt_op_flag |= PKT_OP_GMD;
		rx_pkt_get_gmd_ex(&prx->gameta_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_GMD;
		rxpktsts.pkt_cnt_gmd_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_AIF) {
		rxpktsts.pkt_op_flag |= PKT_OP_AIF;
		rx_pkt_get_audif_ex(&prx->aud_pktinfo);
		rxpktsts.pkt_op_flag &= ~PKT_OP_AIF;
		rxpktsts.pkt_cnt_aif_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_AVI) {
		rxpktsts.pkt_op_flag |= PKT_OP_AVI;
		rx_pkt_get_avi_ex(&prx->avi_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_AVI;
		rxpktsts.pkt_cnt_avi_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_ACR) {
		rxpktsts.pkt_op_flag |= PKT_OP_ACR;
		rx_pkt_get_acr_ex(&prx->acr_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_ACR;
		rxpktsts.pkt_cnt_acr_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_GCP) {
		rxpktsts.pkt_op_flag |= PKT_OP_GCP;
		rx_pkt_get_gcp_ex(&prx->gcp_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_GCP;
		rxpktsts.pkt_cnt_gcp_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_AMP) {
		rxpktsts.pkt_op_flag |= PKT_OP_AMP;
		rx_pkt_get_amp_ex(&prx->amp_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_AMP;
		rxpktsts.pkt_cnt_amp_ex++;
	} else if (pkt_int_src == PKT_BUFF_SET_NVBI) {
		rxpktsts.pkt_op_flag |= PKT_OP_NVBI;
		rx_pkt_get_ntscvbi_ex(&prx->ntscvbi_info);
		rxpktsts.pkt_op_flag &= ~PKT_OP_NVBI;
		rxpktsts.pkt_cnt_nvbi_ex++;
	}

	/*t2 = sched_clock();*/
	/*
	 * timerbuff[timerbuff_idx] = pkt_num;
	 * if (timerbuff_idx++ >= 50)
	 *	timerbuff_idx = 0;
	 */
	return 0;
}

