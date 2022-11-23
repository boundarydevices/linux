/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HDMI_RX2_CTRL_H_
#define _HDMI_RX2_CTRL_H_

#include "mtk_hdmirx.h"

extern const u8 cts_4k_edid[];
extern const u8 test_4_edid[];
extern const u8 pdYMH1800Edid[];
extern const u8 vrr30to60[];
extern const u8 vrr24to60[];

void Linux_Time(unsigned long *prTime);
bool Linux_DeltaTime(unsigned long *u4OverTime,
	unsigned long *prStartT,
	unsigned long *prCurrentT);
void hdmi2_set_hpd_high(struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch);
void hdmi2_set_hpd_low(struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch);

void hdmi2_color_space_convert(struct MTK_HDMI *myhdmi);
void hdmi2_bypass_cs_convert(struct MTK_HDMI *myhdmi);
void hdmi2_rx_init(struct MTK_HDMI *myhdmi);
void hdmi2_hw_init(struct MTK_HDMI *myhdmi);
void hdmi2_tmds_term_ctrl(struct MTK_HDMI *myhdmi, bool bOnOff);
u8 hdmi2_is_un_plug_in(struct MTK_HDMI *myhdmi);
void hdmi2_un_plug_in_clr(struct MTK_HDMI *myhdmi);
void hdmi2_set_un_plug_in(struct MTK_HDMI *myhdmi);
u8 hdmi2_is_sync_lose(struct MTK_HDMI *myhdmi);
void hdmi2_set_sync_lose(struct MTK_HDMI *myhdmi, u8 u1Val);
bool hdmi2_is_420_mode(struct MTK_HDMI *myhdmi);
void hdmi2_aud_pll_select(struct MTK_HDMI *myhdmi, u8 u1On);
u8 hdmi2_get_color_depth(struct MTK_HDMI *myhdmi);
u8 hdmi2_get_hdcp1x_status(struct MTK_HDMI *myhdmi);
u8 hdmi2_get_hdcp2x_status(struct MTK_HDMI *myhdmi);
void hdmi2_stable_status_clr(struct MTK_HDMI *myhdmi);
void hdmi2_video_mute(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2_packet_data_init(struct MTK_HDMI *myhdmi);
void hdmi2_audio_reset(struct MTK_HDMI *myhdmi);
void hdmi2_aud_reset_for_verify(struct MTK_HDMI *myhdmi);
void hdmi2_show_all_status(struct MTK_HDMI *myhdmi);
void hdmi2_debug_enable(struct MTK_HDMI *myhdmi, u32 u4MessageType);
void hdmi2_debug_disable(struct MTK_HDMI *myhdmi, u32 u4MessageType);
void hdmi2_set_hdcp_receiver_mode(struct MTK_HDMI *myhdmi);
void hdmi2_set_hdcp_repeater_mode(struct MTK_HDMI *myhdmi);
void hdmi2_timer_handle(struct MTK_HDMI *myhdmi);
void hdmi2_set_hpd_low(struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch);
void hdmi2_set_hpd_high(
	struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch);
void hdmi2_phy_reset(struct MTK_HDMI *myhdmi, u8 resettype);
u8 hdmi2_get_hdmi_mode(struct MTK_HDMI *myhdmi);
void hdmi2_show_all_infoframe(struct MTK_HDMI *myhdmi);
void hdmi2_get_infoframe(struct MTK_HDMI *myhdmi);
void hdmi2_info_parser(struct MTK_HDMI *myhdmi);
void hdmi2_audio_mute(struct MTK_HDMI *myhdmi, bool bEn);
void hdmi2_hdcp_service(struct MTK_HDMI *myhdmi);
void hdmi2_state(struct MTK_HDMI *myhdmi);
u8 hdmi2_get_state(struct MTK_HDMI *myhdmi);
void hdmi2_set_state(struct MTK_HDMI *myhdmi, u8 state);
void hdmi2_hdcp_status(struct MTK_HDMI *myhdmi);
void hdmi2com_hdcp_reset(struct MTK_HDMI *myhdmi);
void hdmi2_set_digital_reset_start_time(struct MTK_HDMI *myhdmi);
bool hdmi2_is_digital_reset_timeout(
	struct MTK_HDMI *myhdmi, unsigned long delay_ms);
void hdmi2_set_video_mute(struct MTK_HDMI *myhdmi, bool mute, bool force);
void hdmi2_set_hpd_low_timer(struct MTK_HDMI *myhdmi, u8 u1TimerCnt);
void hdmi2_get_audio_info(
	struct MTK_HDMI *myhdmi, struct AUDIO_INFO *pv_get_info);
bool hdmi2_get_5v_level(struct MTK_HDMI *myhdmi);
void hdmi2_write_edid(struct MTK_HDMI *myhdmi, u8 *prData);
void hdmi_rx_set_640_480p_enable(struct MTK_HDMI *myhdmi, u8 bType);
bool hdmi2_is_debug(struct MTK_HDMI *myhdmi, u32 u4MessageType);
int hdmi2cmd_debug_init(struct MTK_HDMI *myhdmi);
void hdmi2cmd_debug_uninit(struct MTK_HDMI *myhdmi);
void hdmi2_avi_info_parser(struct MTK_HDMI *myhdmi);
void hdmi2_video_setting(struct MTK_HDMI *myhdmi);
void hdmi2_vsi_info_parser(struct MTK_HDMI *myhdmi);
void hdmi2_show_status(struct MTK_HDMI *myhdmi);
void hdmi2com_info(struct MTK_HDMI *myhdmi, u8 Type);
void hdmi_rx_task_start(struct MTK_HDMI *myhdmi, bool u1Enable);
void hdmi2_get_pkt_info(struct MTK_HDMI *myhdmi,
	u32 Type, u8 *pbuf);
bool hdmi2_is_hdmi_mode(struct MTK_HDMI *myhdmi);
u8 hdmi2_get_deep_color_status(struct MTK_HDMI *myhdmi);
void hdmi2_rx_var_init(struct MTK_HDMI *myhdmi);
void hdmi2_hdcp_set_receiver(struct MTK_HDMI *myhdmi);
void hdmi2_hdcp_set_repeater(struct MTK_HDMI *myhdmi);
void hdmi2_rx_av_mute(struct MTK_HDMI *myhdmi, bool mute, bool force);
void hdmi2_set_hdcp_auth_done(struct MTK_HDMI *myhdmi,
	bool fgTxHdcpAuthDone);
void hdmi2_rx_info(struct MTK_HDMI *myhdmi);
void txapi_rpt_mode(struct MTK_HDMI *myhdmi,
	enum hdmi_mode_setting mode);
bool txapi_get_edid(struct MTK_HDMI *myhdmi,
	u8 *p,
	u32 num,
	u32 len);
void txapi_hdcp_start(struct MTK_HDMI *myhdmi,
	bool hdcp_on);
bool txapi_hdcp_ver(struct MTK_HDMI *myhdmi);
void txapi_hdmi_enable(struct MTK_HDMI *myhdmi, bool en);
bool txapi_is_plugin(struct MTK_HDMI *myhdmi);
void txapi_signal_off(struct MTK_HDMI *myhdmi);
void txapi_send_pkt(struct MTK_HDMI *myhdmi,
	enum packet_type type,
	bool enable,
	u8 *pkt_data,
	struct HdmiInfo *pkt_p);
void txapi_cfg_video(struct MTK_HDMI *myhdmi);
void txapi_cfg_audio(struct MTK_HDMI *myhdmi, u8 aud);
#ifdef CC_SUPPORT_HDR10Plus
/**
 * @retval       Refer to HDR10Plus EMP Exist or not
 */
bool vHDMI2GetHdr10PlusEMPExist(struct MTK_HDMI *myhdmi);
/**
 * @retval       Refer to HDR10Plus VSI Exist or not
 */
bool vHDMI2GetHdr10PlusVSIExist(struct MTK_HDMI *myhdmi);
/**
 * @brief        Init HDR10Plus metadata buffer
 */
void vHDMI2InitHdr10plusMetadataBuffer(void);
/**
 * @brief Get HDR10Plus Emp metadata buffer
 * @param u2Index---- Metadata index
 * @param *prEmpMetadata---- Pointer to metadata structure HDR10Plus_EMP_T
 * @retval TRUE for valid metadata, FALSE no valid metadata
 */
bool fgHDMI2GetHdr10PlusEmpMetadataBuff(struct MTK_HDMI *myhdmi,
	u16 u2Index, struct HDR10Plus_EMP_T *prEmpMetadata);
/**
 * @brief Get HDR10Plus Vsi metadata buffer
 * @param u2Index---- Metadata index
 * @param *prVsiMetadata---- Pointer to metadata structure HDR10Plus_VSI_T
 * @retval TRUE for valid metadata, FALSE no valid metadata
 */
bool fgHDMI2GetHdr10PlusVsiMetadataBuff(struct MTK_HDMI *myhdmi,
	u16 u2Index, struct HDR10Plus_VSI_T *prVsiMetadata);
/**
 * @retval       set to HDR10Plus EMP Exist or not
 */
void vHDMI2SetHdr10PlusEmpExist(struct MTK_HDMI *myhdmi,
	bool fgEmpExist);
/**
 * @retval       HDR10+ set VSI Exist
 */
void vHDMI2SetHdr10PlusVsiExist(struct MTK_HDMI *myhdmi,
	bool fgEmpExist);
/**
 * @retval       HDR10+ parse VSI Metadata
 */
void _vHDMI2Hdr10PlusVsifParseMd(struct MTK_HDMI *myhdmi,
	struct HDR10Plus_VSI_T *prHdmiHdr10PVsiCtx,
	const struct HdmiInfo *pHdr10PVsiInfo);
/**
 * @retval       HDR10+ VSI Notify to VDP
 */
void vHDMI2HDR10PlusEmpHandler(struct MTK_HDMI *myhdmi,
	struct HDMI_HDR_EMP_MD_T *prDynamicHdrEMPktInfo);
void _vHDMI2ParseHdr10PlusVsiHandle(struct MTK_HDMI *myhdmi);
#endif
void _vHDMI20HDREMPHandler(struct MTK_HDMI *myhdmi);
void hdmi2_emp_vrr_irq(struct MTK_HDMI *myhdmi);
void hdmi2_show_emp(struct MTK_HDMI *myhdmi);
void io_get_dev_info(struct MTK_HDMI *myhdmi,
	struct HDMIRX_DEV_INFO *dev_info);
void hdmi2_get_pkt(struct MTK_HDMI *myhdmi);
void hdmi2_update_tx_port(struct MTK_HDMI *myhdmi);
u32 hdmi2_vrr_frame_rate(struct MTK_HDMI *myhdmi);
bool hdmi_rx_domain_on(struct MTK_HDMI *myhdmi);
void hdmi_rx_domain_off(struct MTK_HDMI *myhdmi);
void hdmirx_toprgu_rst(struct MTK_HDMI *myhdmi);
void io_get_hdr10_info(struct MTK_HDMI *myhdmi,
	struct hdr10InfoPkt *hdr10_info);
void hdmirx_set_vcore(struct MTK_HDMI *myhdmi, u32 vcore);

#endif
