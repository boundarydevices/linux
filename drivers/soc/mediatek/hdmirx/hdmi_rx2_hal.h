/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HDMI_RX2_HAL_H_
#define _HDMI_RX2_HAL_H_

#include "mtk_hdmirx.h"

void hdmi2com_set_output_format_level_color(struct MTK_HDMI *myhdmi,
	u8 format, u8 level, u8 color);
void hdmi2com_set_input_format_level_color(struct MTK_HDMI *myhdmi,
	u8 format, u8 level, u8 color);
void hdmi2com_set_rgb_to_ycbcr(struct MTK_HDMI *myhdmi, u8 enable);
u32 hdmi2com_v_front_porch_odd(struct MTK_HDMI *myhdmi);
u32 hdmi2com_v_back_porch_odd(struct MTK_HDMI *myhdmi);
u32 hdmi2com_v_front_porch_even(struct MTK_HDMI *myhdmi);
u32 hdmi2com_v_back_porch_even(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_aud_ch0(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_aud_ch1(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_aud_ch2(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_aud_ch3(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_aud_ch4(struct MTK_HDMI *myhdmi);
void hdmi2com_set_aud_i2s_format(
	struct MTK_HDMI *myhdmi, u8 u1fmt, u8 u1Cycle);
void hdmi2com_set_aud_i2s_lr_inv(
	struct MTK_HDMI *myhdmi, u8 u1LRInv);
void hdmi2com_set_aud_i2s_lr_ck_edge(
	struct MTK_HDMI *myhdmi, u8 u1EdgeFmt);
void hdmi2com_set_aud_i2s_mclk(
	struct MTK_HDMI *myhdmi, u8 u1MclkType);
void hdmi2com_set_aud_fs(struct MTK_HDMI *myhdmi, enum HDMI_RX_AUDIO_FS eFS);
u8 hdmi2com_get_aud_i2s_mclk(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_port1_5v_level(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_port2_5v_level(struct MTK_HDMI *myhdmi);
void hdmi2com_term_r(struct MTK_HDMI *myhdmi, u8 term_r);
u32 hdmi2com_h_total(struct MTK_HDMI *myhdmi);
u32 hdmi2com_v_total_even(struct MTK_HDMI *myhdmi);
u32 hdmi2com_v_total_odd(struct MTK_HDMI *myhdmi);
u32 hdmi2com_v_total(struct MTK_HDMI *myhdmi);
u32 hdmi2com_h_active(struct MTK_HDMI *myhdmi);
u32 hdmi2com_v_active(struct MTK_HDMI *myhdmi);
u32 hdmi2com_frame_rate(struct MTK_HDMI *myhdmi);
u32 hdmi2com_pixel_clk(struct MTK_HDMI *myhdmi);
bool hdmi2com_ana_crc(struct MTK_HDMI *myhdmi, u16 u2Times);
bool hdmi2com_crc(struct MTK_HDMI *myhdmi, u16 u2Times);
bool hdmi2com_is_interlace(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_av_mute(struct MTK_HDMI *myhdmi);
void hdmi2com_av_mute_clr(struct MTK_HDMI *myhdmi, u8 u1En);
bool hdmi2com_is_hdmi_mode(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_aud_mute(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_scrambing_by_scdc(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_hw_scrambing_en(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_tmds_clk_radio_40x(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_ckdt_1(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_scdt_1(struct MTK_HDMI *myhdmi);
u32 hdmi2com_cts_of_ncts(struct MTK_HDMI *myhdmi);
u32 hdmi2com_n_of_ncts(struct MTK_HDMI *myhdmi);
u8 hdmi2com_aud_fs(struct MTK_HDMI *myhdmi);
bool hdmi2com_get_aud_layout(struct MTK_HDMI *myhdmi);
enum HdmiRxDepth hdmi2com_get_deep_status(struct MTK_HDMI *myhdmi);
void hdmi2com_force_scrambing_on_by_sw(
	struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_set_420_output(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_set_420_to_422(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_set_422_to_444(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_aud_fifo_rst(struct MTK_HDMI *myhdmi);
void hdmi2com_aud_acr_rst(struct MTK_HDMI *myhdmi);
void hdmi2com_deep_rst1(struct MTK_HDMI *myhdmi);
void hdmi2com_deep_rst(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_dig_phy_rst1(struct MTK_HDMI *myhdmi);
void hdmi2com_dig_phy_rst(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_hdcp1x_rst(struct MTK_HDMI *myhdmi);
void hdmi2com_hdcp2x_rst(struct MTK_HDMI *myhdmi);
void hdmi2com_hdcp_init(struct MTK_HDMI *myhdmi);
void hdmi2com_rx_init(struct MTK_HDMI *myhdmi);
void hdmi2com_sw_reset(struct MTK_HDMI *myhdmi);
void hdmi2com_clr_int(struct MTK_HDMI *myhdmi);
void hdmi2com_set_420_to_444(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_show_video_status(struct MTK_HDMI *myhdmi);
void hdmi2com_show_audio_status(struct MTK_HDMI *myhdmi);
u32 hdmi2com_hdcp1x_status(struct MTK_HDMI *myhdmi);
u32 hdmi2com_hdcp2x_status(struct MTK_HDMI *myhdmi);
void hdmi2com_get_video_info_by_avi(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_420_mode(struct MTK_HDMI *myhdmi);
void hdmi2com_audio_pll_sel(struct MTK_HDMI *myhdmi, u8 u1Pll);
void hdmi2com_aud_mute_set(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_aud_mute_mode(struct MTK_HDMI *myhdmi, bool dsd);
void hdmi2com_vid_mute_set(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_vid_black_set(struct MTK_HDMI *myhdmi, u8 u1IsRgb);
void hdmi2com_audio_config(struct MTK_HDMI *myhdmi);
void hdmi2com_set_scdc_en(struct MTK_HDMI *myhdmi, u8 u1En);
void hdmi2com_scdc_status_clr(struct MTK_HDMI *myhdmi);
u32 hdmi2com_h_clk(struct MTK_HDMI *myhdmi);
void hdmi2com_aac_en(struct MTK_HDMI *myhdmi, u8 u1En);
u8 hdmi2com_get_aud_fifo_status(struct MTK_HDMI *myhdmi);
void hdmi2com_aud_acr_setting(struct MTK_HDMI *myhdmi);
u32 hdmi2com_tmds_clk_freq(struct MTK_HDMI *myhdmi);
u8 hdmi2com_aud_fs_cal(struct MTK_HDMI *myhdmi);
void hdmi2com_fs_sw_mode(struct MTK_HDMI *myhdmi,
	unsigned int mode, unsigned int fs);
void hdmi2com_hbr_sw_mode(struct MTK_HDMI *myhdmi,
	unsigned int mode);
void hdmi2com_aud_apll_sel(struct MTK_HDMI *myhdmi, u8 u1On);
void hdmi2com_aud_dacr_rst(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_h_res_stb(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_v_res_stb(struct MTK_HDMI *myhdmi);
void hdmi2com_ana_all_reset(bool b40xMode);
void hdmi2com_ana_main_pll_src_sel(struct MTK_HDMI *myhdmi);
void hdmi2com_hdcp2x_pupd(struct MTK_HDMI *myhdmi);
bool hdmi2com_get_aud_pll_ctrl(struct MTK_HDMI *myhdmi);
u32 hdmi2com_get_ref_clk(struct MTK_HDMI *myhdmi);

void hdmi2com_set_hpd_low(
	struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch);
void hdmi2com_is_hpd_high(
	struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch);
bool hdmi2com_get_hpd_level(
	struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch);
void hdmi2com_audio_pll_config(struct MTK_HDMI *myhdmi,
	bool enable);
u32 hdmi2com_get_aud_err(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_aud_fs(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_hd_audio(struct MTK_HDMI *myhdmi);
void hdmi2com_set_aud_valid_channel(
	struct MTK_HDMI *myhdmi, u8 u1ValidCh);
void hdmi2com_audio_fifo_reset(struct MTK_HDMI *myhdmi);
void hdmi2com_set_aud_mute_channel(
	struct MTK_HDMI *myhdmi, u8 u1MuteCh);
bool hdmi2com_is_multi_pcm_aud(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_audio_packet(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_dsd_audio(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_aud_valid_channel(struct MTK_HDMI *myhdmi);
bool hdmi2com_get_aud_infoframe(
	struct MTK_HDMI *myhdmi, union Audio_InfoFrame *pAudioInfoFrame);
void hdmi2com_switch_port_sel(struct MTK_HDMI *myhdmi, u8 bPort);
void hdmi2com_ddc_path_config(struct MTK_HDMI *myhdmi, bool enable);
bool hdmi2com_upstream_is_hdcp2x_device(struct MTK_HDMI *myhdmi);
void hdmi2com_set_hdcp1x_rpt_mode(struct MTK_HDMI *myhdmi, bool bEn);
void hdmi2com_set_hdcp2x_rpt_mode(struct MTK_HDMI *myhdmi, bool bEn);
void hdmi2com_420_2x_clk_config(struct MTK_HDMI *myhdmi, u8 flag);
void hdmi2com_reg_spy(struct MTK_HDMI *myhdmi, u32 mode);
void hdmi2com_pad_init(struct MTK_HDMI *myhdmi);
void hdmi2com_freq_test(struct MTK_HDMI *myhdmi, u32 source,
	u32 mode, u32 select);
void hdmi2com_force_scdc(struct MTK_HDMI *myhdmi, u32 mode);
void hdmi2com_phy(struct MTK_HDMI *myhdmi, u32 item);
u32 hdmi2com_check_isr(struct MTK_HDMI *myhdmi);
u32 hdmi2com_check_isr130(struct MTK_HDMI *myhdmi);
void hdmi2com_set_isr_enable(
	struct MTK_HDMI *myhdmi, bool enable, u32 en_mark);
void hdmi2com_check_hdcp1x_status(struct MTK_HDMI *myhdmi);
void hdmi2_show_ksv_list(struct MTK_HDMI *myhdmi);
u8 hdmi2com_info_header(struct MTK_HDMI *myhdmi, u8 Type);
void hdmi2com_clear_info_header(struct MTK_HDMI *myhdmi, u8 Type);
void hdmi2com_infoframe_init(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_vsync_detected(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_vid_clk_changed(struct MTK_HDMI *myhdmi);
void hdmi2com_vid_clk_changed_clr(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_pdt_clk_changed(struct MTK_HDMI *myhdmi);
void hdmi2com_set_hv_sync_pol(struct MTK_HDMI *myhdmi, bool is_hd);
u32 hdmi2com_v_res_count(struct MTK_HDMI *myhdmi);
u32 hdmi2com_h_res_count(struct MTK_HDMI *myhdmi);
u32 hdmi2com_xclk_of_pclk(struct MTK_HDMI *myhdmi);
u32 hdmi2com_xclk_of_pdt_clk(struct MTK_HDMI *myhdmi);
void hdmi2com_path_config(struct MTK_HDMI *myhdmi);
u32 hdmi2com_value_diff(u32 a, u32 b);
void hdmi2com_timing_changed_irq_clr(struct MTK_HDMI *myhdmi);
u32 hdmi2com_h_de_changed(struct MTK_HDMI *myhdmi);
u32 hdmi2com_h_total_changed(struct MTK_HDMI *myhdmi);
void hdmi2com_get_aksv(struct MTK_HDMI *myhdmi, u8 *pbuf);
void hdmi2com_get_bksv(struct MTK_HDMI *myhdmi, u8 *pbuf);
void hdmi2com_get_an(struct MTK_HDMI *myhdmi, u8 *pbuf);
bool hdmi2com_is_hdcp1x_auth_start(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_hdcp1x_auth_done(struct MTK_HDMI *myhdmi);
u16 hdmi2com_get_ri(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_hdcp2x_auth_start(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_hdcp2x_auth_done(struct MTK_HDMI *myhdmi);
u32 hdmi2com_h_front_porch(struct MTK_HDMI *myhdmi);
u32 hdmi2com_h_back_porch(struct MTK_HDMI *myhdmi);
bool hdmi2com_get_hdcp1x_rpt_mode(struct MTK_HDMI *myhdmi, bool log);
bool hdmi2com_get_hdcp2x_rpt_mode(struct MTK_HDMI *myhdmi, bool log);
bool hdmi2com_is_hdcp2x_rpt_mode(struct MTK_HDMI *myhdmi);
u8 hdmi2com_hdcp2x_state(struct MTK_HDMI *myhdmi);
void hdmi2com_check_hdcp2x_irq(struct MTK_HDMI *myhdmi);
void hdmi2com_hdcp2x_irq_clr(struct MTK_HDMI *myhdmi);
u32 hdmi2com_get_stream_id_type(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_h_unstable(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_v_unstable(struct MTK_HDMI *myhdmi);
u8 hdmi2com_is_420_clk_config(struct MTK_HDMI *myhdmi);
u8 hdmi2com_check_hdcp1x_decrypt_on(struct MTK_HDMI *myhdmi);
u8 hdmi2com_check_hdcp2x_decrypt_on(struct MTK_HDMI *myhdmi);
void hdmi2com_deep_color_auto_en(
	struct MTK_HDMI *myhdmi, bool en, u32 deep);
void hdmi2com_vid_mute_set_force(struct MTK_HDMI *myhdmi, bool en);
void hdmi2com_show_sram_edid(struct MTK_HDMI *myhdmi, u8 block_num);
void hdmi2com_write_edid_to_sram(struct MTK_HDMI *myhdmi,
	u8 block_num, u8 *pu1Edid);
void hdmi2com_set_rx_port1_hpd_level(struct MTK_HDMI *myhdmi, bool fgHighLevel);
bool hdmi2com_get_rx_port1_hpd_level(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_hdcp2x_ecc_out_of_sync(struct MTK_HDMI *myhdmi);
void hdmi2com_clear_hdcp2x_accm(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_hdcp1x_decrypt_on(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_hdcp2x_decrypt_on(struct MTK_HDMI *myhdmi);
void hdmi2com_disable_hdcp2x(struct MTK_HDMI *myhdmi, bool disable);
u8 hdmi2com_get_itc_content(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_itc_flag(struct MTK_HDMI *myhdmi);
u8 hdmi2com_is_422_input(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_afd_info(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_aspect_ratio(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_scan_info(struct MTK_HDMI *myhdmi);
void hdmi2com_info_status_clr(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_acr_lock(struct MTK_HDMI *myhdmi);
void hdmi2com_aud_fifo_err_clr(struct MTK_HDMI *myhdmi);
u8 hdmi2com_get_clocrimetry(struct MTK_HDMI *myhdmi);
u8 hdmi2com_input_color_space_type(struct MTK_HDMI *myhdmi);
u32 hdmi2com_get_pixel_rpt(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_pixel_rpt(struct MTK_HDMI *myhdmi);
void hdmi2com_set_color_ralated(struct MTK_HDMI *myhdmi);
bool hdmi2com_is_video_mute(struct MTK_HDMI *myhdmi);
void hdmi2com_set_hdcp1_rpt(struct MTK_HDMI *myhdmi,
	struct RxHDCPRptData *pdata);
bool hdmi2com_is_hdcp1_vready(struct MTK_HDMI *myhdmi);
void hdmi2com_set_hdcp1_vready(struct MTK_HDMI *myhdmi, bool is_ready);
void hdmi2com_hdcp2x_req(struct MTK_HDMI *myhdmi, u8 en);
void hdmi2com_clear_receiver_id_setting(struct MTK_HDMI *myhdmi);
void hdmi2com_set_hdcp2_rpt(struct MTK_HDMI *myhdmi,
	struct RxHDCPRptData *pdata);
void hdmi2com_hdcp2_reset(struct MTK_HDMI *myhdmi);
u32 hdmi2com_bus_clk_freq(struct MTK_HDMI *myhdmi);
void hdmi2com_cfg_out(struct MTK_HDMI *myhdmi);
u32 hdmi2com_ana_get_crcvalue(struct MTK_HDMI *myhdmi);
bool hdmi2com_ana_get_crcfail(struct MTK_HDMI *myhdmi);
bool hdmi2com_ana_get_crcready(struct MTK_HDMI *myhdmi);
u32 hdmi2com_dig_get_crcvalue(struct MTK_HDMI *myhdmi);
bool hdmi2com_dig_get_crcfail(struct MTK_HDMI *myhdmi);
bool hdmi2com_dig_get_crcready(struct MTK_HDMI *myhdmi);
void vHDMI2ComDynamicMetaDataEn(struct MTK_HDMI *myhdmi,
	u8 u1En);
void vHDMI2ComDynamicMetaDataClrInt(struct MTK_HDMI *myhdmi,
	u8 u1Fifo);
void vHDMI2ComGetDynamicMetadata(struct MTK_HDMI *myhdmi,
	u8 u1Len,
	struct HDMI_HDR_EMP_MD_T *prDynamicMetadata);
void vHDMI2ComHDR10PlusVsiEn(struct MTK_HDMI *myhdmi);
void vHDMI2ComGetHDR10PlusVsiMetadata(struct MTK_HDMI *myhdmi,
	struct HdmiInfo *prVsiMetadata);
u32 dwHDMI2ComGetBits(struct HDMI_HDR_EMP_MD_T *prHdmi_DyMdPKT, u8 u1Bits);
void delay_ms(u32 delay);
u32 hdmi_fmeter(u8 sel);
void hdmi2com_vrr_emp(struct MTK_HDMI *myhdmi);
void vHDMI2ComDoviEdrVsiEn(struct MTK_HDMI *myhdmi);
u32 hdmi2com_bch_err(struct MTK_HDMI *myhdmi);
void hdmi2com_clear_ced(struct MTK_HDMI *myhdmi);
u32 hdmi2com_get_ced(struct MTK_HDMI *myhdmi);
void hdmi2com_rst_div(struct MTK_HDMI *myhdmi);

#endif
