// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek HDMI CEC Driver
 *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <linux/wait.h>
#include <linux/kthread.h>

#include "mtk_hdmi_common.h"
#include "mtk_mt8195_hdmi_regs.h"
#include "mtk_mt8195_hdmi_ddc.h"
#include "mtk_hdmi_hdcp.h"
#include "mtk_hdmi_ca.h"

#define REPEAT_CHECK_AUTHHDCP_VALUE 25
#define HDMI_HDCP_INTERVAL 10
#define SRM_SUPPORT

#define SV_OK       ((unsigned char)(0))
#define SV_FAIL      ((unsigned char)(-1))

#define HDMI_HDCP_LOG(fmt, arg...) \
	pr_debug("[HDMI][HDCP] %s,%d "fmt, __func__, __LINE__, ##arg) \

#define HDMI_HDCP_FUNC() \
	pr_debug("[HDMI][HDCP] %s\n", __func__) \

static unsigned int mtk_hdmi_hdcp_get_share_info(struct mtk_hdmi *hdmi, unsigned int index)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	return hdcp->share_info[index];
}

static void mtk_hdmi_hdcp_set_share_info(struct mtk_hdmi *hdmi, unsigned int index,
	unsigned int value)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	hdcp->share_info[index] = value;
}

static inline void mtk_hdmi_hdcp_set_delay_time(struct mtk_hdmi *hdmi, unsigned int delay_time)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	hdcp->hdcp_delay_time = delay_time;
}

static bool mtk_hdmi_hdcp_check_ctrl_timeout(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->hdcp_delay_time <= 0)
		return true;
	return false;
}

void mtk_hdmi_hdcp_enable_mute(struct mtk_hdmi *hdmi, bool enable)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	HDMI_HDCP_LOG("enable:%d\n", enable);

	if (enable) {
		mtk_hdmi_hdcp_ca_write_ctrl(hdmi, 0x88880000, 0x5555aaaa);
		hdcp->enable_mute_for_hdcp_flag = 1;
	} else {
		mtk_hdmi_hdcp_ca_write_ctrl(hdmi, 0x88880000, 0xaaaa5555);
		hdcp->enable_mute_for_hdcp_flag = 0;
	}
}

void mtk_hdmi_hdcp_notify_hpd_to_hdcp(struct mtk_hdmi *hdmi)
{
	HDMI_HDCP_LOG("hpd:%d\n", hdmi->hpd);

	if (hdmi->hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON)
		mtk_hdmi_hdcp_ca_write_ctrl(hdmi, 0x88880000, 0xaaaa0005);
	else
		mtk_hdmi_hdcp_ca_write_ctrl(hdmi, 0x88880000, 0xaaaa0006);
}

void mtk_hdmi_hdcp_clean_auth_fail_int(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x00020000);
	udelay(1);
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x00000000);
	HDMI_HDCP_LOG("HDCP2X_STATUS_0 = 0x%08x\n",
		mtk_hdmi_read(hdmi, HDCP2X_STATUS_0));
	hdcp->hdcp_status = SV_FAIL;
}

void mtk_hdmi_hdcp2x_clear_int(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0xfffffff0);
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0xffffffff);
	udelay(1);
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x0);
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0x0);
}

void mtk_hdmi_hdcp1x_reset(struct mtk_hdmi *hdmi)
{
	/* Reset hdcp 1.x */
	/* SOFT_HDCP_1P4_RST, SOFT_HDCP_1P4_RST); */
	mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, SOFT_HDCP_1P4_RST, SOFT_HDCP_1P4_RST);
	udelay(100);
	/* SOFT_HDCP_1P4_NOR, SOFT_HDCP_1P4_RST); */
	mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, SOFT_HDCP_1P4_NOR, SOFT_HDCP_1P4_RST);

	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 0, AN_STOP);
	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 0, HDCP1X_ENC_EN);
}

void mtk_hdmi_hdcp_set_state(struct mtk_hdmi *hdmi, enum HDCP_CTRL_STATE_T e_state)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	HDMI_HDCP_LOG("%s e_state = %d\n", __func__, e_state);
	hdcp->hdcp_ctrl_state = e_state;
}

void mtk_hdmi_hdcp_ddc_hw_poll(struct mtk_hdmi *hdmi, bool _bhw)
{
	if (_bhw == true)
		mtk_hdmi_mask(hdmi, HDCP2X_POL_CTRL, 0, HDCP2X_DIS_POLL_EN);
	else
		mtk_hdmi_mask(hdmi, HDCP2X_POL_CTRL, HDCP2X_DIS_POLL_EN,
		HDCP2X_DIS_POLL_EN);
}

void mtk_hdmi_hdcp2x_reset(struct mtk_hdmi *hdmi)
{
	/* Reset hdcp 2.x */
	HDMI_HDCP_LOG("HDCP2X_DDCM_STATUS(0xc68)=0x%x\n", mtk_hdmi_read(hdmi, HDCP2X_DDCM_STATUS));

	if (mtk_hdmi_read(hdmi, HDCP2X_CTRL_0) &
		HDCP2X_ENCRYPT_EN) {
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, 0, HDCP2X_ENCRYPT_EN);
		mdelay(50);
	}

	mtk_hdmi_hdcp_ddc_hw_poll(hdmi, false);
	/* SOFT_HDCP_RST, SOFT_HDCP_RST); */
	mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, SOFT_HDCP_RST, SOFT_HDCP_RST);
	/* SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST); */
	mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST);
	udelay(1);
	/* SOFT_HDCP_NOR, SOFT_HDCP_RST); */
	mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, SOFT_HDCP_NOR, SOFT_HDCP_RST);
	/* SOFT_HDCP_CORE_NOR, SOFT_HDCP_CORE_RST); */
	mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, SOFT_HDCP_CORE_NOR, SOFT_HDCP_CORE_RST);
}

void mtk_hdmi_hdcp_reset(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->enable_hdcp == false)
		return;
	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}
	mtk_hdmi_hdcp1x_reset(hdmi);

	mutex_lock(&ddc->mtx);
	hdmi_ddc_request(ddc, 3);
	mtk_hdmi_hdcp2x_reset(hdmi);
	mutex_unlock(&ddc->mtx);

	mtk_hdmi_hdcp_set_state(hdmi, HDCP_RECEIVER_NOT_READY);
	hdcp->re_check_bstatus_count = 0;
}

void mtk_hdmi_hdcp1x_init_auth(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_RES_CHG_OK_TIMEOUT);
	mtk_hdmi_hdcp_set_state(hdmi, HDCP_WAIT_RES_CHG_OK);
	hdcp->re_check_bstatus_count = 0;
}

void mtk_hdmi_hdcp_init_auth(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	HDMI_HDCP_LOG("%s, HDCP2X_DDCM_STATUS(0xc68)=0x%x, %lums\n", __func__,
		      mtk_hdmi_read(hdmi, HDCP2X_DDCM_STATUS), jiffies);

	hdcp->hdcp_err_0x30_count = 0;
	hdcp->hdcp_err_0x30_flag = 0;

	mtk_hdmi_hdcp_read_version(hdmi);
	mtk_hdmi_hdcp_reset(hdmi);

	if (hdcp->hdcp_2x_support == true) {
		if (hdcp->fw_is_loaded == false)
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_LOADFW_TIMEOUT);
		else
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_RES_CHG_OK_TIMEOUT);
		mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_WAIT_RES_CHG_OK);
	} else {
		mtk_hdmi_hdcp1x_init_auth(hdmi);
	}
}

void mtk_hdmi_hdcp_repeater_on_off(struct mtk_hdmi *hdmi, unsigned char is_rep)
{
	if (is_rep == true)
		mtk_hdmi_mask(hdmi, HDCP1X_CTRL, RX_RPTR, RX_RPTR);
	else
		mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 0, RX_RPTR);
}

void mtk_hdmi_hdcp_stop_an(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, AN_STOP, AN_STOP);
}

void mtk_hdmi_hdcp_read_an(struct mtk_hdmi *hdmi, unsigned char *an_value)
{
	unsigned char index;

	an_value[0] = mtk_hdmi_read(hdmi, HDCP1X_AN_0) & 0xff;
	an_value[1] = (mtk_hdmi_read(hdmi, HDCP1X_AN_0) & 0xff00) >> 8;
	an_value[2] = (mtk_hdmi_read(hdmi, HDCP1X_AN_0) & 0xff0000) >> 16;
	an_value[3] = (mtk_hdmi_read(hdmi, HDCP1X_AN_0) & 0xff000000) >> 24;
	an_value[4] = mtk_hdmi_read(hdmi, HDCP1X_AN_1) & 0xff;
	an_value[5] = (mtk_hdmi_read(hdmi, HDCP1X_AN_1) & 0xff00) >> 8;
	an_value[6] = (mtk_hdmi_read(hdmi, HDCP1X_AN_1) & 0xff0000) >> 16;
	an_value[7] = (mtk_hdmi_read(hdmi, HDCP1X_AN_1) & 0xff000000) >> 24;
	for (index = 0; index < 8; index++)
		HDMI_HDCP_LOG("[1x]an_value[%d] =0x%02x\n",
		index, an_value[index]);
}

void mtk_hdmi_hdcp_send_an(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	unsigned char hdcp_buf[HDCP_AN_COUNT];

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

	/* Step 1: issue command to general a new An value */
	/* (1) read the value first */
	/* (2) set An control as stop to general a An first */
	mtk_hdmi_hdcp_stop_an(hdmi);

	/* Step 2: Read An from Transmitter */
	mtk_hdmi_hdcp_read_an(hdmi, hdcp_buf);
	/* Step 3: Send An to Receiver */
	fg_ddc_data_write(ddc, RX_ID, RX_REG_HDCP_AN, HDCP_AN_COUNT, hdcp_buf);
}

void mtk_hdmi_hdcp_write_bksv_to_tx(struct mtk_hdmi *hdmi, unsigned char *bksv)
{
	unsigned int temp;

	HDMI_HDCP_LOG("bksv 0x%x; 0x%x; 0x%x; 0x%x; 0x%x\n",
		bksv[0], bksv[1], bksv[2],
		      bksv[3], bksv[4]);
	temp = (((bksv[3]) & 0xff) << 24) +
		(((bksv[2]) & 0xff) << 16) +
	    (((bksv[1]) & 0xff) << 8) +
	    (bksv[0] & 0xff);
	mtk_hdmi_write(hdmi, HDCP1X_BKSV_0, temp);
	udelay(10);
	mtk_hdmi_write(hdmi, HDCP1X_BKSV_1, bksv[4]);

	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 1 << 0, 1 << 0);
	udelay(100);
	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 0 << 0, 1 << 0);

	HDMI_HDCP_LOG("[1x]HDCP1X_BKSV_0(0xcb0)=0x%08x\n",
		mtk_hdmi_read(hdmi, HDCP1X_BKSV_0));
	HDMI_HDCP_LOG("[1x]HDCP1X_BKSV_1(0xcb4)=0x%08x\n",
		mtk_hdmi_read(hdmi, HDCP1X_BKSV_1));
}

bool mtk_hdmi_hdcp_is_repeater(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	return (hdcp->downstream_is_repeater == true);
}

#ifdef SRM_SUPPORT
void mtk_hdmi_hdcp_compare_srm(struct mtk_hdmi *hdmi)
{
	unsigned int ksv_inx = 0, vrl_index = 0;

	unsigned char *ptr_srm, nom_of_device = 0;
	unsigned char ksv_sink_index = 0, index = 0, dw_index = 0;
	unsigned int index_tmp;
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->srm_info.id != 0x80)
		return;

	HDMI_HDCP_LOG("[HDCP]SRM Count = %d ",
		hdcp->srm_info.vrl_len_in_dram);
	HDMI_HDCP_LOG("[HDCP]Key=%x, %x, %x, %x, %x",
		hdcp->hdcp_bksv[0], hdcp->hdcp_bksv[1],
		hdcp->hdcp_bksv[2], hdcp->hdcp_bksv[3], hdcp->hdcp_bksv[4]);

	mtk_hdmi_hdcp_set_share_info(hdmi, SI_DVD_HDCP_REVOCATION_RESULT, REVOCATION_NOT_CHK);

	vrl_index = 0;
	ptr_srm = &hdcp->srm_buff[8];
	while (hdcp->srm_info.vrl_len_in_dram > vrl_index) {
		nom_of_device = *(ptr_srm + vrl_index) & 0x7F;	/* 40*N */
		vrl_index++;
		for (ksv_inx = 0; ksv_inx < nom_of_device; ksv_inx++) {
			for (dw_index = 0; dw_index < 5; dw_index++) {
				index_tmp = vrl_index + (ksv_inx * 5) + dw_index;
				if (ptr_srm[index_tmp] != hdcp->hdcp_bksv[dw_index])
					break;
			}

			if (!mtk_hdmi_hdcp_is_repeater(hdmi))
				goto check_index;

			for (ksv_sink_index = 0;
				ksv_sink_index <
				mtk_hdmi_hdcp_get_share_info(hdmi,
					SI_REPEATER_DEVICE_COUNT);
				ksv_sink_index++) {
				for (index = 0; index < 5; index++) {
					if ((((ksv_sink_index + 1) * 5 -
						index - 1) < 192) &&
						(*(ptr_srm + vrl_index +
						(ksv_inx * 5) + index) !=
						hdcp->ksv_buff[(ksv_sink_index + 1)
						* 5 - index - 1]))
						break;
				}
				if (index == 5)
					break;
			}
check_index:
			if ((dw_index == 5) || (index == 5)) {
				mtk_hdmi_hdcp_set_share_info(hdmi, SI_DVD_HDCP_REVOCATION_RESULT,
				REVOCATION_IS_CHK | IS_REVOCATION_KEY);
				break;
			}
			mtk_hdmi_hdcp_set_share_info(hdmi, SI_DVD_HDCP_REVOCATION_RESULT,
			       REVOCATION_IS_CHK | NOT_REVOCATION_KEY);
		}
		if ((dw_index == 5) || (index == 5))
			break;
		vrl_index += nom_of_device * 5;
	}

	HDMI_HDCP_LOG("[HDCP]Shared Info=%x",
		mtk_hdmi_hdcp_get_share_info(hdmi, SI_DVD_HDCP_REVOCATION_RESULT));
	if (mtk_hdmi_hdcp_get_share_info(hdmi, SI_DVD_HDCP_REVOCATION_RESULT) &
		IS_REVOCATION_KEY)
		HDMI_HDCP_LOG("[HDCP]Revoked Sink Key\n");
}
#endif

bool mtk_hdmi_hdcp_is_ksv_legal(unsigned char ksv[HDCP_AKSV_COUNT])
{
	unsigned char i, bit_shift, one_cnt;

	one_cnt = 0;
	for (i = 0; i < HDCP_AKSV_COUNT; i++) {
		for (bit_shift = 0; bit_shift < 8; bit_shift++)
			if (ksv[i] & BIT(bit_shift))
				one_cnt++;
	}
	if (one_cnt == 20)
		return true;

	HDMI_HDCP_LOG("[HDCP],err ksv is:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
		ksv[0], ksv[1], ksv[2], ksv[3], ksv[4]);
	return false;

}

void mtk_hdmi_hdcp_exchange_ksvs(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;
	unsigned char hdcp_buf[HDCP_AKSV_COUNT] = {0};

#ifdef SRM_SUPPORT
	unsigned char index;
#endif
	/* Step 1: read Aksv from transmitter, and send to receiver */
	HDMI_HDCP_LOG("hdmi_aksv:0x%x 0x%x 0x%x 0x%x 0x%x\n",
		hdcp->hdmi_aksv[0], hdcp->hdmi_aksv[1], hdcp->hdmi_aksv[2], hdcp->hdmi_aksv[3],
		hdcp->hdmi_aksv[4]);

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

	fg_ddc_data_write(ddc, RX_ID, RX_REG_HDCP_AKSV, HDCP_AKSV_COUNT,
		hdcp->hdmi_aksv);

	/* Step 4: read Bksv from receiver, and send to transmitter */
	fg_ddc_data_read(ddc, RX_ID, RX_REG_HDCP_BKSV, HDCP_BKSV_COUNT, hdcp_buf);
	HDMI_HDCP_LOG("hdcp_buf 0x%x; 0x%x; 0x%x; 0x%x; 0x%x\n",
		hdcp_buf[0], hdcp_buf[1],
		      hdcp_buf[2], hdcp_buf[3], hdcp_buf[4]);
	mtk_hdmi_hdcp_write_bksv_to_tx(hdmi, hdcp_buf);
	HDMI_HDCP_LOG("BSKV:0x%x 0x%x 0x%x 0x%x 0x%x\n",
		hdcp_buf[0], hdcp_buf[1],
		hdcp_buf[2], hdcp_buf[3], hdcp_buf[4]);

	for (index = 0; index < HDCP_AKSV_COUNT; index++)
		hdcp->tx_bkav[index] = hdcp_buf[index];

#ifdef SRM_SUPPORT
	for (index = 0; index < HDCP_AKSV_COUNT; index++)
		hdcp->hdcp_bksv[index] = hdcp_buf[HDCP_AKSV_COUNT - index - 1];

	mtk_hdmi_hdcp_compare_srm(hdmi);
#endif

}

unsigned char mtk_hdmi_hdcp_check_ri_status(struct mtk_hdmi *hdmi)
{
	HDMI_HDCP_FUNC();

	if (mtk_hdmi_read(hdmi, HDCP1x_STATUS) & (1 << 25))
		return true;
	else
		return false;
}

bool mtk_hdmi_hdcp_compare_ri(struct mtk_hdmi *hdmi)
{
	unsigned int temp;
	unsigned char hdcp_buf[4];
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	HDMI_HDCP_FUNC();
	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return false;
	}

	hdcp_buf[2] = mtk_hdmi_read(hdmi, KSV_RI_STATUS) & 0xff;
	hdcp_buf[3] = (mtk_hdmi_read(hdmi, KSV_RI_STATUS) >> 8) & 0xff;

	/* Read R0'/ Ri' from Receiver */
	fg_ddc_data_read(ddc, RX_ID, RX_REG_RI, HDCP_RI_COUNT,
				hdcp_buf);

	if (hdcp->hdcp_ctrl_state == HDCP_COMPARE_R0)
		HDMI_HDCP_LOG(
			"[HDCP1.x][R0]Rx_Ri=0x%x%x Tx_Ri=0x%x%x\n",
			hdcp_buf[0], hdcp_buf[1], hdcp_buf[2], hdcp_buf[3]);
	/* compare R0 and R0' */
	for (temp = 0; temp < HDCP_RI_COUNT; temp++) {
		if (((temp + HDCP_RI_COUNT) < sizeof(hdcp_buf))
			&& (hdcp_buf[temp] == hdcp_buf[temp + HDCP_RI_COUNT])) {
			continue;
		} else {	/* R0 != R0' */
			break;
		}
	}

	/* return the compare result */
	if (temp == HDCP_RI_COUNT) {
		hdcp->hdcp_status = SV_OK;
		return true;
	}
	hdcp->hdcp_status = SV_FAIL;
	HDMI_HDCP_LOG("[HDCP][1.x]Rx_Ri=0x%x%x Tx_Ri=0x%x%x\n",
		      hdcp_buf[0], hdcp_buf[1], hdcp_buf[2], hdcp_buf[3]);
	return false;
}

void mtk_hdmi_hdcp_enable_encrypt(struct mtk_hdmi *hdmi)
{
	HDMI_HDCP_FUNC();

	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 1 << 6, 1 << 6);
}

void mtk_hdmi_hdcp_write_ksv_list_port(struct mtk_hdmi *hdmi,
		unsigned char *ksv_data,
		unsigned char device_count,
		unsigned char *pr_status)
{
	unsigned char index;

	HDMI_HDCP_FUNC();

	if ((device_count * 5) < KSV_BUFF_SIZE) {
		mtk_hdmi_write(hdmi, TXDS_BSTATUS, (*(pr_status)) +
			((*(pr_status + 1)) << 8));
		mtk_hdmi_mask(hdmi, HDCP1X_CTRL, (1 << 3), (1 << 3));
		HDMI_HDCP_LOG("[HDCP]HDCP1X_CTRL(0xc1c)=0x%08x\n",
			mtk_hdmi_read(hdmi, TXDS_BSTATUS));

		for (index = 0; index < (device_count * 5); index++) {
			HDMI_HDCP_LOG("[HDCP]HDCP1X_SHA_CTRL(0xcd4)=0x%08x\n",
				(*(ksv_data + index)) + (1 << 8));
			mtk_hdmi_write(hdmi, HDCP1X_SHA_CTRL,
				(*(ksv_data + index)) + (1 << 8));
		}
	}
}

void mtk_hdmi_hdcp_write_hash_port(struct mtk_hdmi *hdmi, unsigned char *hash_vbuff)
{
	unsigned char index;

	HDMI_HDCP_FUNC();

	for (index = 0; index < 5; index++) {
		HDMI_HDCP_LOG("[HDCP]write v,0x%08x =0x%08x\n",
			(HDCP1X_V_H0 + index * 4),
			((*(hash_vbuff + 3 + index * 4)) << 24) +
			((*(hash_vbuff + 2 + index * 4)) << 16) +
			((*(hash_vbuff + 1 + index * 4)) << 8) +
			(*(hash_vbuff + 0 + index * 4)));
	}

	mtk_hdmi_write(hdmi, HDCP1X_V_H0,
		((*(hash_vbuff + 3)) << 24) +
		((*(hash_vbuff + 2)) << 16) +
		((*(hash_vbuff + 1)) << 8) +
		(*(hash_vbuff + 0)));

	mtk_hdmi_write(hdmi, HDCP1X_V_H1,
		((*(hash_vbuff + 7)) << 24) +
		((*(hash_vbuff + 6)) << 16) +
		((*(hash_vbuff + 5)) << 8) +
		(*(hash_vbuff + 4)));

	mtk_hdmi_write(hdmi, HDCP1X_V_H2,
		((*(hash_vbuff + 11)) << 24) +
		((*(hash_vbuff + 10)) << 16) +
		((*(hash_vbuff + 9)) << 8) +
		(*(hash_vbuff + 8)));

	mtk_hdmi_write(hdmi, HDCP1X_V_H3,
		((*(hash_vbuff + 15)) << 24) +
		((*(hash_vbuff + 14)) << 16) +
		((*(hash_vbuff + 13)) << 8) +
		(*(hash_vbuff + 12)));

	mtk_hdmi_write(hdmi, HDCP1X_V_H4,
		((*(hash_vbuff + 19)) << 24) +
		((*(hash_vbuff + 18)) << 16) +
		((*(hash_vbuff + 17)) << 8) +
		(*(hash_vbuff + 16)));

	for (index = 0; index < 5; index++) {
		HDMI_HDCP_LOG("[HDCP]read v,0x%08x =0x%08x\n",
			(HDCP1X_V_H0 + index * 4), mtk_hdmi_read(hdmi, HDCP1X_V_H0 + index * 4));
	}
}

void mtk_hdmi_hdcp_read_ksv_fifo(struct mtk_hdmi *hdmi)
{
	unsigned char temp, index, device_count = 0;
	unsigned char status[2] = {0}, status1 = 0;
	unsigned int tx_bstatus = 0;
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	HDMI_HDCP_FUNC();

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

	fg_ddc_data_read(ddc, RX_ID, RX_REG_BSTATUS1 + 1, 1, &status1);
	fg_ddc_data_read(ddc, RX_ID, RX_REG_BSTATUS1, 1, &device_count);
	hdcp->tx_bstatus = (((unsigned int)status1) << 8) | device_count;

	device_count &= DEVICE_COUNT_MASK;

	if ((device_count & MAX_DEVS_EXCEEDED) ||
		(status1 & MAX_CASCADE_EXCEEDED)) {

		fg_ddc_data_read(ddc, RX_ID, RX_REG_BSTATUS1, 2, status);
		fg_ddc_data_read(ddc, RX_ID, RX_REG_BSTATUS1, 1, &device_count);
		device_count &= DEVICE_COUNT_MASK;
		hdcp->tx_bstatus = status[0] | (status[1] << 8);
		mtk_hdmi_hdcp_set_share_info(hdmi, SI_REPEATER_DEVICE_COUNT, device_count);
		if (mtk_hdmi_hdcp_get_share_info(hdmi, SI_REPEATER_DEVICE_COUNT) == 0)
			hdcp->device_count = 0;
		else
			hdcp->device_count = device_count;

		hdcp->tx_bstatus = tx_bstatus;
		mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
		hdcp->hdcp_status = SV_FAIL;
		return;
	}

	if (device_count > 32) {
		for (temp = 0; temp < 2; temp++) {
			fg_ddc_data_read(ddc, RX_ID, RX_REG_BSTATUS1, 1,
				&device_count);
			device_count &= DEVICE_COUNT_MASK;
			if (device_count <= 32)
				break;
		}
		if (temp == 2)
			device_count = 32;
	}

	mtk_hdmi_hdcp_set_share_info(hdmi, SI_REPEATER_DEVICE_COUNT, device_count);

	if (device_count == 0) {
		for (index = 0; index < 5; index++)
			hdcp->ksv_buff[index] = 0;

		for (index = 0; index < 2; index++)
			status[index] = 0;

		for (index = 0; index < 20; index++)
			hdcp->sha_buff[index] = 0;

		mtk_hdmi_mask(hdmi, HDCP1X_CTRL, (1 << 11), (1 << 11));
	} else {
		fg_ddc_data_read(ddc, RX_ID, RX_REG_KSV_FIFO, device_count * 5,
			hdcp->ksv_buff);
		mtk_hdmi_mask(hdmi, HDCP1X_CTRL, (0 << 11), (1 << 11));
	}

	HDMI_HDCP_LOG("[1x]device_count = %d\n", device_count);

	fg_ddc_data_read(ddc, RX_ID, RX_REG_BSTATUS1, 2, status);
	fg_ddc_data_read(ddc, RX_ID, RX_REG_REPEATER_V, 20, hdcp->sha_buff);

	tx_bstatus = status[0] | (status[1] << 8);
	HDMI_HDCP_LOG(
	"[1x]TX BSTATUS: status[0]=%x, status[1]=%x, tx_bstatus=%x\n",
		   status[0], status[1], tx_bstatus);

#ifdef SRM_SUPPORT
	mtk_hdmi_hdcp_compare_srm(hdmi);
#endif

	mtk_hdmi_hdcp_write_ksv_list_port(hdmi, hdcp->ksv_buff, device_count, status);
	mtk_hdmi_hdcp_write_hash_port(hdmi, hdcp->sha_buff);

	mtk_hdmi_hdcp_set_state(hdmi, HDCP_COMPARE_V);
	/* set time-out value as 0.5 sec */
	mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_V_RDY_TIMEOUT);

	for (index = 0; index < device_count; index++) {
		if ((index * 5 + 4) < KSV_BUFF_SIZE) {
			HDMI_HDCP_LOG("[HDCP1.x] KSV List: Device[%d]= %x,%x,%x,%x,%x\n",
				index, hdcp->ksv_buff[index * 5], hdcp->ksv_buff[index * 5 + 1],
				hdcp->ksv_buff[index * 5 + 2], hdcp->ksv_buff[index * 5 + 3],
				hdcp->ksv_buff[index * 5 + 4]);
		}
	}
	HDMI_HDCP_LOG("[HDCP][1.x]Tx BKSV: %x, %x, %x, %x, %x\n",
		hdcp->tx_bkav[0], hdcp->tx_bkav[1], hdcp->tx_bkav[2],
		hdcp->tx_bkav[3], hdcp->tx_bkav[4]);

	if (mtk_hdmi_hdcp_get_share_info(hdmi, SI_REPEATER_DEVICE_COUNT) == 0)
		hdcp->device_count = 0;
	else
		hdcp->device_count = device_count;

	hdcp->tx_bstatus = tx_bstatus;

	HDMI_HDCP_LOG("[1x]hdcp->device_count = %x, tx_bstatus = %x\n",
		hdcp->device_count, hdcp->tx_bstatus);

}

unsigned int mtk_hdmi_hdcp_read_status(struct mtk_hdmi *hdmi)
{
	HDMI_HDCP_LOG("[1x]HDCP1x_STATUS(0xcf4)=0x%08x\n", mtk_hdmi_read(hdmi, HDCP1x_STATUS));
	return mtk_hdmi_read(hdmi, HDCP1x_STATUS);
}

unsigned int mtk_hdmi_hdcp_read_irq_status01(struct mtk_hdmi *hdmi)
{
	HDMI_HDCP_LOG("[1x]TOP_INT_STA01(0x1ac)=0x%08x\n", mtk_hdmi_read(hdmi, TOP_INT_STA01));
	HDMI_HDCP_LOG("[1x]TOP_INT_CLR01(0x1bc)=0x%08x\n", mtk_hdmi_read(hdmi, TOP_INT_CLR01));
	return mtk_hdmi_read(hdmi, TOP_INT_STA01);
}

void mtk_hdmi_hdcp_write_aksv_key_mask(struct mtk_hdmi *hdmi, unsigned char *pr_data)
{
	unsigned char b_data;
	/* - write wIdx into 92. */
	b_data = 0x00;
	mtk_hdmi_mask(hdmi, HDCP1X_AES_CTRL, (b_data << 24), 0xff << 24);
	b_data = 0x00;
	mtk_hdmi_mask(hdmi, HDCP1X_AES_CTRL, (b_data << 16), 0xff << 16);
}

void mtk_hdmi_hdcp_a_key_done(struct mtk_hdmi *hdmi)
{
	HDMI_HDCP_FUNC();

	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, KEY_LOAD_DONE, KEY_LOAD_DONE);
	udelay(100);
	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 0, KEY_LOAD_DONE);
}

unsigned int mtk_hdmi_hdcp2x_get_state_from_ddc(struct mtk_hdmi_ddc *ddc)
{
	struct mtk_hdmi *hdmi = hdmi_ctx_from_mtk_hdmi_ddc(ddc);

	return mtk_hdmi_hdcp2x_get_state(hdmi);
}

unsigned int mtk_hdmi_hdcp2x_get_state(struct mtk_hdmi *hdmi)
{
	return (mtk_hdmi_read(hdmi, HDCP2X_STATUS_0) &
		HDCP2X_STATE) >> 16;
}

bool mtk_hdmi_hdcp2x_is_err(struct mtk_hdmi *hdmi)
{
	unsigned int hdcp2_txstate;

	hdcp2_txstate = mtk_hdmi_hdcp2x_get_state(hdmi);
	if ((hdcp2_txstate == 0x30)
	    || (hdcp2_txstate == 0x31)
	    || (hdcp2_txstate == 0x32)
	    || (hdcp2_txstate == 0x33)
	    || (hdcp2_txstate == 0x34)
	    || (hdcp2_txstate == 0x35)
	    || (hdcp2_txstate == 0x36)
	    || (hdcp2_txstate == 0x37)
	    || (hdcp2_txstate == 0x38)
	    || (hdcp2_txstate == 0x39)
	    || (hdcp2_txstate == 0x3a)
	    || (hdcp2_txstate == 0x3b)
	    || (hdcp2_txstate == 0x3c)
	    || (hdcp2_txstate == 0x3d)
	    || (hdcp2_txstate == 0x3e))
		return true;
	return false;
}

unsigned char mtk_hdmi_hdcp_count_num1(unsigned char data)
{
	unsigned char i, count = 0;

	for (i = 0; i < 8; i++) {
		if (((data >> i) & 0x01) == 0x01)
			count++;
	}
	return count;
}

bool mtk_hdmi_hdcp_check_err_0x30(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (mtk_hdmi_hdcp2x_is_err(hdmi)) {
		if (mtk_hdmi_hdcp2x_get_state(hdmi) == 0x30)
			hdcp->hdcp_err_0x30_count++;
		else
			hdcp->hdcp_err_0x30_count = 0;
		if (hdcp->hdcp_err_0x30_count > 3) {
			hdcp->hdcp_err_0x30_count = 0;
			hdcp->hdcp_err_0x30_flag = 1;
			phy_power_off(hdmi->phy);
			HDMI_HDCP_LOG("err=0x30, signal off\n");
			return TRUE;
		}
	}
	return FALSE;
}

void mtk_hdmi_hdcp_send_tmds_config(struct mtk_hdmi *hdmi, unsigned char enscramble)
{
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);

	fg_ddc_data_write(ddc, RX_REG_SCRAMBLE >> 1, RX_REG_TMDS_CONFIG, 1,
		&enscramble);
}

void mtk_hdmi_hdcp_service(struct mtk_hdmi *hdmi)
{
	unsigned char index = 0, temp = 0, BStatus[2] = {0};
	unsigned char b_rpt_id[155];
	unsigned int readvalue = 0, i = 0, devicecnt = 0;
	unsigned int uitemp1 = 0, uitemp2 = 0, depth = 0, count1 = 0;
	bool fg_repeater_error = false;
	unsigned char ta_status[2] = {0};
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;
	enum HDCP_CTRL_STATE_T e_hdcp_state = hdcp->hdcp_ctrl_state;

	if (hdcp->enable_hdcp == false) {
		HDMI_HDCP_LOG("hdmi->enable_hdcp==false\n");
		mtk_hdmi_hdcp_set_state(hdmi, HDCP_RECEIVER_NOT_READY);
		mtk_hdmi_av_unmute(hdmi);
		return;
	}

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

	if (hdcp->key_is_installed == false) {
		if (mtk_hdmi_hdcp_ca_get_aksv(hdmi) == true)
			hdcp->key_is_installed = true;
	}

	switch (e_hdcp_state) {
	case HDCP_RECEIVER_NOT_READY:
		HDMI_HDCP_LOG("HDCP_RECEIVER_NOT_READY\n");
		break;

	case HDCP_READ_EDID:
		break;

	case HDCP_WAIT_RES_CHG_OK:
		if (mtk_hdmi_hdcp_check_ctrl_timeout(hdmi)) {
			if (hdcp->enable_hdcp == false) {	/* disable HDCP */
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_RECEIVER_NOT_READY);
				mtk_hdmi_av_unmute(hdmi);
			} else {
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
			}
		}
		break;

	case HDCP_INIT_AUTHENTICATION:
		/* HDCP_1P4_TCLK_EN, HDCP_1P4_TCLK_EN); */
		mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, HDCP_1P4_TCLK_EN, HDCP_1P4_TCLK_EN);
		HDMI_HDCP_LOG("HDCP_INIT_AUTHENTICATION repeater_hdcp = %d\n", hdcp->repeater_hdcp);

		if (hdcp->repeater_hdcp == true) {
			if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_RECEIVER_NOT_READY);
				HDMI_HDCP_LOG("[1.x][REPEATER] hpd low\n");
				break;
			}
		}
		if (hdcp->enable_mute_for_hdcp_flag)
			mtk_hdmi_av_mute(hdmi);

		mtk_hdmi_hdcp_set_share_info(hdmi, SI_HDMI_HDCP_RESULT, 0);

		mutex_lock(&ddc->mtx);
		hdmi_ddc_request(ddc, 5);
		mutex_unlock(&ddc->mtx);

		if (!fg_ddc_data_read(ddc, RX_ID, RX_REG_BCAPS, 1,
			&temp)) {
			HDMI_HDCP_LOG(
			"[1.x]fail-->HDCP_INIT_AUTHENTICATION-->0\n");
			hdcp->hdcp_status = SV_FAIL;
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_300MS_TIMEOUT);
			break;
		}
		if (!(hdmi->dvi_mode)) {
			fg_ddc_data_read(ddc, RX_ID, RX_REG_BSTATUS1, 2, BStatus);
			if ((BStatus[1] & 0x10) == 0) {
				HDMI_HDCP_LOG("[1x]BStatus=0x%x,0x%x\n",
					BStatus[0], BStatus[1]);
				hdcp->re_check_bstatus_count++;
	/* wait for upto 4.5 seconds to detect otherwise proceed anyway */
				if (hdcp->re_check_bstatus_count < 15) {
					hdcp->hdcp_status = SV_FAIL;
					mtk_hdmi_hdcp_set_delay_time(hdmi,
								     HDCP_WAIT_300MS_TIMEOUT);
					break;
				}
			}
		}

		HDMI_HDCP_LOG("[1x]RX_REG_BCAPS = 0x%08x\n", temp);
		for (index = 0; index < HDCP_AKSV_COUNT; index++) {
			hdcp->hdmi_aksv[index] = hdcp->ca_hdcp_aksv[index];
			HDMI_HDCP_LOG("[1x]hdmi_aksv[%d] = 0x%x\n",
				index,
				hdcp->hdmi_aksv[index]);
		}

		mtk_hdmi_hdcp_write_aksv_key_mask(hdmi, hdcp->hdmi_aksv);

		fg_ddc_data_read(ddc, RX_ID, RX_REG_BCAPS, 1, &temp);
		mtk_hdmi_hdcp_set_share_info(hdmi, SI_REPEATER_DEVICE_COUNT, 0);
		if (temp & RX_BIT_ADDR_RPTR)
			hdcp->downstream_is_repeater = true;
		else
			hdcp->downstream_is_repeater = false;

		if (mtk_hdmi_hdcp_is_repeater(hdmi))
			mtk_hdmi_hdcp_repeater_on_off(hdmi, true);
		else
			mtk_hdmi_hdcp_repeater_on_off(hdmi, false);

		mtk_hdmi_hdcp_send_an(hdmi);
		mtk_hdmi_hdcp_exchange_ksvs(hdmi);
		if ((!mtk_hdmi_hdcp_is_ksv_legal(hdcp->hdmi_aksv)) ||
			(!mtk_hdmi_hdcp_is_ksv_legal(hdcp->tx_bkav))) {
			HDMI_HDCP_LOG(
		"[1x]fail-->HDCP_INIT_AUTHENTICATION-->mtk_hdmi_hdcp_is_ksv_legal\n");
			if (hdcp->enable_mute_for_hdcp_flag)
				mtk_hdmi_av_mute(hdmi);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
			hdcp->hdcp_status = SV_FAIL;
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_300MS_TIMEOUT);
			break;
		}

#ifdef SRM_SUPPORT
		if (((hdcp->repeater_hdcp == false) &&
			((mtk_hdmi_hdcp_get_share_info(hdmi, SI_DVD_HDCP_REVOCATION_RESULT) &
			IS_REVOCATION_KEY) && (hdcp->srm_info.id == 0x80)))) {
			HDMI_HDCP_LOG(
			"[1x]fail-->HDCP_INIT_AUTHENTICATION-->1\n");
			if (hdcp->enable_mute_for_hdcp_flag)
				mtk_hdmi_av_mute(hdmi);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
			hdcp->hdcp_status = SV_FAIL;
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_300MS_TIMEOUT);
			break;

		}
#endif

		mtk_hdmi_hdcp_ca_load_hdcp_key(hdmi);
		/* set time-out value as 100 ms */
		mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_R0_TIMEOUT);
		mtk_hdmi_hdcp_a_key_done(hdmi);

		/* change state as waiting R0 */
		mtk_hdmi_hdcp_set_state(hdmi, HDCP_WAIT_R0);
		break;

	case HDCP_WAIT_R0:
		temp = mtk_hdmi_hdcp_check_ri_status(hdmi);
		if (temp == true) {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_COMPARE_R0);
		} else {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
			hdcp->hdcp_status = SV_FAIL;
			break;
		}

	case HDCP_COMPARE_R0:

		if (mtk_hdmi_hdcp_compare_ri(hdmi) == true) {
			mtk_hdmi_hdcp_enable_encrypt(hdmi);	/* Enabe encrption */
			/* change state as check repeater */
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_CHECK_REPEATER);
			mtk_hdmi_hdcp_set_share_info(hdmi, SI_HDMI_HDCP_RESULT, 0x01);
		} else {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_COMPARE_R0);
			HDMI_HDCP_LOG("[1x]fail-->HDCP_WAIT_R0-->1\n");
			hdcp->re_comp_ri_count = 0;
		}

		break;

	case HDCP_RE_COMPARE_R0:
		hdcp->re_comp_ri_count++;
		if (mtk_hdmi_hdcp_check_ctrl_timeout(hdmi) && hdcp->re_comp_ri_count > 3) {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
			hdcp->hdcp_status = SV_FAIL;
			hdcp->re_comp_ri_count = 0;
			HDMI_HDCP_LOG("[1x]fail-->HDCP_WAIT_R0-->2\n");
		} else {
			if (mtk_hdmi_hdcp_compare_ri(hdmi) == true) {
				mtk_hdmi_hdcp_enable_encrypt(hdmi); /* Enabe encrption */
				/* change state as check repeater */
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_CHECK_REPEATER);
				mtk_hdmi_hdcp_set_share_info(hdmi, SI_HDMI_HDCP_RESULT, 0x01);
			}
		}
		break;

	case HDCP_CHECK_REPEATER:
		HDMI_HDCP_LOG("[1x]HDCP_CHECK_REPEATER\n");
		/* if the device is a Repeater, */
		if (mtk_hdmi_hdcp_is_repeater(hdmi)) {
			hdcp->re_check_ready_bit = 0;
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_WAIT_KSV_LIST);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_KSV_LIST_TIMEOUT);
		} else {

			hdcp->device_count = 0;
			hdcp->tx_bstatus = 0;
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_WAIT_RI);
		}

		break;

	case HDCP_WAIT_KSV_LIST:

		fg_ddc_data_read(ddc, RX_ID, RX_REG_BCAPS, 1, &temp);
		if ((temp & RX_BIT_ADDR_READY)) {
			hdcp->re_check_ready_bit = 0;
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_READ_KSV_LIST);
		} else {
			if (hdcp->re_check_ready_bit >
				HDCP_CHECK_KSV_LIST_RDY_RETRY_COUNT) {
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
				hdcp->re_check_ready_bit = 0;
				hdcp->hdcp_status = SV_FAIL;
				HDMI_HDCP_LOG("[1x]HDCP_WAIT_KSV_LIST, fail\n");
			} else {
				hdcp->re_check_ready_bit++;
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_WAIT_KSV_LIST);
				mtk_hdmi_hdcp_set_delay_time(hdmi,
							     HDCP_WAIT_KSV_LIST_RETRY_TIMEOUT);
			}
			break;
		}

	case HDCP_READ_KSV_LIST:
		mtk_hdmi_hdcp_read_ksv_fifo(hdmi);
#ifdef SRM_SUPPORT
		if (((hdcp->repeater_hdcp == false) &&
		    (mtk_hdmi_hdcp_get_share_info(hdmi, SI_DVD_HDCP_REVOCATION_RESULT) &
		    IS_REVOCATION_KEY) &&
		    (hdcp->srm_info.id == 0x80))) {
			if (hdcp->enable_mute_for_hdcp_flag)
				mtk_hdmi_av_mute(hdmi);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
			hdcp->hdcp_status = SV_FAIL;
			/* add 300 ms issue next command */
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_300MS_TIMEOUT);
			break;
		}
#endif
		break;

	case HDCP_COMPARE_V:
		uitemp1 = mtk_hdmi_hdcp_read_status(hdmi);
		uitemp2 = mtk_hdmi_hdcp_read_irq_status01(hdmi);
		mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0x00004000);
		udelay(10);
		mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0x00000000);
		if ((uitemp2 & (1 << 14)) || (uitemp1 & (1 << 28))) {
			if ((uitemp2 & (1 << 14))) {
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_WAIT_RI);
				HDMI_HDCP_LOG("[1x]HDCP_COMPARE_V, pass\n");
				mtk_hdmi_hdcp_set_share_info(hdmi, SI_HDMI_HDCP_RESULT,
							    (mtk_hdmi_hdcp_get_share_info(hdmi,
					 SI_HDMI_HDCP_RESULT) | 0x02));
				/* step 2 OK. */
			} else {
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
				HDMI_HDCP_LOG("[1x]HDCP_COMPARE_V, fail\n");
				hdcp->hdcp_status = SV_FAIL;
			}
		} else {
			HDMI_HDCP_LOG("[HDCP]V Not RDY\n");
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
			hdcp->hdcp_status = SV_FAIL;
		}
		break;

	case HDCP_WAIT_RI:
		mtk_hdmi_av_unmute(hdmi);
		HDMI_HDCP_LOG("[HDCP1.x]pass, %lums\n", jiffies);
		break;

	case HDCP_CHECK_LINK_INTEGRITY:
#ifdef SRM_SUPPORT
		if (((hdcp->repeater_hdcp == false) &&
		    ((mtk_hdmi_hdcp_get_share_info(hdmi, SI_DVD_HDCP_REVOCATION_RESULT) &
			IS_REVOCATION_KEY) && (hdcp->srm_info.id == 0x80)))) {
			if (hdcp->enable_mute_for_hdcp_flag)
				mtk_hdmi_av_mute(hdmi);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
			hdcp->hdcp_status = SV_FAIL;
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_300MS_TIMEOUT);
			break;

		}
#endif
		if (mtk_hdmi_hdcp_compare_ri(hdmi) == true) {
			mtk_hdmi_hdcp_set_share_info(hdmi, SI_HDMI_HDCP_RESULT,
				(mtk_hdmi_hdcp_get_share_info(hdmi, SI_HDMI_HDCP_RESULT) |
				       0x04));
			/* step 3 OK. */
			if (mtk_hdmi_hdcp_is_repeater(hdmi)) {
				if (mtk_hdmi_hdcp_get_share_info(hdmi, SI_HDMI_HDCP_RESULT) ==
					0x07) {	/* step 1, 2, 3. */
					mtk_hdmi_hdcp_set_share_info(hdmi, SI_HDMI_HDCP_RESULT,
					(mtk_hdmi_hdcp_get_share_info(hdmi, SI_HDMI_HDCP_RESULT) |
						       0x08));
					/* all ok. */
				}
			} else {	/* not repeater, don't need step 2. */

				if (mtk_hdmi_hdcp_get_share_info(hdmi, SI_HDMI_HDCP_RESULT) ==
					0x05) {	/* step 1, 3. */
					mtk_hdmi_hdcp_set_share_info(hdmi, SI_HDMI_HDCP_RESULT,
					(mtk_hdmi_hdcp_get_share_info(hdmi, SI_HDMI_HDCP_RESULT) |
						       0x08));
					/* all ok. */
				}
			}
		} else {
			hdcp->re_comp_ri_count = 0;
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_COMPARE_RI);
			HDMI_HDCP_LOG("[1x]fai-->HDCP_CHECK_LINK_INTEGRITY\n");

		}
		break;

	case HDCP_RE_COMPARE_RI:
		HDMI_HDCP_LOG("[1x]HDCP_RE_COMPARE_RI\n");
		hdcp->re_comp_ri_count++;
		if (hdcp->re_comp_ri_count > 5) {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_DO_AUTHENTICATION);
			hdcp->re_comp_ri_count = 0;
			hdcp->hdcp_status = SV_FAIL;
			HDMI_HDCP_LOG("[1x]fai-->HDCP_RE_COMPARE_RI\n");
		} else {
			if (mtk_hdmi_hdcp_compare_ri(hdmi) == true) {
				hdcp->re_comp_ri_count = 0;
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_CHECK_LINK_INTEGRITY);
				mtk_hdmi_hdcp_set_share_info(hdmi, SI_HDMI_HDCP_RESULT,
				(mtk_hdmi_hdcp_get_share_info(hdmi, SI_HDMI_HDCP_RESULT) | 0x04));
				/* step 3 OK. */
				if (mtk_hdmi_hdcp_is_repeater(hdmi)) {
					if (mtk_hdmi_hdcp_get_share_info(hdmi,
						SI_HDMI_HDCP_RESULT) == 0x07) {	/* step 1, 2, 3. */
						mtk_hdmi_hdcp_set_share_info(hdmi,
							SI_HDMI_HDCP_RESULT,
							(mtk_hdmi_hdcp_get_share_info(hdmi,
							SI_HDMI_HDCP_RESULT) | 0x08));
						/* all ok. */
					}
				} else {
					if (mtk_hdmi_hdcp_get_share_info(hdmi,
						SI_HDMI_HDCP_RESULT) == 0x05) {	/* step 1, 3. */
						mtk_hdmi_hdcp_set_share_info(hdmi,
							SI_HDMI_HDCP_RESULT,
							(mtk_hdmi_hdcp_get_share_info(hdmi,
							SI_HDMI_HDCP_RESULT) | 0x08));
						/* all ok. */
					}
				}

			} else {
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_RE_COMPARE_RI);
			}
		}
		break;

	case HDCP_RE_DO_AUTHENTICATION:
		HDMI_HDCP_LOG("[1x]HDCP_RE_DO_AUTHENTICATION\n");
		if (hdcp->enable_mute_for_hdcp_flag)
			mtk_hdmi_av_mute(hdmi);
		mtk_hdmi_hdcp_reset(hdmi);
		if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RECEIVER_NOT_READY);
		} else {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_WAIT_RESET_OK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP_WAIT_RE_DO_AUTHENTICATION);
		}
		break;

	case HDCP_WAIT_RESET_OK:
		if (mtk_hdmi_hdcp_check_ctrl_timeout(hdmi))
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_INIT_AUTHENTICATION);
		break;

		/*hdcp2 code start here */
	case HDCP2x_WAIT_RES_CHG_OK:
		HDMI_HDCP_LOG("HDCP2x_WAIT_RES_CHG_OK, %lums\n", jiffies);
		if (mtk_hdmi_hdcp_check_ctrl_timeout(hdmi)) {
			if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_RECEIVER_NOT_READY);
				HDMI_HDCP_LOG("set state HDCP_RECEIVER_NOT_READY\n");
			} else if (hdcp->enable_hdcp == false) {
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_RECEIVER_NOT_READY);
				mtk_hdmi_av_unmute(hdmi);
				HDMI_HDCP_LOG("set state HDCP_RECEIVER_NOT_READY\n");
			} else {
				if (hdcp->enable_mute_for_hdcp_flag)
					mtk_hdmi_av_mute(hdmi);

				if (hdcp->hdcp_err_0x30_flag == 1) {
					hdcp->hdcp_err_0x30_flag = 0;
					mutex_lock(&ddc->mtx);
					hdmi_ddc_request(ddc, 3);
					mutex_unlock(&ddc->mtx);
					if (mtk_hdmi_tmds_over_340M(hdmi) == TRUE)
						mtk_hdmi_hdcp_send_tmds_config(hdmi,
							SCRAMBLING_ENABLE | TMDS_BIT_CLOCK_RATION);
					else
						mtk_hdmi_hdcp_send_tmds_config(hdmi, 0);
					HDMI_HDCP_LOG("err=0x30, signal on\n");
					phy_power_on(hdmi->phy);
				}

				hdcp->re_repeater_poll_cnt = 0;
				hdcp->re_cert_poll_cnt = 0;
				hdcp->re_auth_cnt = 0;
				mtk_hdmi_hdcp2x_clear_int(hdmi);
				mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_LOAD_FW);

				mtk_hdmi_mask(hdmi, HDCP2X_RPT_SEQ_NUM, 0,
					      HDCP2X_RPT_SEQ_NUM_M);
				mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, 0,
					      HDCP2X_ENCRYPT_EN);
				mtk_hdmi_mask(hdmi, HPD_DDC_CTRL,
					      DDC2_CLOK << DDC_DELAY_CNT_SHIFT,
					      DDC_DELAY_CNT);
			}
		}
		break;

	case HDCP2x_LOAD_FW:
		if (hdcp->repeater_hdcp == true) {
			if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
				mtk_hdmi_hdcp_set_state(hdmi, HDCP_RECEIVER_NOT_READY);
				HDMI_HDCP_LOG(
				"[HDMI][HDCP2.x][REPEATER]TMDS is off\n");
				break;
			}
		}
		HDMI_HDCP_LOG("HDCP2x_LOAD_FW, flag = %d\n", hdcp->fw_is_loaded);

		/* SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST); */
		mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST);

		if (hdcp->fw_is_loaded == false) {
			HDMI_HDCP_LOG("HDCP 2.x tee load firmware\n");
			mtk_hdmi_hdcp_ca_load_firmware(hdmi);
			hdcp->fw_is_loaded = true;
		} else {
			mtk_hdmi_hdcp_ca_reset_firmware(hdmi);
		}
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_CUPD_START,
			HDCP2X_CUPD_START);
		/* SOFT_HDCP_CORE_NOR, SOFT_HDCP_CORE_RST); */
		mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, SOFT_HDCP_CORE_NOR, SOFT_HDCP_CORE_RST);
		/* HDCP_TCLK_EN, HDCP_TCLK_EN); */
		mtk_hdmi_hdcp_ca_write_hdcp_rst(hdmi, HDCP_TCLK_EN, HDCP_TCLK_EN);

		if (hdcp->hdcp_ctrl_state != HDCP2x_LOAD_FW) {
			HDMI_HDCP_LOG("hdcp state changed by other thread\n");
			break;
		}

		mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_INITIAL_OK);
		mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_INITIAL_TIMEOUT);
		break;

	case HDCP2x_INITIAL_OK:
		HDMI_HDCP_LOG("HDCP2x_INITAIL_OK, %lums\n", jiffies);
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		HDMI_HDCP_LOG("TOP_INT_STA00 = 0x%08x\n", readvalue);
		if ((readvalue & HDCP2X_CCHK_DONE_INT_STA) &&
			((ta_status[0] & 0x03) == 0)) {
			HDMI_HDCP_LOG("hdcp2.2 ram/rom check is done\n");
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_AUTHENTICATION);
		} else {
			HDMI_HDCP_LOG("hdcp2.2 ram/rom check is fail, %x\n",
				ta_status[0]);
			mtk_hdmi_hdcp_init_auth(hdmi);
			hdcp->hdcp_status = SV_FAIL;
			hdcp->fw_is_loaded = false;
		}
		break;

	case HDCP2x_AUTHENTICATION:
		HDMI_HDCP_LOG("HDCP2x_AUTHENTICATION, %lums\n", jiffies);
		/* enable reauth_req irq */
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00, 0x02000000, 0x02000000);
		mtk_hdmi_mask(hdmi, HDCP2X_TEST_TP0, 0x75 <<
			      HDCP2X_TP1_SHIFT, HDCP2X_TP1);

		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0,
			      HDCP2X_HDMIMODE,
			      HDCP2X_HDMIMODE |
			      HDCP2X_ENCRYPT_EN);
		mtk_hdmi_mask(hdmi, SI2C_CTRL, RX_CAP_RD_TRIG,
			      RX_CAP_RD_TRIG);

		mtk_hdmi_mask(hdmi, HDCP2X_POL_CTRL, 0x3402,
			      HDCP2X_POL_VAL1 | HDCP2X_POL_VAL0);

		mtk_hdmi_write(hdmi, HDCP2X_TEST_TP0, 0x2a01be03);
		mtk_hdmi_write(hdmi, HDCP2X_TEST_TP1, 0x09026411);
		mtk_hdmi_write(hdmi, HDCP2X_TEST_TP2, 0xa7111110);
		mtk_hdmi_write(hdmi, HDCP2X_TEST_TP3, 0x00fa0d7d);
		mtk_hdmi_mask(hdmi, HDCP2X_GP_IN, 0x0 <<
			      HDCP2X_GP_IN2_SHIFT, HDCP2X_GP_IN2);
		mtk_hdmi_mask(hdmi, HDCP2X_GP_IN, 0x0 <<
			      HDCP2X_GP_IN3_SHIFT, HDCP2X_GP_IN3);

		/* set SM */
		mtk_hdmi_hdcp_ca_write_ctrl(hdmi, 0x88880000, 0xaaaa0000);

		mtk_hdmi_hdcp_ddc_hw_poll(hdmi, true);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_EN, HDCP2X_EN);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_REAUTH_SW,
			HDCP2X_REAUTH_SW);
		udelay(1);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, 0, HDCP2X_REAUTH_SW);
		mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_CHECK_CERT_OK);
		mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_CERT_TIMEOUT);
		break;

	case HDCP2x_CHECK_AKE_OK:
		HDMI_HDCP_LOG("HDCP2x_CHECK_AKE_OK\n");
		HDMI_HDCP_LOG("TOP_INT_STA00=0x%08x,HPD_DDC_STATUS=0x%08x,HDCP2X_STATUS_0=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0));
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if (readvalue & HDCP2X_AKE_SENT_RCVD_INT_STA) {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_CHECK_CERT_OK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_CERT_TIMEOUT);
			HDMI_HDCP_LOG("HDCP2x_CHECK_AKE_OK, hdcp->re_ake_t_poll_cnt = %d\n",
				      hdcp->re_ake_t_poll_cnt);
			hdcp->re_ake_t_poll_cnt = 0;
		} else {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_CHECK_AKE_OK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_AKE_TIMEOUT);
			hdcp->re_ake_t_poll_cnt++;
		}
		break;

	case HDCP2x_CHECK_CERT_OK:
		HDMI_HDCP_LOG("HDCP2x_CHECK_CERT_OK");
		HDMI_HDCP_LOG("TOP_INT_STA00=0x%08x,HPD_DDC_STATUS=0x%08x,HDCP2X_STATUS_0=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0));
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if (readvalue & HDCP2X_CERT_SEND_RCVD_INT_STA) {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_REPEATER_CHECK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_REPEATER_CHECK_TIMEOUT);
			hdcp->re_cert_poll_cnt = 0;
			hdcp->hdcp_err_0x30_count = 0;
		} else if (hdcp->re_cert_poll_cnt < 20) {
			hdcp->re_cert_poll_cnt++;
			if (mtk_hdmi_hdcp_check_err_0x30(hdmi) == TRUE) {
				mutex_lock(&ddc->mtx);
				hdmi_ddc_request(ddc, 3);
				mutex_unlock(&ddc->mtx);
				mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_WAIT_RES_CHG_OK);
				mtk_hdmi_hdcp_set_delay_time(hdmi, 100);
				break;
			}
			if (mtk_hdmi_hdcp2x_is_err(hdmi)) {
				hdcp->re_cert_poll_cnt = 0;
				hdcp->hdcp_status = SV_FAIL;
				mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_WAIT_RES_CHG_OK);
				mtk_hdmi_hdcp_ddc_hw_poll(hdmi, false);
				HDMI_HDCP_LOG("hdcp2_err=%x, HDCP2X_STATE = %d\n",
				     mtk_hdmi_hdcp2x_get_state(hdmi), hdcp->re_cert_poll_cnt);
				break;
			}

			HDMI_HDCP_LOG("hdcp->re_cert_poll_cnt=%d\n", hdcp->re_cert_poll_cnt);

			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_CHECK_CERT_OK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, 10);
		} else {
			HDMI_HDCP_LOG("TOP_INT_STA00=0x%08x,HPD_DDC_STATUS=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
				 mtk_hdmi_read(hdmi, HPD_DDC_STATUS));
			HDMI_HDCP_LOG("HDCP2X_STATUS_0=0x%08x,%d\n",
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0), hdcp->re_cert_poll_cnt);
			hdcp->re_cert_poll_cnt = 0;
			hdcp->hdcp_status = SV_FAIL;
			hdcp->hdcp_err_0x30_count = 0;
			mtk_hdmi_hdcp_init_auth(hdmi);
		}
		break;

	case HDCP2x_REPEATER_CHECK:
		HDMI_HDCP_LOG("HDCP2x_REPEATER_CHECK\n");
		HDMI_HDCP_LOG("TOP_INT_STA00=0x%08x,HPD_DDC_STATUS=0x%08x,HDCP2X_STATUS_0=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0));
		readvalue = mtk_hdmi_read(hdmi, HDCP2X_STATUS_0);
		if (readvalue & HDCP2X_RPT_REPEATER) {
			HDMI_HDCP_LOG("downstream device is repeater\n");
			hdcp->downstream_is_repeater = true;
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_REPEATER_CHECK_OK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_REPEATER_POLL_TIMEOUT);
		} else {
			HDMI_HDCP_LOG("downstream device is receiver\n");
			hdcp->downstream_is_repeater = false;
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_AUTHEN_CHECK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_AUTHEN_TIMEOUT);

			hdcp->device_count = 0;
			hdcp->tx_bstatus = 0;
			hdcp->tx_bkav[4] = mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff;
			hdcp->tx_bkav[3] = (mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff00) >> 8;
			hdcp->tx_bkav[2] = (mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff0000) >> 16;
			hdcp->tx_bkav[1] = (mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff000000) >> 24;
			hdcp->tx_bkav[0] = (mtk_hdmi_read(hdmi, HDCP2X_RPT_SEQ) & 0xff000000) >> 24;

			hdcp->tx_bstatus = ((mtk_hdmi_read(hdmi, HDCP2X_STATUS_0) & 0x3c) >> 2) +
				(((mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & 0xff00) >> 8) << 4) +
				((mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & 0xff) << 9);
			HDMI_HDCP_LOG("[2.x]hdcp->tx_bkav=0x%x;0x%x;0x%x;0x%x;0x%x\n",
				      hdcp->tx_bkav[0], hdcp->tx_bkav[1], hdcp->tx_bkav[2],
				      hdcp->tx_bkav[3], hdcp->tx_bkav[4]);
			HDMI_HDCP_LOG("[2.x]tx_bstatus=0x%x\n", hdcp->tx_bstatus);
		}
		break;

	case HDCP2x_REPEATER_CHECK_OK:
		HDMI_HDCP_LOG("HDCP2x_REPEATER_CHECK_OK\n");
		HDMI_HDCP_LOG("TOP_INT_STA00=0x%08x,HPD_DDC_STATUS=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS));
		HDMI_HDCP_LOG("HDCP2X_STATUS_0=0x%08x,HDCP2X_STATUS_1=0x%08x\n",
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_1));
/*  0x1a0[23] can not work sometime, so add 0x1a0[24][16]  */
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if ((readvalue & HDCP2X_RPT_RCVID_CHANGED_INT_STA) ||
		    (readvalue & HDCP2X_RPT_SMNG_XFER_DONE_INT_STA) ||
		    (readvalue & HDCP2X_AUTH_DONE_INT_STA)) {
			hdcp->re_repeater_poll_cnt = 0;
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_RESET_RECEIVER);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_RESET_RECEIVER_TIMEOUT);

		} else if ((hdcp->re_repeater_poll_cnt <= 30) &&
			   (mtk_hdmi_hdcp2x_is_err(hdmi) == false)) {
			hdcp->re_repeater_poll_cnt++;
			HDMI_HDCP_LOG("hdcp->re_repeater_poll_cnt=%d\n",
				      hdcp->re_repeater_poll_cnt);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_REPEATER_CHECK_OK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_REPEATER_POLL_TIMEOUT);
		} else {
			HDMI_HDCP_LOG("[HDMI][HDCP2.x]assume repeater failure, hdcp2_err=%x\n",
			     mtk_hdmi_hdcp2x_get_state(hdmi));
			mtk_hdmi_hdcp_init_auth(hdmi);
			hdcp->hdcp_status = SV_FAIL;
			hdcp->re_repeater_poll_cnt = 0;

			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_LOADFW_TIMEOUT);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_WAIT_RES_CHG_OK);
		}
		break;

	case HDCP2x_RESET_RECEIVER:
		HDMI_HDCP_LOG("HDCP2x_RESET_RECEIVER\n");
		HDMI_HDCP_LOG("TOP_INT_STA00=0x%08x,HPD_DDC_STATUS=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS));
		HDMI_HDCP_LOG("HDCP2X_STATUS_0=0x%08x,%lums\n",
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0), jiffies);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2, HDCP2X_RPT_RCVID_RD_START,
			      HDCP2X_RPT_RCVID_RD_START);
		udelay(1);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2, 0, HDCP2X_RPT_RCVID_RD_START);
		devicecnt =
		    (mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & HDCP2X_RPT_DEVCNT) >>
		     HDCP2X_RPT_DEVCNT_SHIFT;

		depth = mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) &
			HDCP2X_RPT_DEPTH;
		if ((depth == 0) && (devicecnt != 0))
			fg_repeater_error = true;
		count1 = 0;

		b_rpt_id[0] =
		    (mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) &
		     HDCP2X_RPT_RCVID_OUT) >> HDCP2X_RPT_RCVID_OUT_SHIFT;
		count1 = count1 + mtk_hdmi_hdcp_count_num1(b_rpt_id[0]);
		for (i = 1; i < 5 * devicecnt; i++) {
			mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2,
				HDCP2X_RPT_RCVID_RD,
				HDCP2X_RPT_RCVID_RD);
			udelay(1);
			mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2, 0,
				HDCP2X_RPT_RCVID_RD);
			if (i < 155) {
				b_rpt_id[i] =
				    (mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) &
				     HDCP2X_RPT_RCVID_OUT) >> HDCP2X_RPT_RCVID_OUT_SHIFT;
				count1 = count1+mtk_hdmi_hdcp_count_num1(b_rpt_id[i]);
				if ((i % 5) == 4) {
					if (count1 != 20)
						fg_repeater_error = true;
					count1 = 0;
				}
			} else
				HDMI_HDCP_LOG("device count exceed\n");
		}

		for (i = 0; i < 5 * devicecnt; i++) {
			if ((i % 5) == 0)
				HDMI_HDCP_LOG("ID[%d]:", i / 5);

			HDMI_HDCP_LOG("0x%x,", b_rpt_id[i]);

			if ((i % 5) == 4)
				HDMI_HDCP_LOG("\n");
		}
		if (fg_repeater_error) {
			HDMI_HDCP_LOG("repeater parameter invalid\n");
			if (hdcp->enable_mute_for_hdcp_flag)
				mtk_hdmi_av_mute(hdmi);
			mtk_hdmi_hdcp_reset(hdmi);
			mtk_hdmi_hdcp_init_auth(hdmi);
			break;
		}

		hdcp->device_count = devicecnt;
		mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_REPEAT_MSG_DONE);
		mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_REPEATER_DONE_TIMEOUT);

		hdcp->tx_bkav[4] =
			mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff;
		hdcp->tx_bkav[3] =
			(mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff00) >> 8;
		hdcp->tx_bkav[2] =
			(mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff0000) >> 16;
		hdcp->tx_bkav[1] =
			(mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff000000) >> 24;
		hdcp->tx_bkav[0] =
			(mtk_hdmi_read(hdmi, HDCP2X_RPT_SEQ) & 0xff000000) >> 24;
		hdcp->tx_bstatus =
			((mtk_hdmi_read(hdmi, HDCP2X_STATUS_0) & 0x3c) >> 2) +
			(((mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & 0xff00) >> 8) << 4) +
			((mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & 0xff) << 9);
		break;

	case HDCP2x_REPEAT_MSG_DONE:
		HDMI_HDCP_LOG("HDCP2x_REPEAT_MSG_DONE\n");
		HDMI_HDCP_LOG("TOP_INT_STA00=0x%08x,HPD_DDC_STATUS=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS));
		HDMI_HDCP_LOG("HDCP2X_STATUS_0=0x%08x,%lums\n",
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0), jiffies);
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if ((readvalue & HDCP2X_RPT_SMNG_XFER_DONE_INT_STA) ||
		    (readvalue & HDCP2X_AUTH_DONE_INT_STA)) {
			hdcp->re_repeater_done_cnt = 0;
			mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2, 0, HDCP2X_RPT_SMNG_WR_START);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_AUTHEN_CHECK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_AUTHEN_TIMEOUT);
		} else if ((hdcp->re_repeater_done_cnt < 10) &&
			   (mtk_hdmi_hdcp2x_is_err(hdmi) == false)) {
			hdcp->re_repeater_done_cnt++;
			HDMI_HDCP_LOG("hdcp->re_repeater_done_cnt=%d\n",
				      hdcp->re_repeater_done_cnt);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_REPEAT_MSG_DONE);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_REPEATER_DONE_TIMEOUT);
		} else {
			HDMI_HDCP_LOG("repeater smsg done failure, hdcp2_err=%x\n",
				      mtk_hdmi_hdcp2x_get_state(hdmi));
			mtk_hdmi_hdcp_init_auth(hdmi);

			hdcp->re_repeater_done_cnt = 0;
			hdcp->hdcp_status = SV_FAIL;
		}
		break;

	case HDCP2x_AUTHEN_CHECK:
		HDMI_HDCP_LOG("HDCP2x_AUTHEN_CHECK\n");
		HDMI_HDCP_LOG("TOP_INT_STA00=0x%08x,HPD_DDC_STATUS=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS));
		HDMI_HDCP_LOG("HDCP2X_STATUS_0=0x%08x,%lums\n",
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0), jiffies);
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if ((readvalue & HDCP2X_AUTH_DONE_INT_STA) &&
		    (mtk_hdmi_hdcp2x_is_err(hdmi) == FALSE)) {
			HDMI_HDCP_LOG("[HDCP2.x]pass, %lums\n", jiffies);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_ENCRYPTION);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_AITHEN_DEALY_TIMEOUT);
			hdcp->re_auth_cnt = 0;
		} else if (((readvalue & HDCP2X_AUTH_FAIL_INT_STA) &&
			   (hdcp->re_auth_cnt != 0)) ||
			   (hdcp->re_auth_cnt > REPEAT_CHECK_AUTHHDCP_VALUE) ||
			   mtk_hdmi_hdcp2x_is_err(hdmi)) {
			HDMI_HDCP_LOG("[HDMI][HDCP2.x]authentication fail-->1, hdcp2_err=%x\n",
			     mtk_hdmi_hdcp2x_get_state(hdmi));
			mtk_hdmi_hdcp_init_auth(hdmi);
			hdcp->re_auth_cnt = 0;
			if (readvalue & HDCP2X_AUTH_FAIL_INT_STA) {
				mtk_hdmi_hdcp_clean_auth_fail_int(hdmi);
				HDMI_HDCP_LOG("hdcp2.2 authentication fail-->2\n");
			}
		} else {
			if ((readvalue & HDCP2X_AUTH_FAIL_INT_STA) &&
				(hdcp->re_auth_cnt == 0)) {
				mtk_hdmi_hdcp_clean_auth_fail_int(hdmi);
				HDMI_HDCP_LOG("hdcp2.2 authentication fail-->3\n");
			}
			hdcp->re_auth_cnt++;
			HDMI_HDCP_LOG("hdcp2.2 authentication wait=%d\n",
				      hdcp->re_auth_cnt);
			mtk_hdmi_hdcp_set_state(hdmi, HDCP2x_AUTHEN_CHECK);
			mtk_hdmi_hdcp_set_delay_time(hdmi, HDCP2x_WAIT_AUTHEN_TIMEOUT);
		}
		break;

	case HDCP2x_ENCRYPTION:
		if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_RECEIVER_NOT_READY);
		} else {
			mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_ENCRYPT_EN,
				HDCP2X_ENCRYPT_EN);
			hdcp->re_auth_cnt = 0;
			hdcp->hdcp_status = SV_OK;
			mtk_hdmi_av_unmute(hdmi);
			HDMI_HDCP_LOG("[HDCP2.x]pass, %lums\n", jiffies);
		}
		break;

	default:
		break;
	}
}

void mtk_hdmi_hdcp_read_version(struct mtk_hdmi *hdmi)
{
	unsigned char temp = 0;
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

	if (!fg_ddc_data_read(ddc, RX_ID, RX_REG_HDCP2VERSION,
		1, &temp)) {
		HDMI_HDCP_LOG("read hdcp version fail from sink\n");
		hdcp->hdcp_2x_support = false;
	} else if (temp & 0x4) {
		hdcp->hdcp_2x_support = true;
		HDMI_HDCP_LOG("sink support hdcp2.2 version\n");
	} else {
		hdcp->hdcp_2x_support = false;
		HDMI_HDCP_LOG("sink support hdcp1.x version\n");
	}

	if (hdmi->dvi_mode == true)
		hdcp->hdcp_2x_support = false;
}

void mtk_hdmi_hdcp_timer_isr(struct timer_list *timer)
{
	struct mtk_hdmi_hdcp *hdcp = from_timer(hdcp, timer, hdcp_timer);

	atomic_set(&hdcp->hdmi_hdcp_event, 1);
	wake_up_interruptible(&hdcp->hdcp_wq);
	mod_timer(&hdcp->hdcp_timer, jiffies + HDMI_HDCP_INTERVAL / (1000 / HZ));

	hdcp->hdcp_delay_time -= HDMI_HDCP_INTERVAL;
}

void mtk_hdmi_hdcp_start_task(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	memset((void *)&(hdcp->hdcp_timer), 0, sizeof(hdcp->hdcp_timer));
	timer_setup(&hdcp->hdcp_timer, mtk_hdmi_hdcp_timer_isr, 0);
	hdcp->hdcp_timer.expires = jiffies + 50 / (1000 / HZ);
	add_timer(&hdcp->hdcp_timer);
	hdcp->repeater_hdcp = false;
	mtk_hdmi_hdcp_init_auth(hdmi);
}

void mtk_hdmi_hdcp_stop_task(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->hdcp_timer.function)
		del_timer_sync(&hdcp->hdcp_timer);
	memset((void *)&hdcp->hdcp_timer, 0, sizeof(hdcp->hdcp_timer));
}

void mtk_hdmi_hdcp_task(struct mtk_hdmi *hdmi)
{
	unsigned int status;
	bool check_link = false;
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	status = mtk_hdmi_read(hdmi, TOP_INT_STA00);
	if (status & HDCP_RI_128_INT_STA) {
		/* clear ri irq */
		mtk_hdmi_mask(hdmi, TOP_INT_CLR00, HDCP_RI_128_INT_CLR,
			HDCP_RI_128_INT_CLR);
		mtk_hdmi_mask(hdmi, TOP_INT_CLR00, 0x0, HDCP_RI_128_INT_CLR);

		if ((hdcp->hdcp_ctrl_state == HDCP_WAIT_RI) ||
			(hdcp->hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY)) {
			mtk_hdmi_hdcp_set_state(hdmi, HDCP_CHECK_LINK_INTEGRITY);
			check_link = true;
		}
	}

	if (hdcp->hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY) {
		if (check_link == true)
			check_link = false;
		else
			return;
	}

	if (mtk_hdmi_hdcp_check_ctrl_timeout(hdmi))
		mtk_hdmi_hdcp_service(hdmi);
}

int mtk_hdmi_hdcp_kthread(void *data)
{
	struct mtk_hdmi *hdmi = data;
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	for (;;) {
		wait_event_interruptible(hdcp->hdcp_wq,
			atomic_read(&hdcp->hdmi_hdcp_event));
		atomic_set(&hdcp->hdmi_hdcp_event, 0);
		mtk_hdmi_hdcp_task(hdmi);
		if (kthread_should_stop())
			break;
	}
	return 0;
}

int mtk_hdmi_hdcp_create_task(struct mtk_hdmi *hdmi)
{
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	init_waitqueue_head(&hdcp->hdcp_wq);
	hdcp->hdcp_task = kthread_create(mtk_hdmi_hdcp_kthread,
					hdmi, "mtk_hdmi_hdcp_kthread");
	if (hdcp->hdcp_task == NULL) {
		pr_err("Failed to create hdcp task\n");
		return -EINVAL;
	}
	wake_up_process(hdcp->hdcp_task);

	return 0;
}

