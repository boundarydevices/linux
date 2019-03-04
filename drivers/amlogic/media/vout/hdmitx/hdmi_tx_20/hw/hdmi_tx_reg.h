/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hdmi_tx_reg.h
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

#ifndef _HDMI_TX_REG_H
#define _HDMI_TX_REG_H

/* Use the following functions to access the on-chip HDMITX modules
 * by default
 */
void hdmitx_wr_reg(unsigned int addr, unsigned int data);
void hdmitx_poll_reg(unsigned int addr, unsigned int val,
	unsigned long timeout);
void hdmitx_set_reg_bits(unsigned int addr, unsigned int value,
	unsigned int offset, unsigned int len);
unsigned int hdmitx_rd_reg(unsigned int addr);
void hdmitx_rd_check_reg(unsigned int addr, unsigned int exp_data,
	unsigned int mask);
unsigned long aocec_rd_reg(unsigned long addr);
void aocec_wr_reg(unsigned long addr, unsigned long data);
int hdmitx_hdcp_opr(unsigned int val);

/* TOP-level wrapper registers addresses
 * bit24: 1 means secure access
 * bit28: 1 means DWC, 0 means TOP
 */
#define SEC_OFFSET           (0x1UL << 24)
#define TOP_OFFSET_MASK      (0x0UL << 24)
#define TOP_SEC_OFFSET_MASK  ((TOP_OFFSET_MASK) | (SEC_OFFSET))
#define DWC_OFFSET_MASK      (0x10UL << 24)
#define DWC_SEC_OFFSET_MASK  ((DWC_OFFSET_MASK) | (SEC_OFFSET))
#define HDMITX_DWC_BASE_OFFSET  0xFF600000
#define HDMITX_TOP_BASE_OFFSET  0xFF608000

/* Bit 7 RW Reserved. Default 1.
 * Bit 6 RW Reserved. Default 1.
 * Bit 5 RW Reserved. Default 1.
 * Bit 4 RW sw_reset_phyif: PHY interface. 1=Apply reset; 0=Release from reset.
 *     Default 1.
 * Bit 3 RW sw_reset_intr:  interrupt module. 1=Apply reset;
 *     0=Release from reset. Default 1.
 * Bit 2 RW sw_reset_mem:   KSV/REVOC mem. 1=Apply reset; 0=Release from reset.
 *     Default 1.
 * Bit 1 RW sw_reset_rnd:   random number interface to HDCP. 1=Apply reset;
 *     0=Release from reset. Default 1.
 * Bit 0 RW sw_reset_core: connects to IP's ~irstz. 1=Apply reset;
 *     0=Release from reset. Default 1.
 */
#define HDMITX_TOP_SW_RESET                     (TOP_OFFSET_MASK + 0x000)

/* Bit 12 RW i2s_ws_inv:1=Invert i2s_ws; 0=No invert. Default 0. */
/* Bit 11 RW i2s_clk_inv: 1=Invert i2s_clk; 0=No invert. Default 0. */
/* Bit 10 RW spdif_clk_inv: 1=Invert spdif_clk; 0=No invert. Default 0. */
/* Bit 9 RW tmds_clk_inv: 1=Invert tmds_clk; 0=No invert. Default 0. */
/* Bit 8 RW pixel_clk_inv: 1=Invert pixel_clk; 0=No invert. Default 0. */
/* Bit 4 RW cec_clk_en: 1=enable cec_clk; 0=disable. Default 0. */
/* Bit 3 RW i2s_clk_en: 1=enable i2s_clk; 0=disable. Default 0. */
/* Bit 2 RW spdif_clk_en: 1=enable spdif_clk; 0=disable. Default 0. */
/* Bit 1 RW tmds_clk_en: 1=enable tmds_clk;  0=disable. Default 0. */
/* Bit 0 RW pixel_clk_en: 1=enable pixel_clk; 0=disable. Default 0. */
#define HDMITX_TOP_CLK_CNTL                     (TOP_OFFSET_MASK + 0x001)

/* Bit 11: 0 RW hpd_valid_width: filter out width <= M*1024.    Default 0. */
/* Bit 15:12 RW hpd_glitch_width: filter out glitch <= N.       Default 0. */
#define HDMITX_TOP_HPD_FILTER                   (TOP_OFFSET_MASK + 0x002)

/* intr_maskn: MASK_N, one bit per interrupt source.
 *     1=Enable interrupt source; 0=Disable interrupt source. Default 0.
 * [  8] hdcp_topology_err
 * [  7] rxsense_fall
 * [  6] rxsense_rise
 * [  5] err_i2c_timeout
 * [  4] hdcp22_rndnum_err
 * [  3] nonce_rfrsh_rise
 * [  2] hpd_fall_intr
 * [  1] hpd_rise_intr
 * [  0] core_intr
 */
#define HDMITX_TOP_INTR_MASKN                   (TOP_OFFSET_MASK + 0x003)

/* Bit 30: 0 RW intr_stat: For each bit, write 1 to manually set the interrupt
 *     bit, read back the interrupt status.
 * Bit    31 R  IP interrupt status
 * Bit     2 RW hpd_fall
 * Bit     1 RW hpd_rise
 * Bit     0 RW IP interrupt
 */
#define HDMITX_TOP_INTR_STAT                    (TOP_OFFSET_MASK + 0x004)

/* [4]	  hdcp22_rndnum_err */
/* [3]	  nonce_rfrsh_rise */
/* [2]	  hpd_fall */
/* [1]	  hpd_rise */
/* [0]	  core_intr_rise */
#define HDMITX_TOP_INTR_STAT_CLR                (TOP_OFFSET_MASK + 0x005)

/* Bit 14:12 RW tmds_sel: 3'b000=Output zero; 3'b001=Output normal TMDS data;
 *     3'b010=Output PRBS data; 3'b100=Output shift pattern.         Default 0.
 * Bit 11: 9 RW shift_pttn_repeat: 0=New pattern every clk cycle; 1=New pattern
 *     every 2 clk cycles; ...; 7=New pattern every 8 clk cycles.  Default 0.
 * Bit 8 RW shift_pttn_en: 1= Eanble shift pattern generator; 0=Disable.
 *     Default 0.
 * Bit 4: 3 RW prbs_pttn_mode: 0=PRBS11; 1=PRBS15; 2=PRBS7; 3=PRBS31. Default 0.
 * Bit 2: 1 RW prbs_pttn_width: 0=idle; 1=output 8-bit pattern;
 *     2=Output 1-bit pattern; 3=output 10-bit pattern. Default 0.
 * Bit 0 RW prbs_pttn_en: 1=Enable PRBS generator; 0=Disable. Default 0.
 */
#define HDMITX_TOP_BIST_CNTL                    (TOP_OFFSET_MASK + 0x006)

/* Bit 29:20 RW shift_pttn_data[59:50]. Default 0. */
/* Bit 19:10 RW shift_pttn_data[69:60]. Default 0. */
/* Bit  9: 0 RW shift_pttn_data[79:70]. Default 0. */
#define HDMITX_TOP_SHIFT_PTTN_012               (TOP_OFFSET_MASK + 0x007)

/* Bit 29:20 RW shift_pttn_data[29:20]. Default 0. */
/* Bit 19:10 RW shift_pttn_data[39:30]. Default 0. */
/* Bit  9: 0 RW shift_pttn_data[49:40]. Default 0. */
#define HDMITX_TOP_SHIFT_PTTN_345               (TOP_OFFSET_MASK + 0x008)

/* Bit 19:10 RW shift_pttn_data[ 9: 0]. Default 0. */
/* Bit  9: 0 RW shift_pttn_data[19:10]. Default 0. */
#define HDMITX_TOP_SHIFT_PTTN_67                (TOP_OFFSET_MASK + 0x009)

/* Bit 25:16 RW tmds_clk_pttn[19:10]. Default 0. */
/* Bit  9: 0 RW tmds_clk_pttn[ 9: 0]. Default 0. */
#define HDMITX_TOP_TMDS_CLK_PTTN_01             (TOP_OFFSET_MASK + 0x00A)

/* Bit 25:16 RW tmds_clk_pttn[39:30]. Default 0. */
/* Bit  9: 0 RW tmds_clk_pttn[29:20]. Default 0. */
#define HDMITX_TOP_TMDS_CLK_PTTN_23             (TOP_OFFSET_MASK + 0x00B)

/* Bit 1 RW shift_tmds_clk_pttn:1=Enable shifting clk pattern,
 * used when TMDS CLK rate = TMDS character rate /4.    Default 0.
 * Bit 0 R  Reserved. Default 0.
 */
/* [	1] shift_tmds_clk_pttn */
/* [	0] load_tmds_clk_pttn */
#define HDMITX_TOP_TMDS_CLK_PTTN_CNTL           (TOP_OFFSET_MASK + 0x00C)

/* Bit 0 RW revocmem_wr_fail: Read back 1 to indicate Host write REVOC MEM
 * failure, write 1 to clear the failure flag.  Default 0.
 */
#define HDMITX_TOP_REVOCMEM_STAT                (TOP_OFFSET_MASK + 0x00D)

/* Bit     0 R  filtered HPD status. */
#define HDMITX_TOP_STAT0                        (TOP_OFFSET_MASK + 0x00E)
#define HDMITX_TOP_SKP_CNTL_STAT                (TOP_SEC_OFFSET_MASK + 0x010)
#define HDMITX_TOP_NONCE_0                      (TOP_SEC_OFFSET_MASK + 0x011)
#define HDMITX_TOP_NONCE_1                      (TOP_SEC_OFFSET_MASK + 0x012)
#define HDMITX_TOP_NONCE_2                      (TOP_SEC_OFFSET_MASK + 0x013)
#define HDMITX_TOP_NONCE_3                      (TOP_SEC_OFFSET_MASK + 0x014)
#define HDMITX_TOP_PKF_0                        (TOP_SEC_OFFSET_MASK + 0x015)
#define HDMITX_TOP_PKF_1                        (TOP_SEC_OFFSET_MASK + 0x016)
#define HDMITX_TOP_PKF_2                        (TOP_SEC_OFFSET_MASK + 0x017)
#define HDMITX_TOP_PKF_3                        (TOP_SEC_OFFSET_MASK + 0x018)
#define HDMITX_TOP_DUK_0                        (TOP_SEC_OFFSET_MASK + 0x019)
#define HDMITX_TOP_DUK_1                        (TOP_SEC_OFFSET_MASK + 0x01A)
#define HDMITX_TOP_DUK_2                        (TOP_SEC_OFFSET_MASK + 0x01B)
#define HDMITX_TOP_DUK_3                        (TOP_SEC_OFFSET_MASK + 0x01C)
/* [26:24] infilter_ddc_intern_clk_divide */
/* [23:16] infilter_ddc_sample_clk_divide */
/* [10: 8] infilter_cec_intern_clk_divide */
/* [ 7: 0] infilter_cec_sample_clk_divide */
#define HDMITX_TOP_INFILTER                     (TOP_OFFSET_MASK + 0x01D)
#define HDMITX_TOP_NSEC_SCRATCH                 (TOP_OFFSET_MASK + 0x01E)
#define HDMITX_TOP_SEC_SCRATCH                  (TOP_SEC_OFFSET_MASK + 0x01F)
#define HDMITX_TOP_EMP_CNTL0                    (TOP_OFFSET_MASK + 0x020)
#define HDMITX_TOP_EMP_CNTL1                    (TOP_OFFSET_MASK + 0x021)
#define HDMITX_TOP_EMP_MEMADDR_START            (TOP_OFFSET_MASK + 0x022)
#define HDMITX_TOP_EMP_STAT0                    (TOP_OFFSET_MASK + 0x023)
#define HDMITX_TOP_EMP_STAT1                    (TOP_OFFSET_MASK + 0x024)
#define HDMITX_TOP_AXI_ASYNC_CNTL0              (TOP_OFFSET_MASK + 0x025)
#define HDMITX_TOP_AXI_ASYNC_CNTL1              (TOP_OFFSET_MASK + 0x026)
#define HDMITX_TOP_AXI_ASYNC_STAT0              (TOP_OFFSET_MASK + 0x027)
#define HDMITX_TOP_I2C_BUSY_CNT_MAX             (TOP_OFFSET_MASK + 0x028)
#define HDMITX_TOP_I2C_BUSY_CNT_STAT            (TOP_OFFSET_MASK + 0x029)
#define HDMITX_TOP_HDCP22_BSOD                  (TOP_SEC_OFFSET_MASK + 0x02A)
#define HDMITX_TOP_DDC_CNTL                     (TOP_OFFSET_MASK + 0x02B)
#define HDMITX_TOP_DISABLE_NULL                     (TOP_OFFSET_MASK + 0x030)
#define HDMITX_TOP_REVOCMEM_ADDR_S              (TOP_OFFSET_MASK + 0x2000 >> 2)
#define HDMITX_TOP_REVOCMEM_ADDR_E              (TOP_OFFSET_MASK + 0x365E >> 2)

#define HDMITX_TOP_DONT_TOUCH0                  (TOP_OFFSET_MASK + 0x0FE)
#define HDMITX_TOP_DONT_TOUCH1                  (TOP_OFFSET_MASK + 0x0FF)

/* DWC_HDMI_TX Controller registers addresses */

/* Identification Registers */
#define HDMITX_DWC_DESIGN_ID                    (DWC_OFFSET_MASK + 0x0000)
#define HDMITX_DWC_REVISION_ID                  (DWC_OFFSET_MASK + 0x0001)
#define HDMITX_DWC_PRODUCT_ID0                  (DWC_OFFSET_MASK + 0x0002)
#define HDMITX_DWC_PRODUCT_ID1                  (DWC_OFFSET_MASK + 0x0003)
#define HDMITX_DWC_CONFIG0_ID                   (DWC_OFFSET_MASK + 0x0004)
#define HDMITX_DWC_CONFIG1_ID                   (DWC_OFFSET_MASK + 0x0005)
#define HDMITX_DWC_CONFIG2_ID                   (DWC_OFFSET_MASK + 0x0006)
#define HDMITX_DWC_CONFIG3_ID                   (DWC_OFFSET_MASK + 0x0007)

/* Interrupt Registers */
#define HDMITX_DWC_IH_FC_STAT0                  (DWC_OFFSET_MASK + 0x0100)
#define HDMITX_DWC_IH_FC_STAT1                  (DWC_OFFSET_MASK + 0x0101)
#define HDMITX_DWC_IH_FC_STAT2                  (DWC_OFFSET_MASK + 0x0102)
#define HDMITX_DWC_IH_AS_STAT0                  (DWC_OFFSET_MASK + 0x0103)
#define HDMITX_DWC_IH_PHY_STAT0                 (DWC_OFFSET_MASK + 0x0104)
#define HDMITX_DWC_IH_I2CM_STAT0                (DWC_OFFSET_MASK + 0x0105)
#define HDMITX_DWC_IH_CEC_STAT0                 (DWC_OFFSET_MASK + 0x0106)
#define HDMITX_DWC_IH_VP_STAT0                  (DWC_OFFSET_MASK + 0x0107)
#define HDMITX_DWC_IH_I2CMPHY_STAT0             (DWC_OFFSET_MASK + 0x0108)
#define HDMITX_DWC_IH_DECODE                    (DWC_OFFSET_MASK + 0x0170)
/* [  7] mute_AUDI */
/* [  6] mute_ACP */
/* [  5] mute_HBR */
/* [  4] mute_MAS */
/* [  3] mute_NVBI */
/* [  2] mute_AUDS */
/* [  1] mute_ACR */
/* [  0] mute_NULL */
#define HDMITX_DWC_IH_MUTE_FC_STAT0             (DWC_OFFSET_MASK + 0x0180)
/* [  7] mute_GMD */
/* [  6] mute_ISRC1 */
/* [  5] mute_ISRC2 */
/* [  4] mute_VSD */
/* [  3] mute_SPD */
/* [  2] mute_AMP */
/* [  1] mute_AVI */
/* [  0] mute_GCP */
#define HDMITX_DWC_IH_MUTE_FC_STAT1             (DWC_OFFSET_MASK + 0x0181)
/* [  1] mute_LowPriority_fifo_full */
/* [  0] mute_HighPriority_fifo_full */
#define HDMITX_DWC_IH_MUTE_FC_STAT2             (DWC_OFFSET_MASK + 0x0182)
/* [  4] mute_aud_fifo_underrun */
/* [  3] mute_aud_fifo_overrun */
/* [  2] mute_aud_fifo_empty_thr. oififoemptythr tied to 0. */
/* [  1] mute_aud_fifo_empty */
/* [  0] mute_aud_fifo_full */
#define HDMITX_DWC_IH_MUTE_AS_STAT0             (DWC_OFFSET_MASK + 0x0183)
#define HDMITX_DWC_IH_MUTE_PHY_STAT0            (DWC_OFFSET_MASK + 0x0184)
/* [  2] mute_scdc_readreq */
/* [  1] mute_edid_i2c_master_done */
/* [  0] mute_edid_i2c_master_error */
#define HDMITX_DWC_IH_MUTE_I2CM_STAT0           (DWC_OFFSET_MASK + 0x0185)
/* [  6] cec_wakeup */
/* [  5] cec_error_follower */
/* [  4] cec_error_initiator */
/* [  3] cec_arb_lost */
/* [  2] cec_nack */
/* [  1] cec_eom */
/* [  0] cec_done */
#define HDMITX_DWC_IH_MUTE_CEC_STAT0            (DWC_OFFSET_MASK + 0x0186)
#define HDMITX_DWC_IH_MUTE_VP_STAT0             (DWC_OFFSET_MASK + 0x0187)
#define HDMITX_DWC_IH_MUTE_I2CMPHY_STAT0        (DWC_OFFSET_MASK + 0x0188)
/* [  1] mute_wakeup_interrupt */
/* [  0] mute_all_interrupt */
#define HDMITX_DWC_IH_MUTE                      (DWC_OFFSET_MASK + 0x01FF)

/* Video Sampler Registers */
/* [  7] internal_de_generator */
/* [4:0] video_mapping */
#define HDMITX_DWC_TX_INVID0                    (DWC_OFFSET_MASK + 0x0200)
/* [  2] bcbdata_stuffing */
/* [  1] rcrdata_stuffing */
/* [  0] gydata_stuffing */
#define HDMITX_DWC_TX_INSTUFFING                (DWC_OFFSET_MASK + 0x0201)
#define HDMITX_DWC_TX_GYDATA0                   (DWC_OFFSET_MASK + 0x0202)
#define HDMITX_DWC_TX_GYDATA1                   (DWC_OFFSET_MASK + 0x0203)
#define HDMITX_DWC_TX_RCRDATA0                  (DWC_OFFSET_MASK + 0x0204)
#define HDMITX_DWC_TX_RCRDATA1                  (DWC_OFFSET_MASK + 0x0205)
#define HDMITX_DWC_TX_BCBDATA0                  (DWC_OFFSET_MASK + 0x0206)
#define HDMITX_DWC_TX_BCBDATA1                  (DWC_OFFSET_MASK + 0x0207)

/* Video Packetizer Registers */
#define HDMITX_DWC_VP_STATUS                    (DWC_OFFSET_MASK + 0x0800)
/* [3:0] desired_pr_factor */
#define HDMITX_DWC_VP_PR_CD                     (DWC_OFFSET_MASK + 0x0801)
/* [  5] default_phase */
/* [  2] ycc422_stuffing */
/* [  1] pp_stuffing */
/* [  0] pr_stuffing */
#define HDMITX_DWC_VP_STUFF                     (DWC_OFFSET_MASK + 0x0802)
#define HDMITX_DWC_VP_REMAP                     (DWC_OFFSET_MASK + 0x0803)
#define HDMITX_DWC_VP_CONF                      (DWC_OFFSET_MASK + 0x0804)
/* [  7] mask_int_full_prpt */
/* [  6] mask_int_empty_prpt */
/* [  5] mask_int_full_ppack */
/* [  4] mask_int_empty_ppack */
/* [  3] mask_int_full_remap */
/* [  2] mask_int_empty_remap */
/* [  1] mask_int_full_byp */
/* [  0] mask_int_empty_byp */
#define HDMITX_DWC_VP_MASK                      (DWC_OFFSET_MASK + 0x0807)

/* Frmae Composer Registers */
/* [  7] HDCP_keepout */
/* [  6] vs_in_pol: 0=active low; 1=active high. */
/* [  5] hs_in_pol: 0=active low; 1=active high. */
/* [  4] de_in_pol: 0=active low; 1=active high. */
/* [  3] dvi_modez: 0=dvi; 1=hdmi. */
/* [  1] r_v_blank_in_osc */
/* [  0] in_I_P: 0=progressive; 1=interlaced. */
#define HDMITX_DWC_FC_INVIDCONF                 (DWC_OFFSET_MASK + 0x1000)
/* [7:0] H_in_active[7:0] */
#define HDMITX_DWC_FC_INHACTV0                  (DWC_OFFSET_MASK + 0x1001)
/* [5:0] H_in_active[13:8] */
#define HDMITX_DWC_FC_INHACTV1                  (DWC_OFFSET_MASK + 0x1002)
/* [7:0] H_in_blank[7:0] */
#define HDMITX_DWC_FC_INHBLANK0                 (DWC_OFFSET_MASK + 0x1003)
/* [4:0] H_in_blank[12:8] */
#define HDMITX_DWC_FC_INHBLANK1                 (DWC_OFFSET_MASK + 0x1004)
/* [7:0] V_in_active[7:0] */
#define HDMITX_DWC_FC_INVACTV0                  (DWC_OFFSET_MASK + 0x1005)
/* [4:0] V_in_active[12:8] */
#define HDMITX_DWC_FC_INVACTV1                  (DWC_OFFSET_MASK + 0x1006)
/* [7:0] V_in_blank */
#define HDMITX_DWC_FC_INVBLANK                  (DWC_OFFSET_MASK + 0x1007)
/* [7:0] H_in_delay[7:0] */
#define HDMITX_DWC_FC_HSYNCINDELAY0             (DWC_OFFSET_MASK + 0x1008)
/* [4:0] H_in_delay[12:8] */
#define HDMITX_DWC_FC_HSYNCINDELAY1             (DWC_OFFSET_MASK + 0x1009)
/* [7:0] H_in_width[7:0] */
#define HDMITX_DWC_FC_HSYNCINWIDTH0             (DWC_OFFSET_MASK + 0x100A)
/* [1:0] H_in_width[9:8] */
#define HDMITX_DWC_FC_HSYNCINWIDTH1             (DWC_OFFSET_MASK + 0x100B)
/* [7:0] V_in_delay */
#define HDMITX_DWC_FC_VSYNCINDELAY              (DWC_OFFSET_MASK + 0x100C)
/* [5:0] V_in_width */
#define HDMITX_DWC_FC_VSYNCINWIDTH              (DWC_OFFSET_MASK + 0x100D)
#define HDMITX_DWC_FC_INFREQ0                   (DWC_OFFSET_MASK + 0x100E)
#define HDMITX_DWC_FC_INFREQ1                   (DWC_OFFSET_MASK + 0x100F)
#define HDMITX_DWC_FC_INFREQ2                   (DWC_OFFSET_MASK + 0x1010)
#define HDMITX_DWC_FC_CTRLDUR                   (DWC_OFFSET_MASK + 0x1011)
#define HDMITX_DWC_FC_EXCTRLDUR                 (DWC_OFFSET_MASK + 0x1012)
#define HDMITX_DWC_FC_EXCTRLSPAC                (DWC_OFFSET_MASK + 0x1013)
#define HDMITX_DWC_FC_CH0PREAM                  (DWC_OFFSET_MASK + 0x1014)
#define HDMITX_DWC_FC_CH1PREAM                  (DWC_OFFSET_MASK + 0x1015)
#define HDMITX_DWC_FC_CH2PREAM                  (DWC_OFFSET_MASK + 0x1016)
/* [3:2] YQ */
/* [1:0] CN */
#define HDMITX_DWC_FC_AVICONF3                  (DWC_OFFSET_MASK + 0x1017)
/* [  2] default_phase */
/* [  1] set_avmute */
/* [  0] clear_avmute */
#define HDMITX_DWC_FC_GCP                       (DWC_OFFSET_MASK + 0x1018)
/* [  7] rgb_ycc_indication[2] */
/* [  6] active_format_present */
/* [5:4] scan_information */
/* [3:2] bar_information */
/* [1:0] rgb_ycc_indication[1:0] */
#define HDMITX_DWC_FC_AVICONF0                  (DWC_OFFSET_MASK + 0x1019)
/* [7:6] colorimetry */
/* [5:4] picture_aspect_ratio */
/* [3:0] active_aspect_ratio */
#define HDMITX_DWC_FC_AVICONF1                  (DWC_OFFSET_MASK + 0x101A)
/* [  7] IT_content */
/* [6:4] extended_colorimetry */
/* [3:2] quantization_range */
/* [1:0] non_uniform_picture_scaling */
#define HDMITX_DWC_FC_AVICONF2                  (DWC_OFFSET_MASK + 0x101B)
#define HDMITX_DWC_FC_AVIVID                    (DWC_OFFSET_MASK + 0x101C)
#define HDMITX_DWC_FC_AVIETB0                   (DWC_OFFSET_MASK + 0x101D)
#define HDMITX_DWC_FC_AVIETB1                   (DWC_OFFSET_MASK + 0x101E)
#define HDMITX_DWC_FC_AVISBB0                   (DWC_OFFSET_MASK + 0x101F)
#define HDMITX_DWC_FC_AVISBB1                   (DWC_OFFSET_MASK + 0x1020)
#define HDMITX_DWC_FC_AVIELB0                   (DWC_OFFSET_MASK + 0x1021)
#define HDMITX_DWC_FC_AVIELB1                   (DWC_OFFSET_MASK + 0x1022)
#define HDMITX_DWC_FC_AVISRB0                   (DWC_OFFSET_MASK + 0x1023)
#define HDMITX_DWC_FC_AVISRB1                   (DWC_OFFSET_MASK + 0x1024)
/* [3:0] CT: coding type */
#define HDMITX_DWC_FC_AUDICONF0                 (DWC_OFFSET_MASK + 0x1025)
/* [5:4] SS: sampling size */
/* [2:0] SF: sampling frequency */
#define HDMITX_DWC_FC_AUDICONF1                 (DWC_OFFSET_MASK + 0x1026)
/* CA: channel allocation */
#define HDMITX_DWC_FC_AUDICONF2                 (DWC_OFFSET_MASK + 0x1027)
/* [6:5] LFEPBL: LFE playback info */
/* [  4] DM_INH: down mix enable */
/* [3:0] LSv: Level shift value */
#define HDMITX_DWC_FC_AUDICONF3                 (DWC_OFFSET_MASK + 0x1028)
#define HDMITX_DWC_FC_VSDIEEEID0                (DWC_OFFSET_MASK + 0x1029)
#define HDMITX_DWC_FC_VSDSIZE                   (DWC_OFFSET_MASK + 0x102A)
#define HDMITX_DWC_FC_VSDIEEEID1                (DWC_OFFSET_MASK + 0x1030)
#define HDMITX_DWC_FC_VSDIEEEID2                (DWC_OFFSET_MASK + 0x1031)
#define HDMITX_DWC_FC_VSDPAYLOAD0               (DWC_OFFSET_MASK + 0x1032)
#define HDMITX_DWC_FC_VSDPAYLOAD1               (DWC_OFFSET_MASK + 0x1033)
#define HDMITX_DWC_FC_VSDPAYLOAD2               (DWC_OFFSET_MASK + 0x1034)
#define HDMITX_DWC_FC_VSDPAYLOAD3               (DWC_OFFSET_MASK + 0x1035)
#define HDMITX_DWC_FC_VSDPAYLOAD4               (DWC_OFFSET_MASK + 0x1036)
#define HDMITX_DWC_FC_VSDPAYLOAD5               (DWC_OFFSET_MASK + 0x1037)
#define HDMITX_DWC_FC_VSDPAYLOAD6               (DWC_OFFSET_MASK + 0x1038)
#define HDMITX_DWC_FC_VSDPAYLOAD7               (DWC_OFFSET_MASK + 0x1039)
#define HDMITX_DWC_FC_VSDPAYLOAD8               (DWC_OFFSET_MASK + 0x103A)
#define HDMITX_DWC_FC_VSDPAYLOAD9               (DWC_OFFSET_MASK + 0x103B)
#define HDMITX_DWC_FC_VSDPAYLOAD10              (DWC_OFFSET_MASK + 0x103C)
#define HDMITX_DWC_FC_VSDPAYLOAD11              (DWC_OFFSET_MASK + 0x103D)
#define HDMITX_DWC_FC_VSDPAYLOAD12              (DWC_OFFSET_MASK + 0x103E)
#define HDMITX_DWC_FC_VSDPAYLOAD13              (DWC_OFFSET_MASK + 0x103F)
#define HDMITX_DWC_FC_VSDPAYLOAD14              (DWC_OFFSET_MASK + 0x1040)
#define HDMITX_DWC_FC_VSDPAYLOAD15              (DWC_OFFSET_MASK + 0x1041)
#define HDMITX_DWC_FC_VSDPAYLOAD16              (DWC_OFFSET_MASK + 0x1042)
#define HDMITX_DWC_FC_VSDPAYLOAD17              (DWC_OFFSET_MASK + 0x1043)
#define HDMITX_DWC_FC_VSDPAYLOAD18              (DWC_OFFSET_MASK + 0x1044)
#define HDMITX_DWC_FC_VSDPAYLOAD19              (DWC_OFFSET_MASK + 0x1045)
#define HDMITX_DWC_FC_VSDPAYLOAD20              (DWC_OFFSET_MASK + 0x1046)
#define HDMITX_DWC_FC_VSDPAYLOAD21              (DWC_OFFSET_MASK + 0x1047)
#define HDMITX_DWC_FC_VSDPAYLOAD22              (DWC_OFFSET_MASK + 0x1048)
#define HDMITX_DWC_FC_VSDPAYLOAD23              (DWC_OFFSET_MASK + 0x1049)
#define HDMITX_DWC_FC_SPDVENDORNAME0            (DWC_OFFSET_MASK + 0x104A)
#define HDMITX_DWC_FC_SPDVENDORNAME1            (DWC_OFFSET_MASK + 0x104B)
#define HDMITX_DWC_FC_SPDVENDORNAME2            (DWC_OFFSET_MASK + 0x104C)
#define HDMITX_DWC_FC_SPDVENDORNAME3            (DWC_OFFSET_MASK + 0x104D)
#define HDMITX_DWC_FC_SPDVENDORNAME4            (DWC_OFFSET_MASK + 0x104E)
#define HDMITX_DWC_FC_SPDVENDORNAME5            (DWC_OFFSET_MASK + 0x104F)
#define HDMITX_DWC_FC_SPDVENDORNAME6            (DWC_OFFSET_MASK + 0x1050)
#define HDMITX_DWC_FC_SPDVENDORNAME7            (DWC_OFFSET_MASK + 0x1051)
#define HDMITX_DWC_FC_SDPPRODUCTNAME0           (DWC_OFFSET_MASK + 0x1052)
#define HDMITX_DWC_FC_SDPPRODUCTNAME1           (DWC_OFFSET_MASK + 0x1053)
#define HDMITX_DWC_FC_SDPPRODUCTNAME2           (DWC_OFFSET_MASK + 0x1054)
#define HDMITX_DWC_FC_SDPPRODUCTNAME3           (DWC_OFFSET_MASK + 0x1055)
#define HDMITX_DWC_FC_SDPPRODUCTNAME4           (DWC_OFFSET_MASK + 0x1056)
#define HDMITX_DWC_FC_SDPPRODUCTNAME5           (DWC_OFFSET_MASK + 0x1057)
#define HDMITX_DWC_FC_SDPPRODUCTNAME6           (DWC_OFFSET_MASK + 0x1058)
#define HDMITX_DWC_FC_SDPPRODUCTNAME7           (DWC_OFFSET_MASK + 0x1059)
#define HDMITX_DWC_FC_SDPPRODUCTNAME8           (DWC_OFFSET_MASK + 0x105A)
#define HDMITX_DWC_FC_SDPPRODUCTNAME9           (DWC_OFFSET_MASK + 0x105B)
#define HDMITX_DWC_FC_SDPPRODUCTNAME10          (DWC_OFFSET_MASK + 0x105C)
#define HDMITX_DWC_FC_SDPPRODUCTNAME11          (DWC_OFFSET_MASK + 0x105D)
#define HDMITX_DWC_FC_SDPPRODUCTNAME12          (DWC_OFFSET_MASK + 0x105E)
#define HDMITX_DWC_FC_SDPPRODUCTNAME13          (DWC_OFFSET_MASK + 0x105F)
#define HDMITX_DWC_FC_SDPPRODUCTNAME14          (DWC_OFFSET_MASK + 0x1060)
#define HDMITX_DWC_FC_SPDPRODUCTNAME15          (DWC_OFFSET_MASK + 0x1061)
#define HDMITX_DWC_FC_SPDDEVICEINF              (DWC_OFFSET_MASK + 0x1062)
/* [7:4] aud_packet_sampflat */
/* [  0] aud_packet_layout */
#define HDMITX_DWC_FC_AUDSCONF                  (DWC_OFFSET_MASK + 0x1063)
#define HDMITX_DWC_FC_AUDSSTAT                  (DWC_OFFSET_MASK + 0x1064)
/* [  7] V3r */
/* [  6] V2r */
/* [  5] V1r */
/* [  4] V0r */
/* [  3] V3l */
/* [  2] V2l */
/* [  1] V1l */
/* [  0] V0l */
#define HDMITX_DWC_FC_AUDSV                     (DWC_OFFSET_MASK + 0x1065)
#define HDMITX_DWC_FC_AUDSU                     (DWC_OFFSET_MASK + 0x1066)
/* bit5:4  CSB 41:40 */
/* bit0  CSB 2 */
#define HDMITX_DWC_FC_AUDSCHNLS0                (DWC_OFFSET_MASK + 0x1067)
/* bit7:0  CSB 15:8 */
#define HDMITX_DWC_FC_AUDSCHNLS1                (DWC_OFFSET_MASK + 0x1068)
/* bit6:4  CSB 5:3 */
/* bit3:0  CSB 17:16 */
#define HDMITX_DWC_FC_AUDSCHNLS2                (DWC_OFFSET_MASK + 0x1069)
/* bit7:4 CSB 22:21 2nd right sub */
/* bit3:0 CSB 22:21 1st right sub */
#define HDMITX_DWC_FC_AUDSCHNLS3                (DWC_OFFSET_MASK + 0x106A)
/* bit?? CSB 22:21 4th right sub */
/* bit?? CSB 22:21 3rd right sub */
#define HDMITX_DWC_FC_AUDSCHNLS4                (DWC_OFFSET_MASK + 0x106B)
/* bit7:4 CSB 22:21 2nd left sub */
/* bit3:0 CSB 22:21 1st left sub */
#define HDMITX_DWC_FC_AUDSCHNLS5                (DWC_OFFSET_MASK + 0x106C)
/* bit?? CSB 22:21 4th left sub */
/* bit?? CSB 22:21 3rd left sub */
#define HDMITX_DWC_FC_AUDSCHNLS6                (DWC_OFFSET_MASK + 0x106D)
#define HDMITX_DWC_FC_AUDSCHNLS7                (DWC_OFFSET_MASK + 0x106E)
#define HDMITX_DWC_FC_AUDSCHNLS8                (DWC_OFFSET_MASK + 0x106F)
#define HDMITX_DWC_FC_DATACH0FILL               (DWC_OFFSET_MASK + 0x1070)
#define HDMITX_DWC_FC_DATACH1FILL               (DWC_OFFSET_MASK + 0x1071)
#define HDMITX_DWC_FC_DATACH2FILL               (DWC_OFFSET_MASK + 0x1072)
#define HDMITX_DWC_FC_CTRLQHIGH                 (DWC_OFFSET_MASK + 0x1073)
#define HDMITX_DWC_FC_CTRLQLOW                  (DWC_OFFSET_MASK + 0x1074)
#define HDMITX_DWC_FC_ACP0                      (DWC_OFFSET_MASK + 0x1075)
#define HDMITX_DWC_FC_ACP16                     (DWC_OFFSET_MASK + 0x1082)
#define HDMITX_DWC_FC_ACP15                     (DWC_OFFSET_MASK + 0x1083)
#define HDMITX_DWC_FC_ACP14                     (DWC_OFFSET_MASK + 0x1084)
#define HDMITX_DWC_FC_ACP13                     (DWC_OFFSET_MASK + 0x1085)
#define HDMITX_DWC_FC_ACP12                     (DWC_OFFSET_MASK + 0x1086)
#define HDMITX_DWC_FC_ACP11                     (DWC_OFFSET_MASK + 0x1087)
#define HDMITX_DWC_FC_ACP10                     (DWC_OFFSET_MASK + 0x1088)
#define HDMITX_DWC_FC_ACP9                      (DWC_OFFSET_MASK + 0x1089)
#define HDMITX_DWC_FC_ACP8                      (DWC_OFFSET_MASK + 0x108A)
#define HDMITX_DWC_FC_ACP7                      (DWC_OFFSET_MASK + 0x108B)
#define HDMITX_DWC_FC_ACP6                      (DWC_OFFSET_MASK + 0x108C)
#define HDMITX_DWC_FC_ACP5                      (DWC_OFFSET_MASK + 0x108D)
#define HDMITX_DWC_FC_ACP4                      (DWC_OFFSET_MASK + 0x108E)
#define HDMITX_DWC_FC_ACP3                      (DWC_OFFSET_MASK + 0x108F)
#define HDMITX_DWC_FC_ACP2                      (DWC_OFFSET_MASK + 0x1090)
#define HDMITX_DWC_FC_ACP1                      (DWC_OFFSET_MASK + 0x1091)
#define HDMITX_DWC_FC_ISCR1_0                   (DWC_OFFSET_MASK + 0x1092)
#define HDMITX_DWC_FC_ISCR1_16                  (DWC_OFFSET_MASK + 0x1093)
#define HDMITX_DWC_FC_ISCR1_15                  (DWC_OFFSET_MASK + 0x1094)
#define HDMITX_DWC_FC_ISCR1_14                  (DWC_OFFSET_MASK + 0x1095)
#define HDMITX_DWC_FC_ISCR1_13                  (DWC_OFFSET_MASK + 0x1096)
#define HDMITX_DWC_FC_ISCR1_12                  (DWC_OFFSET_MASK + 0x1097)
#define HDMITX_DWC_FC_ISCR1_11                  (DWC_OFFSET_MASK + 0x1098)
#define HDMITX_DWC_FC_ISCR1_10                  (DWC_OFFSET_MASK + 0x1099)
#define HDMITX_DWC_FC_ISCR1_9                   (DWC_OFFSET_MASK + 0x109A)
#define HDMITX_DWC_FC_ISCR1_8                   (DWC_OFFSET_MASK + 0x109B)
#define HDMITX_DWC_FC_ISCR1_7                   (DWC_OFFSET_MASK + 0x109C)
#define HDMITX_DWC_FC_ISCR1_6                   (DWC_OFFSET_MASK + 0x109D)
#define HDMITX_DWC_FC_ISCR1_5                   (DWC_OFFSET_MASK + 0x109E)
#define HDMITX_DWC_FC_ISCR1_4                   (DWC_OFFSET_MASK + 0x109F)
#define HDMITX_DWC_FC_ISCR1_3                   (DWC_OFFSET_MASK + 0x10A0)
#define HDMITX_DWC_FC_ISCR1_2                   (DWC_OFFSET_MASK + 0x10A1)
#define HDMITX_DWC_FC_ISCR1_1                   (DWC_OFFSET_MASK + 0x10A2)
#define HDMITX_DWC_FC_ISCR0_15                  (DWC_OFFSET_MASK + 0x10A3)
#define HDMITX_DWC_FC_ISCR0_14                  (DWC_OFFSET_MASK + 0x10A4)
#define HDMITX_DWC_FC_ISCR0_13                  (DWC_OFFSET_MASK + 0x10A5)
#define HDMITX_DWC_FC_ISCR0_12                  (DWC_OFFSET_MASK + 0x10A6)
#define HDMITX_DWC_FC_ISCR0_11                  (DWC_OFFSET_MASK + 0x10A7)
#define HDMITX_DWC_FC_ISCR0_10                  (DWC_OFFSET_MASK + 0x10A8)
#define HDMITX_DWC_FC_ISCR0_9                   (DWC_OFFSET_MASK + 0x10A9)
#define HDMITX_DWC_FC_ISCR0_8                   (DWC_OFFSET_MASK + 0x10AA)
#define HDMITX_DWC_FC_ISCR0_7                   (DWC_OFFSET_MASK + 0x10AB)
#define HDMITX_DWC_FC_ISCR0_6                   (DWC_OFFSET_MASK + 0x10AC)
#define HDMITX_DWC_FC_ISCR0_5                   (DWC_OFFSET_MASK + 0x10AD)
#define HDMITX_DWC_FC_ISCR0_4                   (DWC_OFFSET_MASK + 0x10AE)
#define HDMITX_DWC_FC_ISCR0_3                   (DWC_OFFSET_MASK + 0x10AF)
#define HDMITX_DWC_FC_ISCR0_2                   (DWC_OFFSET_MASK + 0x10B0)
#define HDMITX_DWC_FC_ISCR0_1                   (DWC_OFFSET_MASK + 0x10B1)
#define HDMITX_DWC_FC_ISCR0_0                   (DWC_OFFSET_MASK + 0x10B2)
/* [  4] spd_auto */
/* [  3] vsd_auto */
/* [  2] isrc2_auto */
/* [  1] isrc1_auto */
/* [  0] acp_auto */
#define HDMITX_DWC_FC_DATAUTO0                  (DWC_OFFSET_MASK + 0x10B3)
#define HDMITX_DWC_FC_DATAUTO1                  (DWC_OFFSET_MASK + 0x10B4)
#define HDMITX_DWC_FC_DATAUTO2                  (DWC_OFFSET_MASK + 0x10B5)
#define HDMITX_DWC_FC_DATMAN                    (DWC_OFFSET_MASK + 0x10B6)
/* [  6] drm_auto: instert on Vsync */
/* [  5] nvbi_auto: insert on Vsync */
/* [  4] amp_auto: insert on Vsync */
/* [  3] avi_auto: insert on Vsync */
/* [  2] gcp_auto: insert on Vsync */
/* [  1] audi_auto: insert on Vsync */
/* [  0] acr_auto: insert on CTS update. Assert this bit later to avoid
 * initial packets with false CTS value
 */
#define HDMITX_DWC_FC_DATAUTO3                  (DWC_OFFSET_MASK + 0x10B7)
#define HDMITX_DWC_FC_RDRB0                     (DWC_OFFSET_MASK + 0x10B8)
#define HDMITX_DWC_FC_RDRB1                     (DWC_OFFSET_MASK + 0x10B9)
#define HDMITX_DWC_FC_RDRB2                     (DWC_OFFSET_MASK + 0x10BA)
#define HDMITX_DWC_FC_RDRB3                     (DWC_OFFSET_MASK + 0x10BB)
#define HDMITX_DWC_FC_RDRB4                     (DWC_OFFSET_MASK + 0x10BC)
#define HDMITX_DWC_FC_RDRB5                     (DWC_OFFSET_MASK + 0x10BD)
#define HDMITX_DWC_FC_RDRB6                     (DWC_OFFSET_MASK + 0x10BE)
#define HDMITX_DWC_FC_RDRB7                     (DWC_OFFSET_MASK + 0x10BF)
#define HDMITX_DWC_FC_RDRB8                     (DWC_OFFSET_MASK + 0x10C0)
#define HDMITX_DWC_FC_RDRB9                     (DWC_OFFSET_MASK + 0x10C1)
#define HDMITX_DWC_FC_RDRB10                    (DWC_OFFSET_MASK + 0x10C2)
#define HDMITX_DWC_FC_RDRB11                    (DWC_OFFSET_MASK + 0x10C3)
/* [  7] AUDI_int_mask */
/* [  6] ACP_int_mask */
/* [  5] HBR_int_mask */
/* [  2] AUDS_int_mask */
/* [  1] ACR_int_mask */
/* [  0] NULL_int_mask */
#define HDMITX_DWC_FC_MASK0                     (DWC_OFFSET_MASK + 0x10D2)
/* [  7] GMD_int_mask */
/* [  6] ISRC1_int_mask */
/* [  5] ISRC2_int_mask */
/* [  4] VSD_int_mask */
/* [  3] SPD_int_mask */
/* [  1] AVI_int_mask */
/* [  0] GCP_int_mask */
#define HDMITX_DWC_FC_MASK1                     (DWC_OFFSET_MASK + 0x10D6)
/* [  2] Mask bit for FC_INT2.DRM interrupt bit */
/* [  1] LowPriority_fifo_full */
/* [  0] HighPriority_fifo_full */
#define HDMITX_DWC_FC_MASK2                     (DWC_OFFSET_MASK + 0x10DA)
/* [7:4] incoming_pr_factor */
/* [3:0] output_pr_factor */
#define HDMITX_DWC_FC_PRCONF                    (DWC_OFFSET_MASK + 0x10E0)
/* [  4] scrambler_ucp_line */
/* [  0] scrambler_en. Only update this bit once we've sent SCDC message*/
#define HDMITX_DWC_FC_SCRAMBLER_CTRL            (DWC_OFFSET_MASK + 0x10E1)
#define HDMITX_DWC_FC_MULTISTREAM_CTRL          (DWC_OFFSET_MASK + 0x10E2)
/* [  7] drm_tx_en */
/* [  6] nvbi_tx_en */
/* [  5] amp_tx_en */
/* [  4] aut_tx_en */
/* [  3] audi_tx_en */
/* [  2] avi_tx_en */
/* [  1] gcp_tx_en */
/* [  0] acr_tx_en */
#define HDMITX_DWC_FC_PACKET_TX_EN              (DWC_OFFSET_MASK + 0x10E3)
/* [  1] actspc_hdlr_tgl */
/* [  0] actspc_hdlr_en */
#define HDMITX_DWC_FC_ACTSPC_HDLR_CFG           (DWC_OFFSET_MASK + 0x10E8)
#define HDMITX_DWC_FC_INVACT_2D_0               (DWC_OFFSET_MASK + 0x10E9)
/* [3:0] fc_invact_2d_0[11:8] */
/* [7:0] fc_invact_2d_0[7:0] */
#define HDMITX_DWC_FC_INVACT_2D_1               (DWC_OFFSET_MASK + 0x10EA)

#define HDMITX_DWC_FC_GMD_STAT                  (DWC_OFFSET_MASK + 0x1100)
#define HDMITX_DWC_FC_GMD_EN                    (DWC_OFFSET_MASK + 0x1101)
#define HDMITX_DWC_FC_GMD_UP                    (DWC_OFFSET_MASK + 0x1102)
#define HDMITX_DWC_FC_GMD_CONF                  (DWC_OFFSET_MASK + 0x1103)
#define HDMITX_DWC_FC_GMD_HB                    (DWC_OFFSET_MASK + 0x1104)
#define HDMITX_DWC_FC_GMD_PB0                   (DWC_OFFSET_MASK + 0x1105)
#define HDMITX_DWC_FC_GMD_PB1                   (DWC_OFFSET_MASK + 0x1106)
#define HDMITX_DWC_FC_GMD_PB2                   (DWC_OFFSET_MASK + 0x1107)
#define HDMITX_DWC_FC_GMD_PB3                   (DWC_OFFSET_MASK + 0x1108)
#define HDMITX_DWC_FC_GMD_PB4                   (DWC_OFFSET_MASK + 0x1109)
#define HDMITX_DWC_FC_GMD_PB5                   (DWC_OFFSET_MASK + 0x110A)
#define HDMITX_DWC_FC_GMD_PB6                   (DWC_OFFSET_MASK + 0x110B)
#define HDMITX_DWC_FC_GMD_PB7                   (DWC_OFFSET_MASK + 0x110C)
#define HDMITX_DWC_FC_GMD_PB8                   (DWC_OFFSET_MASK + 0x110D)
#define HDMITX_DWC_FC_GMD_PB9                   (DWC_OFFSET_MASK + 0x110E)
#define HDMITX_DWC_FC_GMD_PB10                  (DWC_OFFSET_MASK + 0x110F)
#define HDMITX_DWC_FC_GMD_PB11                  (DWC_OFFSET_MASK + 0x1110)
#define HDMITX_DWC_FC_GMD_PB12                  (DWC_OFFSET_MASK + 0x1111)
#define HDMITX_DWC_FC_GMD_PB13                  (DWC_OFFSET_MASK + 0x1112)
#define HDMITX_DWC_FC_GMD_PB14                  (DWC_OFFSET_MASK + 0x1113)
#define HDMITX_DWC_FC_GMD_PB15                  (DWC_OFFSET_MASK + 0x1114)
#define HDMITX_DWC_FC_GMD_PB16                  (DWC_OFFSET_MASK + 0x1115)
#define HDMITX_DWC_FC_GMD_PB17                  (DWC_OFFSET_MASK + 0x1116)
#define HDMITX_DWC_FC_GMD_PB18                  (DWC_OFFSET_MASK + 0x1117)
#define HDMITX_DWC_FC_GMD_PB19                  (DWC_OFFSET_MASK + 0x1118)
#define HDMITX_DWC_FC_GMD_PB20                  (DWC_OFFSET_MASK + 0x1119)
#define HDMITX_DWC_FC_GMD_PB21                  (DWC_OFFSET_MASK + 0x111A)
#define HDMITX_DWC_FC_GMD_PB22                  (DWC_OFFSET_MASK + 0x111B)
#define HDMITX_DWC_FC_GMD_PB23                  (DWC_OFFSET_MASK + 0x111C)
#define HDMITX_DWC_FC_GMD_PB24                  (DWC_OFFSET_MASK + 0x111D)
#define HDMITX_DWC_FC_GMD_PB25                  (DWC_OFFSET_MASK + 0x111E)
#define HDMITX_DWC_FC_GMD_PB26                  (DWC_OFFSET_MASK + 0x111F)
#define HDMITX_DWC_FC_GMD_PB27                  (DWC_OFFSET_MASK + 0x1120)

/* Audio Metadata Packet Registers */
#define HDMITX_DWC_FC_AMP_HB01                  (DWC_OFFSET_MASK + 0x1128)
#define HDMITX_DWC_FC_AMP_HB02                  (DWC_OFFSET_MASK + 0x1129)
#define HDMITX_DWC_FC_AMP_PB00                  (DWC_OFFSET_MASK + 0x112A)
#define HDMITX_DWC_FC_AMP_PB01                  (DWC_OFFSET_MASK + 0x112B)
#define HDMITX_DWC_FC_AMP_PB02                  (DWC_OFFSET_MASK + 0x112C)
#define HDMITX_DWC_FC_AMP_PB03                  (DWC_OFFSET_MASK + 0x112D)
#define HDMITX_DWC_FC_AMP_PB04                  (DWC_OFFSET_MASK + 0x112E)
#define HDMITX_DWC_FC_AMP_PB05                  (DWC_OFFSET_MASK + 0x112F)
#define HDMITX_DWC_FC_AMP_PB06                  (DWC_OFFSET_MASK + 0x1130)
#define HDMITX_DWC_FC_AMP_PB07                  (DWC_OFFSET_MASK + 0x1131)
#define HDMITX_DWC_FC_AMP_PB08                  (DWC_OFFSET_MASK + 0x1132)
#define HDMITX_DWC_FC_AMP_PB09                  (DWC_OFFSET_MASK + 0x1133)
#define HDMITX_DWC_FC_AMP_PB10                  (DWC_OFFSET_MASK + 0x1134)
#define HDMITX_DWC_FC_AMP_PB11                  (DWC_OFFSET_MASK + 0x1135)
#define HDMITX_DWC_FC_AMP_PB12                  (DWC_OFFSET_MASK + 0x1136)
#define HDMITX_DWC_FC_AMP_PB13                  (DWC_OFFSET_MASK + 0x1137)
#define HDMITX_DWC_FC_AMP_PB14                  (DWC_OFFSET_MASK + 0x1138)
#define HDMITX_DWC_FC_AMP_PB15                  (DWC_OFFSET_MASK + 0x1139)
#define HDMITX_DWC_FC_AMP_PB16                  (DWC_OFFSET_MASK + 0x113A)
#define HDMITX_DWC_FC_AMP_PB17                  (DWC_OFFSET_MASK + 0x113B)
#define HDMITX_DWC_FC_AMP_PB18                  (DWC_OFFSET_MASK + 0x113C)
#define HDMITX_DWC_FC_AMP_PB19                  (DWC_OFFSET_MASK + 0x113D)
#define HDMITX_DWC_FC_AMP_PB20                  (DWC_OFFSET_MASK + 0x113E)
#define HDMITX_DWC_FC_AMP_PB21                  (DWC_OFFSET_MASK + 0x113F)
#define HDMITX_DWC_FC_AMP_PB22                  (DWC_OFFSET_MASK + 0x1140)
#define HDMITX_DWC_FC_AMP_PB23                  (DWC_OFFSET_MASK + 0x1141)
#define HDMITX_DWC_FC_AMP_PB24                  (DWC_OFFSET_MASK + 0x1142)
#define HDMITX_DWC_FC_AMP_PB25                  (DWC_OFFSET_MASK + 0x1143)
#define HDMITX_DWC_FC_AMP_PB26                  (DWC_OFFSET_MASK + 0x1144)
#define HDMITX_DWC_FC_AMP_PB27                  (DWC_OFFSET_MASK + 0x1145)

/* NTSC VBI Packet Registers */
#define HDMITX_DWC_FC_NVBI_HB01                 (DWC_OFFSET_MASK + 0x1148)
#define HDMITX_DWC_FC_NVBI_HB02                 (DWC_OFFSET_MASK + 0x1149)
#define HDMITX_DWC_FC_NVBI_PB01                 (DWC_OFFSET_MASK + 0x114A)
#define HDMITX_DWC_FC_NVBI_PB02                 (DWC_OFFSET_MASK + 0x114B)
#define HDMITX_DWC_FC_NVBI_PB03                 (DWC_OFFSET_MASK + 0x114C)
#define HDMITX_DWC_FC_NVBI_PB04                 (DWC_OFFSET_MASK + 0x114D)
#define HDMITX_DWC_FC_NVBI_PB05                 (DWC_OFFSET_MASK + 0x114E)
#define HDMITX_DWC_FC_NVBI_PB06                 (DWC_OFFSET_MASK + 0x114F)
#define HDMITX_DWC_FC_NVBI_PB07                 (DWC_OFFSET_MASK + 0x1150)
#define HDMITX_DWC_FC_NVBI_PB08                 (DWC_OFFSET_MASK + 0x1151)
#define HDMITX_DWC_FC_NVBI_PB09                 (DWC_OFFSET_MASK + 0x1152)
#define HDMITX_DWC_FC_NVBI_PB10                 (DWC_OFFSET_MASK + 0x1153)
#define HDMITX_DWC_FC_NVBI_PB11                 (DWC_OFFSET_MASK + 0x1154)
#define HDMITX_DWC_FC_NVBI_PB12                 (DWC_OFFSET_MASK + 0x1155)
#define HDMITX_DWC_FC_NVBI_PB13                 (DWC_OFFSET_MASK + 0x1156)
#define HDMITX_DWC_FC_NVBI_PB14                 (DWC_OFFSET_MASK + 0x1157)
#define HDMITX_DWC_FC_NVBI_PB15                 (DWC_OFFSET_MASK + 0x1158)
#define HDMITX_DWC_FC_NVBI_PB16                 (DWC_OFFSET_MASK + 0x1159)
#define HDMITX_DWC_FC_NVBI_PB17                 (DWC_OFFSET_MASK + 0x115A)
#define HDMITX_DWC_FC_NVBI_PB18                 (DWC_OFFSET_MASK + 0x115B)
#define HDMITX_DWC_FC_NVBI_PB19                 (DWC_OFFSET_MASK + 0x115C)
#define HDMITX_DWC_FC_NVBI_PB20                 (DWC_OFFSET_MASK + 0x115D)
#define HDMITX_DWC_FC_NVBI_PB21                 (DWC_OFFSET_MASK + 0x115E)
#define HDMITX_DWC_FC_NVBI_PB22                 (DWC_OFFSET_MASK + 0x115F)
#define HDMITX_DWC_FC_NVBI_PB23                 (DWC_OFFSET_MASK + 0x1160)
#define HDMITX_DWC_FC_NVBI_PB24                 (DWC_OFFSET_MASK + 0x1161)
#define HDMITX_DWC_FC_NVBI_PB25                 (DWC_OFFSET_MASK + 0x1162)
#define HDMITX_DWC_FC_NVBI_PB26                 (DWC_OFFSET_MASK + 0x1163)
#define HDMITX_DWC_FC_NVBI_PB27                 (DWC_OFFSET_MASK + 0x1164)
#define HDMITX_DWC_FC_DRM_HB01                  (DWC_OFFSET_MASK + 0x1168)
#define HDMITX_DWC_FC_DRM_HB02                  (DWC_OFFSET_MASK + 0x1169)
#define HDMITX_DWC_FC_DRM_PB00                  (DWC_OFFSET_MASK + 0x116A)
#define HDMITX_DWC_FC_DRM_PB01                  (DWC_OFFSET_MASK + 0x116B)
#define HDMITX_DWC_FC_DRM_PB02                  (DWC_OFFSET_MASK + 0x116C)
#define HDMITX_DWC_FC_DRM_PB03                  (DWC_OFFSET_MASK + 0x116D)
#define HDMITX_DWC_FC_DRM_PB04                  (DWC_OFFSET_MASK + 0x116E)
#define HDMITX_DWC_FC_DRM_PB05                  (DWC_OFFSET_MASK + 0x116F)
#define HDMITX_DWC_FC_DRM_PB06                  (DWC_OFFSET_MASK + 0x1170)
#define HDMITX_DWC_FC_DRM_PB07                  (DWC_OFFSET_MASK + 0x1171)
#define HDMITX_DWC_FC_DRM_PB08                  (DWC_OFFSET_MASK + 0x1172)
#define HDMITX_DWC_FC_DRM_PB09                  (DWC_OFFSET_MASK + 0x1173)
#define HDMITX_DWC_FC_DRM_PB10                  (DWC_OFFSET_MASK + 0x1174)
#define HDMITX_DWC_FC_DRM_PB11                  (DWC_OFFSET_MASK + 0x1175)
#define HDMITX_DWC_FC_DRM_PB12                  (DWC_OFFSET_MASK + 0x1176)
#define HDMITX_DWC_FC_DRM_PB13                  (DWC_OFFSET_MASK + 0x1177)
#define HDMITX_DWC_FC_DRM_PB14                  (DWC_OFFSET_MASK + 0x1178)
#define HDMITX_DWC_FC_DRM_PB15                  (DWC_OFFSET_MASK + 0x1179)
#define HDMITX_DWC_FC_DRM_PB16                  (DWC_OFFSET_MASK + 0x117A)
#define HDMITX_DWC_FC_DRM_PB17                  (DWC_OFFSET_MASK + 0x117B)
#define HDMITX_DWC_FC_DRM_PB18                  (DWC_OFFSET_MASK + 0x117C)
#define HDMITX_DWC_FC_DRM_PB19                  (DWC_OFFSET_MASK + 0x117D)
#define HDMITX_DWC_FC_DRM_PB20                  (DWC_OFFSET_MASK + 0x117E)
#define HDMITX_DWC_FC_DRM_PB21                  (DWC_OFFSET_MASK + 0x117F)
#define HDMITX_DWC_FC_DRM_PB22                  (DWC_OFFSET_MASK + 0x1180)
#define HDMITX_DWC_FC_DRM_PB23                  (DWC_OFFSET_MASK + 0x1181)
#define HDMITX_DWC_FC_DRM_PB24                  (DWC_OFFSET_MASK + 0x1182)
#define HDMITX_DWC_FC_DRM_PB25                  (DWC_OFFSET_MASK + 0x1183)
#define HDMITX_DWC_FC_DRM_PB26                  (DWC_OFFSET_MASK + 0x1184)

#define HDMITX_DWC_FC_DBGFORCE                  (DWC_OFFSET_MASK + 0x1200)
#define HDMITX_DWC_FC_DBGAUD0CH0                (DWC_OFFSET_MASK + 0x1201)
#define HDMITX_DWC_FC_DBGAUD1CH0                (DWC_OFFSET_MASK + 0x1202)
#define HDMITX_DWC_FC_DBGAUD2CH0                (DWC_OFFSET_MASK + 0x1203)
#define HDMITX_DWC_FC_DBGAUD0CH1                (DWC_OFFSET_MASK + 0x1204)
#define HDMITX_DWC_FC_DBGAUD1CH1                (DWC_OFFSET_MASK + 0x1205)
#define HDMITX_DWC_FC_DBGAUD2CH1                (DWC_OFFSET_MASK + 0x1206)
#define HDMITX_DWC_FC_DBGAUD0CH2                (DWC_OFFSET_MASK + 0x1207)
#define HDMITX_DWC_FC_DBGAUD1CH2                (DWC_OFFSET_MASK + 0x1208)
#define HDMITX_DWC_FC_DBGAUD2CH2                (DWC_OFFSET_MASK + 0x1209)
#define HDMITX_DWC_FC_DBGAUD0CH3                (DWC_OFFSET_MASK + 0x120A)
#define HDMITX_DWC_FC_DBGAUD1CH3                (DWC_OFFSET_MASK + 0x120B)
#define HDMITX_DWC_FC_DBGAUD2CH3                (DWC_OFFSET_MASK + 0x120C)
#define HDMITX_DWC_FC_DBGAUD0CH4                (DWC_OFFSET_MASK + 0x120D)
#define HDMITX_DWC_FC_DBGAUD1CH4                (DWC_OFFSET_MASK + 0x120E)
#define HDMITX_DWC_FC_DBGAUD2CH4                (DWC_OFFSET_MASK + 0x120F)
#define HDMITX_DWC_FC_DBGAUD0CH5                (DWC_OFFSET_MASK + 0x1210)
#define HDMITX_DWC_FC_DBGAUD1CH5                (DWC_OFFSET_MASK + 0x1211)
#define HDMITX_DWC_FC_DBGAUD2CH5                (DWC_OFFSET_MASK + 0x1212)
#define HDMITX_DWC_FC_DBGAUD0CH6                (DWC_OFFSET_MASK + 0x1213)
#define HDMITX_DWC_FC_DBGAUD1CH6                (DWC_OFFSET_MASK + 0x1214)
#define HDMITX_DWC_FC_DBGAUD2CH6                (DWC_OFFSET_MASK + 0x1215)
#define HDMITX_DWC_FC_DBGAUD0CH7                (DWC_OFFSET_MASK + 0x1216)
#define HDMITX_DWC_FC_DBGAUD1CH7                (DWC_OFFSET_MASK + 0x1217)
#define HDMITX_DWC_FC_DBGAUD2CH7                (DWC_OFFSET_MASK + 0x1218)
#define HDMITX_DWC_FC_DBGTMDS0                  (DWC_OFFSET_MASK + 0x1219)
#define HDMITX_DWC_FC_DBGTMDS1                  (DWC_OFFSET_MASK + 0x121A)
#define HDMITX_DWC_FC_DBGTMDS2                  (DWC_OFFSET_MASK + 0x121B)

/* HDMI Source PHY Registers */
#define HDMITX_DWC_PHY_CONF0                    (DWC_OFFSET_MASK + 0x3000)
#define HDMITX_DWC_PHY_TST0                     (DWC_OFFSET_MASK + 0x3001)
#define HDMITX_DWC_PHY_TST1                     (DWC_OFFSET_MASK + 0x3002)
#define HDMITX_DWC_PHY_TST2                     (DWC_OFFSET_MASK + 0x3003)
#define HDMITX_DWC_PHY_STAT0                    (DWC_OFFSET_MASK + 0x3004)
#define HDMITX_DWC_PHY_INT0                     (DWC_OFFSET_MASK + 0x3005)
#define HDMITX_DWC_PHY_MASK0                    (DWC_OFFSET_MASK + 0x3006)
#define HDMITX_DWC_PHY_POL0                     (DWC_OFFSET_MASK + 0x3007)

/* I2C Master PHY Registers */
#define HDMITX_DWC_I2CM_PHY_SLAVE               (DWC_OFFSET_MASK + 0x3020)
#define HDMITX_DWC_I2CM_PHY_ADDRESS             (DWC_OFFSET_MASK + 0x3021)
#define HDMITX_DWC_I2CM_PHY_DATAO_1             (DWC_OFFSET_MASK + 0x3022)
#define HDMITX_DWC_I2CM_PHY_DATAO_0             (DWC_OFFSET_MASK + 0x3023)
#define HDMITX_DWC_I2CM_PHY_DATAI_1             (DWC_OFFSET_MASK + 0x3024)
#define HDMITX_DWC_I2CM_PHY_DATAI_0             (DWC_OFFSET_MASK + 0x3025)
#define HDMITX_DWC_I2CM_PHY_OPERATION           (DWC_OFFSET_MASK + 0x3026)
#define HDMITX_DWC_I2CM_PHY_INT                 (DWC_OFFSET_MASK + 0x3027)
#define HDMITX_DWC_I2CM_PHY_CTLINT              (DWC_OFFSET_MASK + 0x3028)
#define HDMITX_DWC_I2CM_PHY_DIV                 (DWC_OFFSET_MASK + 0x3029)
#define HDMITX_DWC_I2CM_PHY_SOFTRSTZ            (DWC_OFFSET_MASK + 0x302A)
#define HDMITX_DWC_I2CM_PHY_SS_SCL_HCNT_1       (DWC_OFFSET_MASK + 0x302B)
#define HDMITX_DWC_I2CM_PHY_SS_SCL_HCNT_0       (DWC_OFFSET_MASK + 0x302C)
#define HDMITX_DWC_I2CM_PHY_SS_SCL_LCNT_1       (DWC_OFFSET_MASK + 0x302D)
#define HDMITX_DWC_I2CM_PHY_SS_SCL_LCNT_0       (DWC_OFFSET_MASK + 0x302E)
#define HDMITX_DWC_I2CM_PHY_FS_SCL_HCNT_1       (DWC_OFFSET_MASK + 0x302F)
#define HDMITX_DWC_I2CM_PHY_FS_SCL_HCNT_0       (DWC_OFFSET_MASK + 0x3030)
#define HDMITX_DWC_I2CM_PHY_FS_SCL_LCNT_1       (DWC_OFFSET_MASK + 0x3031)
#define HDMITX_DWC_I2CM_PHY_FS_SCL_LCNT_0       (DWC_OFFSET_MASK + 0x3032)
#define HDMITX_DWC_I2CM_PHY_SDA_HOLD            (DWC_OFFSET_MASK + 0x3033)

/* Audio Sampler Registers */

  /* [  7] sw_audio_fifo_rst */
  /* [  5] 0=select SPDIF; 1=select I2S. */
  /* [3:0] i2s_in_en: enable it later in test.c */
#define HDMITX_DWC_AUD_CONF0                    (DWC_OFFSET_MASK + 0x3100)
/* [4:0] i2s_width */
/* [7:5] i2s_mode: 0=standard I2S mode */
#define HDMITX_DWC_AUD_CONF1                    (DWC_OFFSET_MASK + 0x3101)
/* [  3] fifo_empty_mask: 0=enable int; 1=mask int. */
/* [  2] fifo_full_mask: 0=enable int; 1=mask int. */
#define HDMITX_DWC_AUD_INT                      (DWC_OFFSET_MASK + 0x3102)
  /* [  1] NLPCM */
#define HDMITX_DWC_AUD_CONF2                    (DWC_OFFSET_MASK + 0x3103)

/* [  4] fifo_overrun_mask: 0=enable int; 1=mask int.
 * Enable it later when audio starts.
 */
#define HDMITX_DWC_AUD_INT1                     (DWC_OFFSET_MASK + 0x3104)

#define HDMITX_DWC_AUD_N1                       (DWC_OFFSET_MASK + 0x3200)
#define HDMITX_DWC_AUD_N2                       (DWC_OFFSET_MASK + 0x3201)
#define HDMITX_DWC_AUD_N3                       (DWC_OFFSET_MASK + 0x3202)
#define HDMITX_DWC_AUD_CTS1                     (DWC_OFFSET_MASK + 0x3203)
#define HDMITX_DWC_AUD_CTS2                     (DWC_OFFSET_MASK + 0x3204)
#define HDMITX_DWC_AUD_CTS3                     (DWC_OFFSET_MASK + 0x3205)
#define HDMITX_DWC_AUD_INPUTCLKFS               (DWC_OFFSET_MASK + 0x3206)
/* [  7] sw_audio_fifo_rst */
#define HDMITX_DWC_AUD_SPDIF0                   (DWC_OFFSET_MASK + 0x3300)
/* [4:0] spdif_width */
/* [  7] setnlpcm */
#define HDMITX_DWC_AUD_SPDIF1                   (DWC_OFFSET_MASK + 0x3301)
/* [  3] SPDIF fifo_empty_mask: 0=enable int; 1=mask int. */
/* [  2] SPDIF fifo_full_mask: 0=enable int; 1=mask int. */
#define HDMITX_DWC_AUD_SPDIFINT                 (DWC_OFFSET_MASK + 0x3302)
/* [  4] SPDIF fifo_overrun_mask: 0=enable int; 1=mask int. */
#define HDMITX_DWC_AUD_SPDIFINT1                (DWC_OFFSET_MASK + 0x3303)

/* Generic Parallel Audio Interface Registers   (DWC_OFFSET_MASK + 0x3500) */
/* Audio DMA Registers                          (DWC_OFFSET_MASK + 0x3600) */

/* Main Controller Registers */
/* [  6] hdcpclk_disable */
/* [  5] cecclk_disable */
/* [  4] cscclk_disable */
/* [  3] audclk_disable */
/* [  2] prepclk_disable */
/* [  1] tmdsclk_disable */
/* [  0] pixelclk_disable */
#define HDMITX_DWC_MC_CLKDIS                    (DWC_OFFSET_MASK + 0x4001)
/*
 * [  7] gpaswrst_req: 0=generate reset pulse; 1=no reset.
 * [  6] cecswrst_req: 0=generate reset pulse; 1=no reset.
 * [  4] spdifswrst_req: 0=generate reset pulse; 1=no reset.
 * [  3] i2sswrst_req: 0=generate reset pulse; 1=no reset.
 * [  2] prepswrst_req: 0=generate reset pulse; 1=no reset.
 * [  1] tmdsswrst_req: 0=generate reset pulse; 1=no reset.
 * [  0] pixelswrst_req: 0=generate reset pulse; 1=no reset.
 */
#define HDMITX_DWC_MC_SWRSTZREQ                 (DWC_OFFSET_MASK + 0x4002)
#define HDMITX_DWC_MC_OPCTRL                    (DWC_OFFSET_MASK + 0x4003)
/* [  0] CSC enable */
#define HDMITX_DWC_MC_FLOWCTRL                  (DWC_OFFSET_MASK + 0x4004)
#define HDMITX_DWC_MC_PHYRSTZ                   (DWC_OFFSET_MASK + 0x4005)
#define HDMITX_DWC_MC_LOCKONCLOCK               (DWC_OFFSET_MASK + 0x4006)

/* Color Space Converter Registers */
/* [  7] csc_limit */
#define HDMITX_DWC_CSC_CFG                      (DWC_OFFSET_MASK + 0x4100)
#define HDMITX_DWC_CSC_SCALE                    (DWC_OFFSET_MASK + 0x4101)
#define HDMITX_DWC_CSC_COEF_A1_MSB              (DWC_OFFSET_MASK + 0x4102)
#define HDMITX_DWC_CSC_COEF_A1_LSB              (DWC_OFFSET_MASK + 0x4103)
#define HDMITX_DWC_CSC_COEF_A2_MSB              (DWC_OFFSET_MASK + 0x4104)
#define HDMITX_DWC_CSC_COEF_A2_LSB              (DWC_OFFSET_MASK + 0x4105)
#define HDMITX_DWC_CSC_COEF_A3_MSB              (DWC_OFFSET_MASK + 0x4106)
#define HDMITX_DWC_CSC_COEF_A3_LSB              (DWC_OFFSET_MASK + 0x4107)
#define HDMITX_DWC_CSC_COEF_A4_MSB              (DWC_OFFSET_MASK + 0x4108)
#define HDMITX_DWC_CSC_COEF_A4_LSB              (DWC_OFFSET_MASK + 0x4109)
#define HDMITX_DWC_CSC_COEF_B1_MSB              (DWC_OFFSET_MASK + 0x410A)
#define HDMITX_DWC_CSC_COEF_B1_LSB              (DWC_OFFSET_MASK + 0x410B)
#define HDMITX_DWC_CSC_COEF_B2_MSB              (DWC_OFFSET_MASK + 0x410C)
#define HDMITX_DWC_CSC_COEF_B2_LSB              (DWC_OFFSET_MASK + 0x410D)
#define HDMITX_DWC_CSC_COEF_B3_MSB              (DWC_OFFSET_MASK + 0x410E)
#define HDMITX_DWC_CSC_COEF_B3_LSB              (DWC_OFFSET_MASK + 0x410F)
#define HDMITX_DWC_CSC_COEF_B4_MSB              (DWC_OFFSET_MASK + 0x4110)
#define HDMITX_DWC_CSC_COEF_B4_LSB              (DWC_OFFSET_MASK + 0x4111)
#define HDMITX_DWC_CSC_COEF_C1_MSB              (DWC_OFFSET_MASK + 0x4112)
#define HDMITX_DWC_CSC_COEF_C1_LSB              (DWC_OFFSET_MASK + 0x4113)
#define HDMITX_DWC_CSC_COEF_C2_MSB              (DWC_OFFSET_MASK + 0x4114)
#define HDMITX_DWC_CSC_COEF_C2_LSB              (DWC_OFFSET_MASK + 0x4115)
#define HDMITX_DWC_CSC_COEF_C3_MSB              (DWC_OFFSET_MASK + 0x4116)
#define HDMITX_DWC_CSC_COEF_C3_LSB              (DWC_OFFSET_MASK + 0x4117)
#define HDMITX_DWC_CSC_COEF_C4_MSB              (DWC_OFFSET_MASK + 0x4118)
#define HDMITX_DWC_CSC_COEF_C4_LSB              (DWC_OFFSET_MASK + 0x4119)
#define HDMITX_DWC_CSC_LIMIT_UP_MSB             (DWC_OFFSET_MASK + 0x411A)
#define HDMITX_DWC_CSC_LIMIT_UP_LSB             (DWC_OFFSET_MASK + 0x411B)
#define HDMITX_DWC_CSC_LIMIT_DN_MSB             (DWC_OFFSET_MASK + 0x411C)
#define HDMITX_DWC_CSC_LIMIT_DN_LSB             (DWC_OFFSET_MASK + 0x411D)

/* HDCP Encryption Engine Registers */
#define HDMITX_DWC_A_HDCPCFG0                   (DWC_SEC_OFFSET_MASK + 0x5000)
/* [  4] hdcp_lock */
/* [  3] dissha1check */
/* [  2] ph2upshiftenc */
/* [  1] encryptiondisable */
/* [  0] swresetn. Write 0 to activate, self-clear to 1. */
#define HDMITX_DWC_A_HDCPCFG1                   (DWC_SEC_OFFSET_MASK + 0x5001)
#define HDMITX_DWC_A_HDCPOBS0                   (DWC_OFFSET_MASK + 0x5002)
#define HDMITX_DWC_A_HDCPOBS1                   (DWC_OFFSET_MASK + 0x5003)
#define HDMITX_DWC_A_HDCPOBS2                   (DWC_OFFSET_MASK + 0x5004)
#define HDMITX_DWC_A_HDCPOBS3                   (DWC_OFFSET_MASK + 0x5005)
#define HDMITX_DWC_A_APIINTCLR                  (DWC_OFFSET_MASK + 0x5006)
#define HDMITX_DWC_A_APIINTSTAT                 (DWC_OFFSET_MASK + 0x5007)
/* [  7] hdcp_engaged_int_mask */
/* [  6] hdcp_failed_int_mask */
/* [  4] i2c_nack_int_mask */
/* [  3] lost_arbitration_int_mask */
/* [  2] keepout_error_int_mask */
/* [  1] ksv_sha1_calc_int_mask */
/* [  0] ksv_access_int_mask */
#define HDMITX_DWC_A_APIINTMSK                  (DWC_OFFSET_MASK + 0x5008)
/* [6:5] unencryptconf */
/* [  4] dataenpol */
/* [  3] vsyncpol */
/* [  1] hsyncpol */
#define HDMITX_DWC_A_VIDPOLCFG                  (DWC_OFFSET_MASK + 0x5009)
#define HDMITX_DWC_A_OESSWCFG                   (DWC_OFFSET_MASK + 0x500A)
#define HDMITX_DWC_A_COREVERLSB                 (DWC_OFFSET_MASK + 0x5014)
#define HDMITX_DWC_A_COREVERMSB                 (DWC_OFFSET_MASK + 0x5015)
/* [  3] sha1_fail */
/* [  2] ksv_ctrl_update */
/* [  1] Rsvd for read-only ksv_mem_access */
/* [  0] ksv_mem_request */
#define HDMITX_DWC_A_KSVMEMCTRL                 (DWC_OFFSET_MASK + 0x5016)
#define HDMITX_DWC_A_BSTATUS_HI                 (DWC_OFFSET_MASK + 0x5017)
#define HDMITX_DWC_A_BSTATUS_LO                 (DWC_OFFSET_MASK + 0x5018)

#define HDMITX_DWC_HDCP_BSTATUS_0               (TOP_OFFSET_MASK + 0x2000)
#define HDMITX_DWC_HDCP_BSTATUS_1               (TOP_OFFSET_MASK + 0x2001)
#define HDMITX_DWC_HDCP_M0_0                    (TOP_OFFSET_MASK + 0x2002)
#define HDMITX_DWC_HDCP_M0_1                    (TOP_OFFSET_MASK + 0x2003)
#define HDMITX_DWC_HDCP_M0_2                    (TOP_OFFSET_MASK + 0x2004)
#define HDMITX_DWC_HDCP_M0_3                    (TOP_OFFSET_MASK + 0x2005)
#define HDMITX_DWC_HDCP_M0_4                    (TOP_OFFSET_MASK + 0x2006)
#define HDMITX_DWC_HDCP_M0_5                    (TOP_OFFSET_MASK + 0x2007)
#define HDMITX_DWC_HDCP_M0_6                    (TOP_OFFSET_MASK + 0x2008)
#define HDMITX_DWC_HDCP_M0_7                    (TOP_OFFSET_MASK + 0x2009)
#define HDMITX_DWC_HDCP_KSV                     (TOP_OFFSET_MASK + 0x200A)
#define HDMITX_DWC_HDCP_VH                      (TOP_OFFSET_MASK + 0x2285)
#define HDMITX_DWC_HDCP_REVOC_SIZE_0            (TOP_OFFSET_MASK + 0x2299)
#define HDMITX_DWC_HDCP_REVOC_SIZE_1            (TOP_OFFSET_MASK + 0x229A)
#define HDMITX_DWC_HDCP_REVOC_LIST              (TOP_OFFSET_MASK + 0x229B)
#define HDMITX_DWC_HDCP_REVOC_LIST_END          (TOP_OFFSET_MASK + 0x365E)

/* HDCP BKSV Registers */
#define HDMITX_DWC_HDCPREG_BKSV0                (DWC_OFFSET_MASK + 0x7800)
#define HDMITX_DWC_HDCPREG_BKSV1                (DWC_OFFSET_MASK + 0x7801)
#define HDMITX_DWC_HDCPREG_BKSV2                (DWC_OFFSET_MASK + 0x7802)
#define HDMITX_DWC_HDCPREG_BKSV3                (DWC_OFFSET_MASK + 0x7803)
#define HDMITX_DWC_HDCPREG_BKSV4                (DWC_OFFSET_MASK + 0x7804)

/* HDCP AN Registers */
#define HDMITX_DWC_HDCPREG_ANCONF               (DWC_OFFSET_MASK + 0x7805)
#define HDMITX_DWC_HDCPREG_AN0                  (DWC_OFFSET_MASK + 0x7806)
#define HDMITX_DWC_HDCPREG_AN1                  (DWC_OFFSET_MASK + 0x7807)
#define HDMITX_DWC_HDCPREG_AN2                  (DWC_OFFSET_MASK + 0x7808)
#define HDMITX_DWC_HDCPREG_AN3                  (DWC_OFFSET_MASK + 0x7809)
#define HDMITX_DWC_HDCPREG_AN4                  (DWC_OFFSET_MASK + 0x780A)
#define HDMITX_DWC_HDCPREG_AN5                  (DWC_OFFSET_MASK + 0x780B)
#define HDMITX_DWC_HDCPREG_AN6                  (DWC_OFFSET_MASK + 0x780C)
#define HDMITX_DWC_HDCPREG_AN7                  (DWC_OFFSET_MASK + 0x780D)
#define HDMITX_DWC_HDCPREG_RMLCTL               (DWC_OFFSET_MASK + 0x780E)

/* Encrypted DPK Embedded Storage Registers */
#define HDMITX_DWC_HDCPREG_RMLSTS               (DWC_OFFSET_MASK + 0x780F)
#define HDMITX_DWC_HDCPREG_SEED0                (DWC_SEC_OFFSET_MASK + 0x7810)
#define HDMITX_DWC_HDCPREG_SEED1                (DWC_SEC_OFFSET_MASK + 0x7811)
#define HDMITX_DWC_HDCPREG_DPK0                 (DWC_SEC_OFFSET_MASK + 0x7812)
#define HDMITX_DWC_HDCPREG_DPK1                 (DWC_SEC_OFFSET_MASK + 0x7813)
#define HDMITX_DWC_HDCPREG_DPK2                 (DWC_SEC_OFFSET_MASK + 0x7814)
#define HDMITX_DWC_HDCPREG_DPK3                 (DWC_SEC_OFFSET_MASK + 0x7815)
#define HDMITX_DWC_HDCPREG_DPK4                 (DWC_SEC_OFFSET_MASK + 0x7816)
#define HDMITX_DWC_HDCPREG_DPK5                 (DWC_SEC_OFFSET_MASK + 0x7817)
#define HDMITX_DWC_HDCPREG_DPK6                 (DWC_SEC_OFFSET_MASK + 0x7818)

/* HDCP22 Registers */
#define HDMITX_DWC_HDCP22REG_ID                 (DWC_OFFSET_MASK + 0x7900)
#define HDMITX_DWC_HDCP22REG_CTRL               (DWC_SEC_OFFSET_MASK + 0x7904)
#define HDMITX_DWC_HDCP22REG_CTRL1              (DWC_OFFSET_MASK + 0x7905)
#define HDMITX_DWC_HDCP22REG_STS                (DWC_OFFSET_MASK + 0x7908)
#define HDMITX_DWC_HDCP22REG_MASK               (DWC_OFFSET_MASK + 0x790C)
#define HDMITX_DWC_HDCP22REG_STAT               (DWC_OFFSET_MASK + 0x790D)
#define HDMITX_DWC_HDCP22REG_MUTE               (DWC_OFFSET_MASK + 0x790E)


/* ********** CEC related ********** */

/* CEC 2.0 Engine Registers */
#define HDMITX_DWC_CEC_CTRL                     (DWC_OFFSET_MASK + 0x7D00)
#define HDMITX_DWC_CEC_INTR_MASK                (DWC_OFFSET_MASK + 0x7D02)
#define HDMITX_DWC_CEC_LADD_LOW                 (DWC_OFFSET_MASK + 0x7D05)
#define HDMITX_DWC_CEC_LADD_HIGH                (DWC_OFFSET_MASK + 0x7D06)
#define HDMITX_DWC_CEC_TX_CNT                   (DWC_OFFSET_MASK + 0x7D07)
#define HDMITX_DWC_CEC_RX_CNT                   (DWC_OFFSET_MASK + 0x7D08)
#define HDMITX_DWC_CEC_TX_DATA00                (DWC_OFFSET_MASK + 0x7D10)
#define HDMITX_DWC_CEC_TX_DATA01                (DWC_OFFSET_MASK + 0x7D11)
#define HDMITX_DWC_CEC_TX_DATA02                (DWC_OFFSET_MASK + 0x7D12)
#define HDMITX_DWC_CEC_TX_DATA03                (DWC_OFFSET_MASK + 0x7D13)
#define HDMITX_DWC_CEC_TX_DATA04                (DWC_OFFSET_MASK + 0x7D14)
#define HDMITX_DWC_CEC_TX_DATA05                (DWC_OFFSET_MASK + 0x7D15)
#define HDMITX_DWC_CEC_TX_DATA06                (DWC_OFFSET_MASK + 0x7D16)
#define HDMITX_DWC_CEC_TX_DATA07                (DWC_OFFSET_MASK + 0x7D17)
#define HDMITX_DWC_CEC_TX_DATA08                (DWC_OFFSET_MASK + 0x7D18)
#define HDMITX_DWC_CEC_TX_DATA09                (DWC_OFFSET_MASK + 0x7D19)
#define HDMITX_DWC_CEC_TX_DATA10                (DWC_OFFSET_MASK + 0x7D1A)
#define HDMITX_DWC_CEC_TX_DATA11                (DWC_OFFSET_MASK + 0x7D1B)
#define HDMITX_DWC_CEC_TX_DATA12                (DWC_OFFSET_MASK + 0x7D1C)
#define HDMITX_DWC_CEC_TX_DATA13                (DWC_OFFSET_MASK + 0x7D1D)
#define HDMITX_DWC_CEC_TX_DATA14                (DWC_OFFSET_MASK + 0x7D1E)
#define HDMITX_DWC_CEC_TX_DATA15                (DWC_OFFSET_MASK + 0x7D1F)
#define HDMITX_DWC_CEC_RX_DATA00                (DWC_OFFSET_MASK + 0x7D20)
#define HDMITX_DWC_CEC_RX_DATA01                (DWC_OFFSET_MASK + 0x7D21)
#define HDMITX_DWC_CEC_RX_DATA02                (DWC_OFFSET_MASK + 0x7D22)
#define HDMITX_DWC_CEC_RX_DATA03                (DWC_OFFSET_MASK + 0x7D23)
#define HDMITX_DWC_CEC_RX_DATA04                (DWC_OFFSET_MASK + 0x7D24)
#define HDMITX_DWC_CEC_RX_DATA05                (DWC_OFFSET_MASK + 0x7D25)
#define HDMITX_DWC_CEC_RX_DATA06                (DWC_OFFSET_MASK + 0x7D26)
#define HDMITX_DWC_CEC_RX_DATA07                (DWC_OFFSET_MASK + 0x7D27)
#define HDMITX_DWC_CEC_RX_DATA08                (DWC_OFFSET_MASK + 0x7D28)
#define HDMITX_DWC_CEC_RX_DATA09                (DWC_OFFSET_MASK + 0x7D29)
#define HDMITX_DWC_CEC_RX_DATA10                (DWC_OFFSET_MASK + 0x7D2A)
#define HDMITX_DWC_CEC_RX_DATA11                (DWC_OFFSET_MASK + 0x7D2B)
#define HDMITX_DWC_CEC_RX_DATA12                (DWC_OFFSET_MASK + 0x7D2C)
#define HDMITX_DWC_CEC_RX_DATA13                (DWC_OFFSET_MASK + 0x7D2D)
#define HDMITX_DWC_CEC_RX_DATA14                (DWC_OFFSET_MASK + 0x7D2E)
#define HDMITX_DWC_CEC_RX_DATA15                (DWC_OFFSET_MASK + 0x7D2F)
#define HDMITX_DWC_CEC_LOCK_BUF                 (DWC_OFFSET_MASK + 0x7D30)
#define HDMITX_DWC_CEC_WAKEUPCTRL               (DWC_OFFSET_MASK + 0x7D31)

/* I2C Master Registers(E-DDC/SCDC) */
#define HDMITX_DWC_I2CM_SLAVE                   (DWC_OFFSET_MASK + 0x7E00)
#define HDMITX_DWC_I2CM_ADDRESS                 (DWC_OFFSET_MASK + 0x7E01)
#define HDMITX_DWC_I2CM_DATAO                   (DWC_OFFSET_MASK + 0x7E02)
#define HDMITX_DWC_I2CM_DATAI                   (DWC_OFFSET_MASK + 0x7E03)
#define HDMITX_DWC_I2CM_OPERATION               (DWC_OFFSET_MASK + 0x7E04)
/* [  2] done_mask */
/* [  6] read_req_mask */
#define HDMITX_DWC_I2CM_INT                     (DWC_OFFSET_MASK + 0x7E05)
/* [  6] nack_mask */
/* [  2] arbitration_error_mask */
#define HDMITX_DWC_I2CM_CTLINT                  (DWC_OFFSET_MASK + 0x7E06)
/* [  3] i2c_fast_mode: 0=standard mode; 1=fast mode. */
#define HDMITX_DWC_I2CM_DIV                     (DWC_OFFSET_MASK + 0x7E07)
#define HDMITX_DWC_I2CM_SEGADDR                 (DWC_OFFSET_MASK + 0x7E08)
#define HDMITX_DWC_I2CM_SOFTRSTZ                (DWC_OFFSET_MASK + 0x7E09)
#define HDMITX_DWC_I2CM_SEGPTR                  (DWC_OFFSET_MASK + 0x7E0A)
/* I2CM_SS_SCL_HCNT = RndUp(min_ss_scl_htime*Freq(sfrclkInMHz)/1000) */
/* I2CM_SS_SCL_LCNT = RndUp(min_ss_scl_ltime*Freq(sfrclkInMHz)/1000) */
/* I2CM_FS_SCL_HCNT = RndUp(min_fs_scl_htime*Freq(sfrclkInMHz)/1000) */
/* I2CM_FS_SCL_LCNT = RndUp(min_fs_scl_ltime*Freq(sfrclkInMHz)/1000) */
/* Where Freq(sfrclkInMHz)=24; */
#define HDMITX_DWC_I2CM_SS_SCL_HCNT_1           (DWC_OFFSET_MASK + 0x7E0B)
#define HDMITX_DWC_I2CM_SS_SCL_HCNT_0           (DWC_OFFSET_MASK + 0x7E0C)
#define HDMITX_DWC_I2CM_SS_SCL_LCNT_1           (DWC_OFFSET_MASK + 0x7E0D)
#define HDMITX_DWC_I2CM_SS_SCL_LCNT_0           (DWC_OFFSET_MASK + 0x7E0E)
#define HDMITX_DWC_I2CM_FS_SCL_HCNT_1           (DWC_OFFSET_MASK + 0x7E0F)
#define HDMITX_DWC_I2CM_FS_SCL_HCNT_0           (DWC_OFFSET_MASK + 0x7E10)
#define HDMITX_DWC_I2CM_FS_SCL_LCNT_1           (DWC_OFFSET_MASK + 0x7E11)
#define HDMITX_DWC_I2CM_FS_SCL_LCNT_0           (DWC_OFFSET_MASK + 0x7E12)
#define HDMITX_DWC_I2CM_SDA_HOLD                (DWC_OFFSET_MASK + 0x7E13)
/* [  5] updt_rd_vsyncpoll_en */
/* [  4] read_request_en */
/* [  0] read_update */
#define HDMITX_DWC_I2CM_SCDC_UPDATE             (DWC_OFFSET_MASK + 0x7E14)
#define HDMITX_DWC_I2CM_READ_BUFF0              (DWC_OFFSET_MASK + 0x7E20)
#define HDMITX_DWC_I2CM_READ_BUFF1              (DWC_OFFSET_MASK + 0x7E21)
#define HDMITX_DWC_I2CM_READ_BUFF2              (DWC_OFFSET_MASK + 0x7E22)
#define HDMITX_DWC_I2CM_READ_BUFF3              (DWC_OFFSET_MASK + 0x7E23)
#define HDMITX_DWC_I2CM_READ_BUFF4              (DWC_OFFSET_MASK + 0x7E24)
#define HDMITX_DWC_I2CM_READ_BUFF5              (DWC_OFFSET_MASK + 0x7E25)
#define HDMITX_DWC_I2CM_READ_BUFF6              (DWC_OFFSET_MASK + 0x7E26)
#define HDMITX_DWC_I2CM_READ_BUFF7              (DWC_OFFSET_MASK + 0x7E27)
#define HDMITX_DWC_I2CM_SCDC_UPDATE0            (DWC_OFFSET_MASK + 0x7E30)
#define HDMITX_DWC_I2CM_SCDC_UPDATE1            (DWC_OFFSET_MASK + 0x7E31)

#endif  /* __HDMI_TX_REG_H_ */
