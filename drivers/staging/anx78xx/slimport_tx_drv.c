/*
 * Copyright(c) 2015, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/delay.h>
#include <linux/types.h>

#include "anx78xx.h"
#include "slimport_tx_drv.h"

#define XTAL_CLK_M10	pxtal_data[XTAL_27M].xtal_clk_m10
#define XTAL_CLK	pxtal_data[XTAL_27M].xtal_clk

struct slimport {
	int	block_en;	/* HDCP control enable/ disable from AP */

	u8	tx_test_bw;
	bool	tx_test_lt;
	bool	tx_test_edid;

	u8	changed_bandwidth;

	u8	hdmi_dvi_status;
	u8	need_clean_status;

	u8	ds_vid_stb_cntr;
	u8	hdcp_fail_count;

	u8	edid_break;
	u8	edid_checksum;
	u8	edid_blocks[256];

	u8	read_edid_flag;

	u8	down_sample_en;

	struct packet_avi	tx_packet_avi;
	struct packet_spd	tx_packet_spd;
	struct packet_mpeg	tx_packet_mpeg;
	struct audio_info_frame	tx_audioinfoframe;

	struct common_int	common_int_status;
	struct hdmi_rx_int	hdmi_rx_int_status;

	enum sp_tx_state		tx_system_state;
	enum sp_tx_state		tx_system_state_bak;
	enum audio_output_status	tx_ao_state;
	enum video_output_status	tx_vo_state;
	enum sink_connection_status	tx_sc_state;
	enum sp_tx_lt_status		tx_lt_state;
	enum hdcp_status		hcdp_state;
};

static struct slimport sp;

static const u16 chipid_list[] = {
	0x7818,
	0x7816,
	0x7814,
	0x7812,
	0x7810,
	0x7806,
	0x7802
};

static void sp_hdmi_rx_new_vsi_int(struct anx78xx *anx78xx);
static u8 sp_hdcp_cap_check(struct anx78xx *anx78xx);
static void sp_tx_show_information(struct anx78xx *anx78xx);
static void sp_print_sys_state(struct anx78xx *anx78xx, u8 ss);

static int sp_read_reg(struct anx78xx *anx78xx, u8 slave_addr,
		u8 offset, u8 *buf)
{
	u8 ret;
	struct i2c_client *client = anx78xx->client;

	client->addr = (slave_addr >> 1);

	ret = i2c_smbus_read_byte_data(client, offset);
	if (ret < 0) {
		dev_err(&client->dev, "failed to read i2c addr=%x\n",
			slave_addr);
		return ret;
	}

	*buf = ret;

	return 0;
}

static int sp_write_reg(struct anx78xx *anx78xx, u8 slave_addr,
		u8 offset, u8 value)
{
	int ret;
	struct i2c_client *client = anx78xx->client;

	client->addr = (slave_addr >> 1);

	ret = i2c_smbus_write_byte_data(client, offset, value);
	if (ret < 0)
		dev_err(&client->dev, "failed to write i2c addr=%x\n",
			slave_addr);

	return ret;
}

static u8 sp_i2c_read_byte(struct anx78xx *anx78xx,
		u8 dev, u8 offset)
{
	u8 ret;

	sp_read_reg(anx78xx, dev, offset, &ret);
	return ret;
}

static void sp_reg_bit_ctl(struct anx78xx *anx78xx, u8 addr, u8 offset,
			u8 data, bool enable)
{
	u8 c;

	sp_read_reg(anx78xx, addr, offset, &c);
	if (enable) {
		if ((c & data) != data) {
			c |= data;
			sp_write_reg(anx78xx, addr, offset, c);
		}
	} else
		if ((c & data) == data) {
			c &= ~data;
			sp_write_reg(anx78xx, addr, offset, c);
		}
}

static inline void sp_write_reg_or(struct anx78xx *anx78xx, u8 address,
			u8 offset, u8 mask)
{
	sp_write_reg(anx78xx, address, offset,
		sp_i2c_read_byte(anx78xx, address, offset) | mask);
}

static inline void sp_write_reg_and(struct anx78xx *anx78xx, u8 address,
			u8 offset, u8 mask)
{
	sp_write_reg(anx78xx, address, offset,
		sp_i2c_read_byte(anx78xx, address, offset) & mask);
}

static inline void sp_write_reg_and_or(struct anx78xx *anx78xx, u8 address,
			u8 offset, u8 and_mask, u8 or_mask)
{
	sp_write_reg(anx78xx, address, offset,
	  (sp_i2c_read_byte(anx78xx, address, offset) & and_mask) | or_mask);
}

static inline void sp_write_reg_or_and(struct anx78xx *anx78xx, u8 address,
			 u8 offset, u8 or_mask, u8 and_mask)
{
	sp_write_reg(anx78xx, address, offset,
	  (sp_i2c_read_byte(anx78xx, address, offset) | or_mask) & and_mask);
}

static inline void sp_tx_video_mute(struct anx78xx *anx78xx, bool enable)
{
	sp_reg_bit_ctl(anx78xx, TX_P2, VID_CTRL1, VIDEO_MUTE, enable);
}

static inline void hdmi_rx_mute_audio(struct anx78xx *anx78xx, bool enable)
{
	sp_reg_bit_ctl(anx78xx, RX_P0, RX_MUTE_CTRL, AUD_MUTE, enable);
}

static inline void hdmi_rx_mute_video(struct anx78xx *anx78xx, bool enable)
{
	sp_reg_bit_ctl(anx78xx, RX_P0, RX_MUTE_CTRL, VID_MUTE, enable);
}

static inline void sp_tx_addronly_set(struct anx78xx *anx78xx, bool enable)
{
	sp_reg_bit_ctl(anx78xx, TX_P0, AUX_CTRL2, ADDR_ONLY_BIT, enable);
}

static inline void sp_tx_set_link_bw(struct anx78xx *anx78xx, u8 bw)
{
	sp_write_reg(anx78xx, TX_P0, SP_TX_LINK_BW_SET_REG, bw);
}

static inline u8 sp_tx_get_link_bw(struct anx78xx *anx78xx)
{
	return sp_i2c_read_byte(anx78xx, TX_P0, SP_TX_LINK_BW_SET_REG);
}

static inline bool sp_tx_get_pll_lock_status(struct anx78xx *anx78xx)
{
	u8 temp;

	temp = sp_i2c_read_byte(anx78xx, TX_P0, TX_DEBUG1);

	return (temp & DEBUG_PLL_LOCK) != 0 ? true : false;
}

static inline void gen_m_clk_with_downspeading(struct anx78xx *anx78xx)
{
	sp_write_reg_or(anx78xx, TX_P0, SP_TX_M_CALCU_CTRL, M_GEN_CLK_SEL);
}

static inline void gen_m_clk_without_downspeading(struct anx78xx *anx78xx)
{
	sp_write_reg_and(anx78xx, TX_P0, SP_TX_M_CALCU_CTRL, (~M_GEN_CLK_SEL));
}

static inline void hdmi_rx_set_hpd(struct anx78xx *anx78xx, bool enable)
{
	if (enable)
		sp_write_reg_or(anx78xx, TX_P2, SP_TX_VID_CTRL3_REG, HPD_OUT);
	else
		sp_write_reg_and(anx78xx, TX_P2, SP_TX_VID_CTRL3_REG, ~HPD_OUT);
}

static inline void hdmi_rx_set_termination(struct anx78xx *anx78xx,
				bool enable)
{
	if (enable)
		sp_write_reg_and(anx78xx, RX_P0, HDMI_RX_TMDS_CTRL_REG7,
				~TERM_PD);
	else
		sp_write_reg_or(anx78xx, RX_P0, HDMI_RX_TMDS_CTRL_REG7,
				TERM_PD);
}

static inline void sp_tx_clean_hdcp_status(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	sp_write_reg(anx78xx, TX_P0, TX_HDCP_CTRL0, 0x03);
	sp_write_reg_or(anx78xx, TX_P0, TX_HDCP_CTRL0, RE_AUTH);
	usleep_range(2000, 4000);
	dev_dbg(dev, "sp_tx_clean_hdcp_status\n");
}

static inline void sp_tx_link_phy_initialization(struct anx78xx *anx78xx)
{
	sp_write_reg(anx78xx, TX_P2, SP_TX_ANALOG_CTRL0, 0x02);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG0, 0x01);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG10, 0x00);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG1, 0x03);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG11, 0x00);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG2, 0x07);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG12, 0x00);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG3, 0x7f);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG13, 0x00);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG4, 0x71);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG14, 0x0c);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG5, 0x6b);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG15, 0x42);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG6, 0x7f);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG16, 0x1e);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG7, 0x73);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG17, 0x3e);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG8, 0x7f);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG18, 0x72);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG9, 0x7F);
	sp_write_reg(anx78xx, TX_P1, SP_TX_LT_CTRL_REG19, 0x7e);
}

static inline void sp_tx_set_sys_state(struct anx78xx *anx78xx, u8 ss)
{
	struct device *dev = &anx78xx->client->dev;

	dev_dbg(dev, "set: clean_status: %x,\n ", (u16)sp.need_clean_status);

	if ((sp.tx_system_state >= STATE_LINK_TRAINING)
	   && (ss < STATE_LINK_TRAINING))
		sp_write_reg_or(anx78xx, TX_P0, SP_TX_ANALOG_PD_REG, CH0_PD);

	sp.tx_system_state_bak = sp.tx_system_state;
	sp.tx_system_state = ss;
	sp.need_clean_status = 1;
	sp_print_sys_state(anx78xx, sp.tx_system_state);
}

static inline void reg_hardware_reset(struct anx78xx *anx78xx)
{
	sp_write_reg_or(anx78xx, TX_P2, SP_TX_RST_CTRL_REG, HW_RST);
	sp_tx_clean_state_machine();
	sp_tx_set_sys_state(anx78xx, STATE_SP_INITIALIZED);
	msleep(500);
}

static inline void write_dpcd_addr(struct anx78xx *anx78xx, u8 addrh,
			u8 addrm, u8 addrl)
{
	u8 temp;

	if (sp_i2c_read_byte(anx78xx, TX_P0, AUX_ADDR_7_0) != addrl)
		sp_write_reg(anx78xx, TX_P0, AUX_ADDR_7_0, addrl);

	if (sp_i2c_read_byte(anx78xx, TX_P0, AUX_ADDR_15_8) != addrm)
		sp_write_reg(anx78xx, TX_P0, AUX_ADDR_15_8, addrm);

	sp_read_reg(anx78xx, TX_P0, AUX_ADDR_19_16, &temp);

	if ((temp & 0x0F) != (addrh & 0x0F))
		sp_write_reg(anx78xx, TX_P0, AUX_ADDR_19_16,
			(temp  & 0xF0) | addrh);
}

static inline void goto_next_system_state(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	dev_dbg(dev, "next: clean_status: %x,\n ", (u16)sp.need_clean_status);

	sp.tx_system_state_bak = sp.tx_system_state;
	sp.tx_system_state++;
	sp_print_sys_state(anx78xx, sp.tx_system_state);
}

static inline void redo_cur_system_state(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	dev_dbg(dev, "redo: clean_status: %x,\n ", (u16)sp.need_clean_status);

	sp.need_clean_status = 1;
	sp.tx_system_state_bak = sp.tx_system_state;
	sp_print_sys_state(anx78xx, sp.tx_system_state);
}

static inline void system_state_change_with_case(struct anx78xx *anx78xx,
				u8 status)
{
	struct device *dev = &anx78xx->client->dev;

	if (sp.tx_system_state >= status) {
		dev_dbg(dev, "change_case: clean_status: %xm,\n ",
			(u16)sp.need_clean_status);

		if ((sp.tx_system_state >= STATE_LINK_TRAINING)
				&& (status < STATE_LINK_TRAINING))
			sp_write_reg_or(anx78xx, TX_P0, SP_TX_ANALOG_PD_REG,
					CH0_PD);

		sp.need_clean_status = 1;
		sp.tx_system_state_bak = sp.tx_system_state;
		sp.tx_system_state = status;
		sp_print_sys_state(anx78xx, sp.tx_system_state);
	}
}

static void sp_wait_aux_op_finish(struct anx78xx *anx78xx, u8 *err_flag)
{
	u8 cnt;
	u8 c;
	struct device *dev = &anx78xx->client->dev;

	*err_flag = 0;
	cnt = 150;
	while (sp_i2c_read_byte(anx78xx, TX_P0, AUX_CTRL2) & AUX_OP_EN) {
		usleep_range(2000, 4000);
		if ((cnt--) == 0) {
			dev_err(dev, "aux operate failed!\n");
			*err_flag = 1;
			break;
		}
	}

	sp_read_reg(anx78xx, TX_P0, SP_TX_AUX_STATUS, &c);
	if (c & 0x0F) {
		dev_err(dev, "wait aux operation status %.2x\n", (u16)c);
		*err_flag = 1;
	}
}

static void sp_print_sys_state(struct anx78xx *anx78xx, u8 ss)
{
	struct device *dev = &anx78xx->client->dev;

	switch (ss) {
	case STATE_WAITTING_CABLE_PLUG:
		dev_dbg(dev, "-STATE_WAITTING_CABLE_PLUG-\n");
		break;
	case STATE_SP_INITIALIZED:
		dev_dbg(dev, "-STATE_SP_INITIALIZED-\n");
		break;
	case STATE_SINK_CONNECTION:
		dev_dbg(dev, "-STATE_SINK_CONNECTION-\n");
		break;
	case STATE_PARSE_EDID:
		dev_dbg(dev, "-STATE_PARSE_EDID-\n");
		break;
	case STATE_LINK_TRAINING:
		dev_dbg(dev, "-STATE_LINK_TRAINING-\n");
		break;
	case STATE_VIDEO_OUTPUT:
		dev_dbg(dev, "-STATE_VIDEO_OUTPUT-\n");
		break;
	case STATE_HDCP_AUTH:
		dev_dbg(dev, "-STATE_HDCP_AUTH-\n");
		break;
	case STATE_AUDIO_OUTPUT:
		dev_dbg(dev, "-STATE_AUDIO_OUTPUT-\n");
		break;
	case STATE_PLAY_BACK:
		dev_dbg(dev, "-STATE_PLAY_BACK-\n");
		break;
	default:
		dev_err(dev, "system state is error1\n");
		break;
	}
}

static void sp_tx_rst_aux(struct anx78xx *anx78xx)
{
	sp_write_reg_or(anx78xx, TX_P2, RST_CTRL2, AUX_RST);
	sp_write_reg_and(anx78xx, TX_P2, RST_CTRL2, ~AUX_RST);
}

static u8 sp_tx_aux_dpcdread_bytes(struct anx78xx *anx78xx, u8 addrh,
		u8 addrm, u8 addrl, u8 ccount, u8 *pbuf)
{
	u8 c, c1, i;
	u8 bok;
	struct device *dev = &anx78xx->client->dev;

	sp_write_reg(anx78xx, TX_P0, BUF_DATA_COUNT, 0x80);
	c = ((ccount - 1) << 4) | 0x09;
	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, c);
	write_dpcd_addr(anx78xx, addrh, addrm, addrl);
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, AUX_OP_EN);
	usleep_range(2000, 4000);

	sp_wait_aux_op_finish(anx78xx, &bok);
	if (bok == AUX_ERR) {
		dev_err(dev, "aux read failed\n");
		sp_read_reg(anx78xx, TX_P2, SP_TX_INT_STATUS1, &c);
		sp_read_reg(anx78xx, TX_P0, TX_DEBUG1, &c1);
		if (c1 & POLLING_EN) {
			if (c & POLLING_ERR)
				sp_tx_rst_aux(anx78xx);
		} else
			sp_tx_rst_aux(anx78xx);
		return AUX_ERR;
	}

	for (i = 0; i < ccount; i++) {
		sp_read_reg(anx78xx, TX_P0, BUF_DATA_0 + i, &c);
		*(pbuf + i) = c;
		if (i >= MAX_BUF_CNT)
			break;
	}
	return AUX_OK;
}

static u8 sp_tx_aux_dpcdwrite_bytes(struct anx78xx *anx78xx, u8 addrh,
		u8 addrm, u8 addrl, u8 ccount, u8 *pbuf)
{
	u8 c, i, ret;

	c =  ((ccount - 1) << 4) | 0x08;
	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, c);
	write_dpcd_addr(anx78xx, addrh, addrm, addrl);
	for (i = 0; i < ccount; i++) {
		c = *pbuf;
		pbuf++;
		sp_write_reg(anx78xx, TX_P0, BUF_DATA_0 + i, c);

		if (i >= 15)
			break;
	}
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, AUX_OP_EN);
	sp_wait_aux_op_finish(anx78xx, &ret);
	return ret;
}

static u8 sp_tx_aux_dpcdwrite_byte(struct anx78xx *anx78xx, u8 addrh,
		u8 addrm, u8 addrl, u8 data1)
{
	u8 ret;

	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, 0x08);
	write_dpcd_addr(anx78xx, addrh, addrm, addrl);
	sp_write_reg(anx78xx, TX_P0, BUF_DATA_0, data1);
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, AUX_OP_EN);
	sp_wait_aux_op_finish(anx78xx, &ret);
	return ret;
}

static void sp_block_power_ctrl(struct anx78xx *anx78xx,
		enum sp_tx_power_block sp_tx_pd_block, u8 power)
{
	struct device *dev = &anx78xx->client->dev;

	if (power == SP_POWER_ON)
		sp_write_reg_and(anx78xx, TX_P2, SP_POWERD_CTRL_REG,
			~sp_tx_pd_block);
	else
		sp_write_reg_or(anx78xx, TX_P2, SP_POWERD_CTRL_REG,
			sp_tx_pd_block);

	 dev_dbg(dev, "sp_tx_power_on: %.2x\n", (u16)sp_tx_pd_block);
}

static void sp_vbus_power_off(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	int i;

	for (i = 0; i < 5; i++) {
		sp_write_reg_and(anx78xx, TX_P2, TX_PLL_FILTER5,
				(~P5V_PROTECT_PD & ~SHORT_PROTECT_PD));
		sp_write_reg_or(anx78xx, TX_P2, TX_PLL_FILTER, V33_SWITCH_ON);
		if (!(sp_i2c_read_byte(anx78xx, TX_P2, TX_PLL_FILTER5)
		    & 0xc0)) {
			dev_dbg(dev, "3.3V output enabled\n");
			break;
		}

		dev_dbg(dev, "VBUS power can not be supplied\n");
	}
}

void sp_tx_clean_state_machine(void)
{
	sp.tx_system_state = STATE_WAITTING_CABLE_PLUG;
	sp.tx_system_state_bak = STATE_WAITTING_CABLE_PLUG;
	sp.tx_sc_state = SC_INIT;
	sp.tx_lt_state = LT_INIT;
	sp.hcdp_state = HDCP_CAPABLE_CHECK;
	sp.tx_vo_state = VO_WAIT_VIDEO_STABLE;
	sp.tx_ao_state = AO_INIT;
}

u8 sp_tx_cur_states(void)
{
	return sp.tx_system_state;
}

void sp_tx_variable_init(void)
{
	u16 i;

	sp.block_en = 1;

	sp.tx_system_state = STATE_WAITTING_CABLE_PLUG;
	sp.tx_system_state_bak = STATE_WAITTING_CABLE_PLUG;

	sp.edid_break = 0;
	sp.read_edid_flag = 0;
	sp.edid_checksum = 0;
	for (i = 0; i < 256; i++)
		sp.edid_blocks[i] = 0;

	sp.tx_lt_state = LT_INIT;
	sp.hcdp_state = HDCP_CAPABLE_CHECK;
	sp.need_clean_status = 0;
	sp.tx_sc_state = SC_INIT;
	sp.tx_vo_state = VO_WAIT_VIDEO_STABLE;
	sp.tx_ao_state = AO_INIT;
	sp.changed_bandwidth = LINK_5P4G;
	sp.hdmi_dvi_status = HDMI_MODE;

	sp.tx_test_lt = 0;
	sp.tx_test_bw = 0;
	sp.tx_test_edid = 0;

	sp.ds_vid_stb_cntr = 0;
	sp.hdcp_fail_count = 0;

}

static void hdmi_rx_tmds_phy_initialization(struct anx78xx *anx78xx)
{
	sp_write_reg(anx78xx, RX_P0, HDMI_RX_TMDS_CTRL_REG2, 0xa9);
	sp_write_reg(anx78xx, RX_P0, HDMI_RX_TMDS_CTRL_REG7, 0x80);

	sp_write_reg(anx78xx, RX_P0, HDMI_RX_TMDS_CTRL_REG1, 0x90);
	sp_write_reg(anx78xx, RX_P0, HDMI_RX_TMDS_CTRL_REG6, 0x92);
	sp_write_reg(anx78xx, RX_P0, HDMI_RX_TMDS_CTRL_REG20, 0xf2);
}

static void hdmi_rx_initialization(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	sp_write_reg(anx78xx, RX_P0, RX_MUTE_CTRL, AUD_MUTE | VID_MUTE);
	sp_write_reg_or(anx78xx, RX_P0, RX_CHIP_CTRL,
		MAN_HDMI5V_DET | PLLLOCK_CKDT_EN | DIGITAL_CKDT_EN);

	sp_write_reg_or(anx78xx, RX_P0, RX_SRST, HDCP_MAN_RST | SW_MAN_RST |
		TMDS_RST | VIDEO_RST);
	sp_write_reg_and(anx78xx, RX_P0, RX_SRST, ~HDCP_MAN_RST &
		~SW_MAN_RST & ~TMDS_RST & ~VIDEO_RST);

	sp_write_reg_or(anx78xx, RX_P0, RX_AEC_EN0, AEC_EN06 | AEC_EN05);
	sp_write_reg_or(anx78xx, RX_P0, RX_AEC_EN2, AEC_EN21);
	sp_write_reg_or(anx78xx, RX_P0, RX_AEC_CTRL, AVC_EN | AAC_OE | AAC_EN);

	sp_write_reg_and(anx78xx, RX_P0, RX_SYS_PWDN1, ~PWDN_CTRL);

	sp_write_reg_or(anx78xx, RX_P0, RX_VID_DATA_RNG, R2Y_INPUT_LIMIT);
	sp_write_reg(anx78xx, RX_P0, 0x65, 0xc4);
	sp_write_reg(anx78xx, RX_P0, 0x66, 0x18);

	/* enable DDC stretch */
	sp_write_reg(anx78xx, TX_P0, TX_EXTRA_ADDR, 0x50);

	hdmi_rx_tmds_phy_initialization(anx78xx);
	hdmi_rx_set_hpd(anx78xx, 0);
	hdmi_rx_set_termination(anx78xx, 0);
	dev_dbg(dev, "HDMI Rx is initialized...\n");
}

struct anx78xx_clock_data const pxtal_data[XTAL_CLK_NUM] = {
	{19, 192},
	{24, 240},
	{25, 250},
	{26, 260},
	{27, 270},
	{38, 384},
	{52, 520},
	{27, 270},
};

static void xtal_clk_sel(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	dev_dbg(dev, "define XTAL_CLK:  %x\n ", (u16)XTAL_27M);
	sp_write_reg_and_or(anx78xx, TX_P2,
			TX_ANALOG_DEBUG2, (~0x3c), 0x3c & (XTAL_27M << 2));
	sp_write_reg(anx78xx, TX_P0, 0xEC, (u8)(((u16)XTAL_CLK_M10)));
	sp_write_reg(anx78xx, TX_P0, 0xED,
		(u8)(((u16)XTAL_CLK_M10 & 0xFF00) >> 2) | XTAL_CLK);

	sp_write_reg(anx78xx, TX_P0,
			I2C_GEN_10US_TIMER0, (u8)(((u16)XTAL_CLK_M10)));
	sp_write_reg(anx78xx, TX_P0, I2C_GEN_10US_TIMER1,
			(u8)(((u16)XTAL_CLK_M10 & 0xFF00) >> 8));
	sp_write_reg(anx78xx, TX_P0, 0xBF, (u8)(((u16)XTAL_CLK - 1)));

	sp_write_reg_and_or(anx78xx, RX_P0, 0x49, 0x07,
			(u8)(((((u16)XTAL_CLK) >> 1) - 2) << 3));

}

void sp_tx_initialization(struct anx78xx *anx78xx)
{
	sp_write_reg(anx78xx, TX_P0, AUX_CTRL2, 0x30);
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, 0x08);

	sp_write_reg_and(anx78xx, TX_P0, TX_HDCP_CTRL,
			(u8)(~AUTO_EN) & (~AUTO_START));
	sp_write_reg(anx78xx, TX_P0, OTP_KEY_PROTECT1, OTP_PSW1);
	sp_write_reg(anx78xx, TX_P0, OTP_KEY_PROTECT2, OTP_PSW2);
	sp_write_reg(anx78xx, TX_P0, OTP_KEY_PROTECT3, OTP_PSW3);
	sp_write_reg_or(anx78xx, TX_P0, HDCP_KEY_CMD, DISABLE_SYNC_HDCP);
	sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL8_REG, VID_VRES_TH);

	sp_write_reg(anx78xx, TX_P0, HDCP_AUTO_TIMER, HDCP_AUTO_TIMER_VAL);
	sp_write_reg_or(anx78xx, TX_P0, TX_HDCP_CTRL, LINK_POLLING);

	sp_write_reg_or(anx78xx, TX_P0, TX_LINK_DEBUG, M_VID_DEBUG);
	sp_write_reg_or(anx78xx, TX_P2, TX_ANALOG_DEBUG2, POWERON_TIME_1P5MS);

	xtal_clk_sel(anx78xx);
	sp_write_reg(anx78xx, TX_P0, AUX_DEFER_CTRL, 0x8C);

	sp_write_reg_or(anx78xx, TX_P0, TX_DP_POLLING, AUTO_POLLING_DISABLE);
	/*
	 * Short the link intergrity check timer to speed up bstatus
	 * polling for HDCP CTS item 1A-07
	 */
	sp_write_reg(anx78xx, TX_P0, SP_TX_LINK_CHK_TIMER, 0x1d);
	sp_write_reg_or(anx78xx, TX_P0, TX_MISC, EQ_TRAINING_LOOP);

	sp_write_reg_or(anx78xx, TX_P0, SP_TX_ANALOG_PD_REG, CH0_PD);

	sp_write_reg(anx78xx, TX_P2, SP_TX_INT_CTRL_REG, 0X01);
	/* disable HDCP mismatch function for VGA dongle */
	sp_tx_link_phy_initialization(anx78xx);
	gen_m_clk_with_downspeading(anx78xx);

	sp.down_sample_en = 0;
}

bool sp_chip_detect(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u16 id;
	u8 idh = 0, idl = 0;
	int i;

	anx78xx_poweron(anx78xx);

	/* check chip id */
	sp_read_reg(anx78xx, TX_P2, SP_TX_DEV_IDL_REG, &idl);
	sp_read_reg(anx78xx, TX_P2, SP_TX_DEV_IDH_REG, &idh);
	id = idl | (idh << 8);

	dev_dbg(dev, "CHIPID: ANX%x\n", id & 0xffff);

	for (i = 0; i < sizeof(chipid_list) / sizeof(u16); i++) {
		if (id == chipid_list[i])
			return true;
	}

	return false;
}

static void sp_waiting_cable_plug_process(struct anx78xx *anx78xx)
{
	sp_tx_variable_init();
	anx78xx_poweron(anx78xx);
	goto_next_system_state(anx78xx);
}

/*
 * Check if it is ANALOGIX dongle.
 */
static const u8 ANX_OUI[3] = {0x00, 0x22, 0xB9};

static u8 is_anx_dongle(struct anx78xx *anx78xx)
{
	u8 buf[3];

	/* 0x0500~0x0502: BRANCH_IEEE_OUI */
	sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x05, 0x00, 3, buf);

	if (!memcmp(buf, ANX_OUI, 3))
		return 1;

	return 0;
}

static void sp_tx_get_rx_bw(struct anx78xx *anx78xx, u8 *bw)
{
	if (is_anx_dongle(anx78xx))
		*bw = LINK_6P75G;	/* just for debug */
	else
		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x00,
					DPCD_MAX_LINK_RATE, 1, bw);
}

static u8 sp_tx_get_cable_type(struct anx78xx *anx78xx,
			enum cable_type_status det_cable_type_state)
{
	struct device *dev = &anx78xx->client->dev;

	u8 ds_port_preset;
	u8 aux_status;
	u8 data_buf[16];
	u8 cur_cable_type;

	ds_port_preset = 0;
	cur_cable_type = DWN_STRM_IS_NULL;

	aux_status = sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x00, 0x05, 1,
					&ds_port_preset);

	dev_dbg(dev, "DPCD 0x005: %x\n", (int)ds_port_preset);

	switch (det_cable_type_state) {
	case CHECK_AUXCH:
		if (AUX_OK == aux_status) {
			sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x00, 0, 0x0c,
					data_buf);
			det_cable_type_state = GETTED_CABLE_TYPE;
		} else {
			dev_err(dev, " AUX access error\n");
			break;
		}
	case GETTED_CABLE_TYPE:
		switch ((ds_port_preset  & (BIT(1) | BIT(2))) >> 1) {
		case 0x00:
			cur_cable_type = DWN_STRM_IS_DIGITAL;
			dev_dbg(dev, "Downstream is DP dongle.\n");
			break;
		case 0x01:
		case 0x03:
			cur_cable_type = DWN_STRM_IS_ANALOG;
			dev_dbg(dev, "Downstream is VGA dongle.\n");
			break;
		case 0x02:
			cur_cable_type = DWN_STRM_IS_HDMI;
			dev_dbg(dev, "Downstream is HDMI dongle.\n");
			break;
		default:
			cur_cable_type = DWN_STRM_IS_NULL;
			dev_err(dev, "Downstream can not recognized.\n");
			break;
		}
	default:
		break;
	}
	return cur_cable_type;
}

static u8 sp_tx_get_dp_connection(struct anx78xx *anx78xx)
{
	u8 c;

	if (AUX_OK != sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02,
				DPCD_SINK_COUNT, 1, &c))
		return 0;

	if (c & 0x1f) {
		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x00, 0x04, 1, &c);
		if (c & 0x20) {
			sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x06, 0x00, 1,
						&c);
			/*
			 * Bit 5 = SET_DN_DEVICE_DP_PWR_5V
			 * Bit 6 = SET_DN_DEVICE_DP_PWR_12V
			 * Bit 7 = SET_DN_DEVICE_DP_PWR_18V
			 */
			c = c & 0x1F;
			sp_tx_aux_dpcdwrite_byte(anx78xx, 0x00, 0x06, 0x00,
						c | 0x20);
		}
		return 1;
	} else
		return 0;
}

static u8 sp_tx_get_downstream_connection(struct anx78xx *anx78xx)
{
	return sp_tx_get_dp_connection(anx78xx);
}

static void sp_sink_connection(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	switch (sp.tx_sc_state) {
	case SC_INIT:
		sp.tx_sc_state++;
	case SC_CHECK_CABLE_TYPE:
	case SC_WAITTING_CABLE_TYPE:
	default:
		if (sp_tx_get_cable_type(anx78xx, CHECK_AUXCH) ==
		   DWN_STRM_IS_NULL) {
			sp.tx_sc_state++;
			if (sp.tx_sc_state >= SC_WAITTING_CABLE_TYPE) {
				sp.tx_sc_state = SC_NOT_CABLE;
				dev_dbg(dev, "Can not get cable type!\n");
			}
			break;
		}

		sp.tx_sc_state = SC_SINK_CONNECTED;
	case SC_SINK_CONNECTED:
		if (sp_tx_get_downstream_connection(anx78xx))
			goto_next_system_state(anx78xx);
		break;
	case SC_NOT_CABLE:
		sp_vbus_power_off(anx78xx);
		reg_hardware_reset(anx78xx);
		break;
	}
}

/******************start EDID process********************/
static void sp_tx_enable_video_input(struct anx78xx *anx78xx, u8 enable)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c;

	sp_read_reg(anx78xx, TX_P2, VID_CTRL1, &c);
	if (enable) {
		c = (c & 0xf7) | VIDEO_EN;
		sp_write_reg(anx78xx, TX_P2, VID_CTRL1, c);
		dev_dbg(dev, "Slimport Video is enabled!\n");

	} else {
		c &= ~VIDEO_EN;
		sp_write_reg(anx78xx, TX_P2, VID_CTRL1, c);
		dev_dbg(dev, "Slimport Video is disabled!\n");
	}
}

static u8 sp_get_edid_detail(u8 *data_buf)
{
	u16 pixclock_edid;

	pixclock_edid = ((((u16)data_buf[1] << 8))
			| ((u16)data_buf[0] & 0xFF));
	if (pixclock_edid <= 5300)
		return LINK_1P62G;
	else if ((pixclock_edid > 5300) && (pixclock_edid <= 8900))
		return LINK_2P7G;
	else if ((pixclock_edid > 8900) && (pixclock_edid <= 18000))
		return LINK_5P4G;
	else
		return LINK_6P75G;
}

static u8 sp_parse_edid_to_get_bandwidth(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 desc_offset = 0;
	u8 i, bandwidth, temp;

	bandwidth = LINK_1P62G;
	temp = LINK_1P62G;
	i = 0;
	while (i < 4 && sp.edid_blocks[0x36 + desc_offset] != 0) {
		temp = sp_get_edid_detail(sp.edid_blocks + 0x36 + desc_offset);
		dev_dbg(dev, "bandwidth via EDID : %x\n", (u16)temp);
		if (bandwidth < temp)
			bandwidth = temp;
		if (bandwidth > LINK_5P4G)
			break;
		desc_offset += 0x12;
		++i;
	}
	return bandwidth;
}

static void sp_tx_aux_wr(struct anx78xx *anx78xx, u8 offset)
{
	sp_write_reg(anx78xx, TX_P0, BUF_DATA_0, offset);
	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, 0x04);
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, AUX_OP_EN);
	sp_wait_aux_op_finish(anx78xx, &sp.edid_break);
}

static void sp_tx_aux_rd(struct anx78xx *anx78xx, u8 len_cmd)
{
	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, len_cmd);
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, AUX_OP_EN);
	sp_wait_aux_op_finish(anx78xx, &sp.edid_break);
}

static u8 sp_tx_get_edid_block(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c;

	sp_tx_aux_wr(anx78xx, 0x7e);
	sp_tx_aux_rd(anx78xx, 0x01);
	sp_read_reg(anx78xx, TX_P0, BUF_DATA_0, &c);
	dev_dbg(dev, "EDID Block = %d\n", (int)(c + 1));

	if (c > 3)
		c = 1;
	return c;
}

static void sp_edid_read(struct anx78xx *anx78xx, u8 offset,
			u8 *pblock_buf)
{
	u8 data_cnt, cnt;
	u8 c;

	sp_tx_aux_wr(anx78xx, offset);
	sp_tx_aux_rd(anx78xx, 0xf5);
	data_cnt = 0;
	cnt = 0;

	while ((data_cnt) < 16)	{
		sp_read_reg(anx78xx, TX_P0, BUF_DATA_COUNT, &c);

		if ((c & 0x1f) != 0) {
			data_cnt = data_cnt + c;
			do {
				sp_read_reg(anx78xx, TX_P0, BUF_DATA_0 + c - 1,
					&(pblock_buf[c - 1]));
				if (c == 1)
					break;
			} while (c--);
		} else {
			if (cnt++ <= 2) {
				sp_tx_rst_aux(anx78xx);
				c = 0x05 | ((0x0f - data_cnt) << 4);
				sp_tx_aux_rd(anx78xx, c);
			} else {
				 sp.edid_break = 1;
				 break;
			}
		}
	}
	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, 0x01);
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, ADDR_ONLY_BIT | AUX_OP_EN);
	sp_wait_aux_op_finish(anx78xx, &sp.edid_break);
	sp_tx_addronly_set(anx78xx, 0);
}

static void sp_tx_edid_read_initial(struct anx78xx *anx78xx)
{
	sp_write_reg(anx78xx, TX_P0, AUX_ADDR_7_0, 0x50);
	sp_write_reg(anx78xx, TX_P0, AUX_ADDR_15_8, 0);
	sp_write_reg_and(anx78xx, TX_P0, AUX_ADDR_19_16, 0xf0);
}

static void sp_seg_edid_read(struct anx78xx *anx78xx,
			u8 segment, u8 offset)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c, cnt;
	int i;

	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, 0x04);

	sp_write_reg(anx78xx, TX_P0, AUX_ADDR_7_0, 0x30);

	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, ADDR_ONLY_BIT | AUX_OP_EN);

	sp_read_reg(anx78xx, TX_P0, AUX_CTRL2, &c);

	sp_wait_aux_op_finish(anx78xx, &sp.edid_break);
	sp_read_reg(anx78xx, TX_P0, AUX_CTRL, &c);

	sp_write_reg(anx78xx, TX_P0, BUF_DATA_0, segment);

	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, 0x04);

	sp_write_reg_and_or(anx78xx, TX_P0, AUX_CTRL2, ~ADDR_ONLY_BIT,
			AUX_OP_EN);
	cnt = 0;
	sp_read_reg(anx78xx, TX_P0, AUX_CTRL2, &c);
	while (c & AUX_OP_EN) {
		usleep_range(1000, 2000);
		cnt++;
		if (cnt == 10) {
			dev_dbg(dev, "write break");
			sp_tx_rst_aux(anx78xx);
			cnt = 0;
			sp.edid_break = 1;
			return;
		}
		sp_read_reg(anx78xx, TX_P0, AUX_CTRL2, &c);

	}

	sp_write_reg(anx78xx, TX_P0, AUX_ADDR_7_0, 0x50);

	sp_tx_aux_wr(anx78xx, offset);

	sp_tx_aux_rd(anx78xx, 0xf5);
	cnt = 0;
	for (i = 0; i < 16; i++) {
		sp_read_reg(anx78xx, TX_P0, BUF_DATA_COUNT, &c);
		while ((c & 0x1f) == 0) {
			usleep_range(2000, 4000);
			cnt++;
			sp_read_reg(anx78xx, TX_P0, BUF_DATA_COUNT, &c);
			if (cnt == 10) {
				dev_dbg(dev, "read break");
				sp_tx_rst_aux(anx78xx);
				sp.edid_break = 1;
				return;
			}
		}


		sp_read_reg(anx78xx, TX_P0, BUF_DATA_0+i, &c);
	}

	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, 0x01);
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, ADDR_ONLY_BIT | AUX_OP_EN);
	sp_write_reg_and(anx78xx, TX_P0, AUX_CTRL2, ~ADDR_ONLY_BIT);
	sp_read_reg(anx78xx, TX_P0, AUX_CTRL2, &c);
	while (c & AUX_OP_EN)
		sp_read_reg(anx78xx, TX_P0, AUX_CTRL2, &c);
}

static bool sp_edid_checksum_result(u8 *pbuf)
{
	u8 cnt, checksum;

	checksum = 0;

	for (cnt = 0; cnt < 0x80; cnt++)
		checksum = checksum + pbuf[cnt];

	sp.edid_checksum = checksum - pbuf[0x7f];
	sp.edid_checksum = ~sp.edid_checksum + 1;

	return checksum == 0 ? 1 : 0;
}

static void sp_edid_header_result(struct anx78xx *anx78xx, u8 *pbuf)
{
	struct device *dev = &anx78xx->client->dev;

	if ((pbuf[0] == 0) && (pbuf[7] == 0) && (pbuf[1] == 0xff)
		&& (pbuf[2] == 0xff) && (pbuf[3] == 0xff)
		&& (pbuf[4] == 0xff) && (pbuf[5] == 0xff) && (pbuf[6] == 0xff))
		dev_dbg(dev, "Good EDID header!\n");
	else
		dev_err(dev, "Bad EDID header!\n");
}

static void sp_check_edid_data(struct anx78xx *anx78xx, u8 *pblock_buf)
{
	struct device *dev = &anx78xx->client->dev;
	u8 i;

	sp_edid_header_result(anx78xx, pblock_buf);
	for (i = 0; i <= ((pblock_buf[0x7e] > 1) ? 1 : pblock_buf[0x7e]); i++) {
		if (!sp_edid_checksum_result(pblock_buf + i * 128))
			dev_err(dev, "Block %x edid checksum error\n", (u16)i);
		else
			dev_dbg(dev, "Block %x edid checksum OK\n", (u16)i);
	}
}

static bool sp_tx_edid_read(struct anx78xx *anx78xx, u8 *pedid_blocks_buf)
{
	struct device *dev = &anx78xx->client->dev;
	u8 offset = 0;
	u8 count, blocks_num;
	u8 pblock_buf[16];
	u8 i, j, c;

	sp.edid_break = 0;
	sp_tx_edid_read_initial(anx78xx);
	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, 0x04);
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, 0x03);
	sp_wait_aux_op_finish(anx78xx, &sp.edid_break);
	sp_tx_addronly_set(anx78xx, 0);

	blocks_num = sp_tx_get_edid_block(anx78xx);

	count = 0;
	do {
		switch (count) {
		case 0:
		case 1:
			for (i = 0; i < 8; i++) {
				offset = (i + count * 8) * 16;
				sp_edid_read(anx78xx, offset, pblock_buf);
				if (sp.edid_break == 1)
					break;
				for (j = 0; j < 16; j++) {
					pedid_blocks_buf[offset + j]
						= pblock_buf[j];
				}
			}
			break;
		case 2:
			offset = 0x00;
			for (j = 0; j < 8; j++) {
				if (sp.edid_break == 1)
					break;
				sp_seg_edid_read(anx78xx, count / 2, offset);
				offset = offset + 0x10;
			}
			break;
		case 3:
			offset = 0x80;
			for (j = 0; j < 8; j++) {
				if (sp.edid_break == 1)
					break;
				sp_seg_edid_read(anx78xx, count / 2, offset);
				offset = offset + 0x10;
			}
			break;
		default:
			break;
		}
		count++;
		if (sp.edid_break == 1)
			break;
	} while (blocks_num >= count);

	sp_tx_rst_aux(anx78xx);
	if (sp.read_edid_flag == 0) {
		sp_check_edid_data(anx78xx, pedid_blocks_buf);
		sp.read_edid_flag = 1;
	}
	sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x18, 1, &c);
	if (c & 0x04) {
		dev_dbg(dev, "check sum = %.2x\n", (u16)sp.edid_checksum);
		c = sp.edid_checksum;
		sp_tx_aux_dpcdwrite_bytes(anx78xx, 0x00, 0x02, 0x61, 1, &c);
		sp.tx_test_edid = 1;
		c = 0x04;
		sp_tx_aux_dpcdwrite_bytes(anx78xx, 0x00, 0x02, 0x60, 1, &c);
		dev_dbg(dev, "Test EDID done\n");

	}

	return 0;
}

static bool sp_check_with_pre_edid(struct anx78xx *anx78xx, u8 *org_buf)
{
	struct device *dev = &anx78xx->client->dev;
	u8 i;
	u8 temp_buf[16];
	bool return_flag;

	return_flag = 0;
	sp.edid_break = 0;
	sp_tx_edid_read_initial(anx78xx);
	sp_write_reg(anx78xx, TX_P0, AUX_CTRL, 0x04);
	sp_write_reg_or(anx78xx, TX_P0, AUX_CTRL2, 0x03);
	sp_wait_aux_op_finish(anx78xx, &sp.edid_break);
	sp_tx_addronly_set(anx78xx, 0);

	sp_edid_read(anx78xx, 0x70, temp_buf);

	if (sp.edid_break == 0) {

		for (i = 0; i < 16; i++) {
			if (org_buf[0x70 + i] != temp_buf[i]) {
				dev_dbg(dev, "%s\n",
					"different checksum and blocks num\n");
				return_flag = 1;
				break;
			}
		}
	} else
		return_flag = 1;

	if (return_flag)
		goto return_point;

	sp_edid_read(anx78xx, 0x08, temp_buf);
	if (sp.edid_break == 0) {
		for (i = 0; i < 16; i++) {
			if (org_buf[i + 8] != temp_buf[i]) {
				dev_dbg(dev, "different edid information\n");
				return_flag = 1;
				break;
			}
		}
	} else
		return_flag = 1;

return_point:
	sp_tx_rst_aux(anx78xx);

	return return_flag;
}

static void sp_edid_process(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 temp_value, temp_value1;
	u8 i;

	dev_dbg(dev, "edid_process\n");

	if (sp.read_edid_flag == 1) {
		if (sp_check_with_pre_edid(anx78xx, sp.edid_blocks))
			sp.read_edid_flag = 0;
		else
			dev_dbg(dev, "Don`t need to read edid!\n");
	}

	if (sp.read_edid_flag == 0) {
		sp_tx_edid_read(anx78xx, sp.edid_blocks);
		if (sp.edid_break)
			dev_err(dev, "ERR:EDID corruption!\n");
	}

	/* Release the HPD after the OTP loaddown */
	i = 10;
	do {
		if ((sp_i2c_read_byte(anx78xx, TX_P0, HDCP_KEY_STATUS) & 0x01))
			break;

		dev_dbg(dev, "waiting HDCP KEY loaddown\n");
		usleep_range(1000, 2000);
	} while (--i);

	sp_write_reg(anx78xx, RX_P0, HDMI_RX_INT_MASK1_REG, 0xe2);
	hdmi_rx_set_hpd(anx78xx, 1);
	dev_dbg(dev, "hdmi_rx_set_hpd 1 !\n");

	hdmi_rx_set_termination(anx78xx, 1);

	sp_tx_get_rx_bw(anx78xx, &temp_value);
	dev_dbg(dev, "RX BW %x\n", (u16)temp_value);

	temp_value1 = sp_parse_edid_to_get_bandwidth(anx78xx);
	if (temp_value <= temp_value1)
		temp_value1 = temp_value;

	dev_dbg(dev, "set link bw in edid %x\n", (u16)temp_value1);
	sp.changed_bandwidth = temp_value1;
	goto_next_system_state(anx78xx);
}
/******************End EDID process********************/

/******************start Link training process********************/
static void sp_tx_lvttl_bit_mapping(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c, colorspace;
	u8 vid_bit;

	vid_bit = 0;
	sp_read_reg(anx78xx, RX_P1, HDMI_RX_AVI_DATA00_REG, &colorspace);
	colorspace &= 0x60;

	switch (((sp_i2c_read_byte(anx78xx, RX_P0, HDMI_RX_VIDEO_STATUS_REG1)
		& COLOR_DEPTH) >> 4)) {
	default:
	case HDMI_LEGACY:
		c = IN_BPC_8BIT;
		vid_bit = 0;
		break;
	case HDMI_24BIT:
		c = IN_BPC_8BIT;
		if (colorspace == 0x20)
			vid_bit = 5;
		else
			vid_bit = 1;
		break;
	case HDMI_30BIT:
		c = IN_BPC_10BIT;
		if (colorspace == 0x20)
			vid_bit = 6;
		else
			vid_bit = 2;
		break;
	case HDMI_36BIT:
		c = IN_BPC_12BIT;
		if (colorspace == 0x20)
			vid_bit = 6;
		else
			vid_bit = 3;
		break;

	}
	/*
	 * For down sample video (12bit, 10bit ---> 8bit),
	 * this register don`t change
	 */
	if (sp.down_sample_en == 0)
		sp_write_reg_and_or(anx78xx, TX_P2,
			SP_TX_VID_CTRL2_REG, 0x8c, colorspace >> 5 | c);

	/* Patch: for 10bit video must be set this value to 12bit by someone */
	if (sp.down_sample_en == 1 && c == IN_BPC_10BIT)
		vid_bit = 3;

	sp_write_reg_and_or(anx78xx, TX_P2,
		BIT_CTRL_SPECIFIC, 0x00, ENABLE_BIT_CTRL | vid_bit << 1);

	if (sp.tx_test_edid) {
		sp_write_reg_and(anx78xx, TX_P2, SP_TX_VID_CTRL2_REG, 0x8f);
		dev_dbg(dev, "***color space is set to 18bit***\n");
	}

	if (colorspace) {
		sp_write_reg(anx78xx, TX_P0, SP_TX_VID_BLANK_SET1, 0x80);
		sp_write_reg(anx78xx, TX_P0, SP_TX_VID_BLANK_SET2, 0x00);
		sp_write_reg(anx78xx, TX_P0, SP_TX_VID_BLANK_SET3, 0x80);
	} else {
		sp_write_reg(anx78xx, TX_P0, SP_TX_VID_BLANK_SET1, 0x0);
		sp_write_reg(anx78xx, TX_P0, SP_TX_VID_BLANK_SET2, 0x0);
		sp_write_reg(anx78xx, TX_P0, SP_TX_VID_BLANK_SET3, 0x0);
	}
}

static ulong sp_tx_pclk_calc(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	ulong str_plck;
	u16 vid_counter;
	u8 c;

	sp_read_reg(anx78xx, RX_P0, 0x8d, &c);
	vid_counter = c;
	vid_counter = vid_counter << 8;
	sp_read_reg(anx78xx, RX_P0, 0x8c, &c);
	vid_counter |= c;
	str_plck = ((ulong)vid_counter * XTAL_CLK_M10)  >> 12;
	dev_dbg(dev, "PCLK = %d.%d\n", (((u16)(str_plck))/10),
		((u16)str_plck - (((u16)str_plck/10)*10)));
	return str_plck;
}

static u8 sp_tx_bw_lc_sel(struct anx78xx *anx78xx, ulong pclk)
{
	struct device *dev = &anx78xx->client->dev;
	ulong pixel_clk;
	u8 c1;

	switch (((sp_i2c_read_byte(anx78xx, RX_P0, HDMI_RX_VIDEO_STATUS_REG1)
		& COLOR_DEPTH) >> 4)) {
	case HDMI_LEGACY:
	case HDMI_24BIT:
	default:
		pixel_clk = pclk;
		break;
	case HDMI_30BIT:
		pixel_clk = (pclk * 5) >> 2;
		break;
	case HDMI_36BIT:
		pixel_clk = (pclk * 3) >> 1;
		break;
	}
	dev_dbg(dev, "pixel_clk = %d.%d\n", (((u16)(pixel_clk))/10),
		((u16)pixel_clk - (((u16)pixel_clk/10)*10)));

	sp.down_sample_en = 0;
	if (pixel_clk <= 530)
		c1 = LINK_1P62G;
	else if ((530 < pixel_clk) && (pixel_clk <= 890))
		c1 = LINK_2P7G;
	else if ((890 < pixel_clk) && (pixel_clk <= 1800))
		c1 = LINK_5P4G;
	else {
		 c1 = LINK_6P75G;
		 if (pixel_clk > 2240)
			sp.down_sample_en = 1;
	}

	if (sp_tx_get_link_bw(anx78xx) != c1) {
		sp.changed_bandwidth = c1;
		dev_dbg(dev, "%s! %.2x\n",
		    "different bandwidth between sink support and cur video",
		    (u16)c1);
		return 1;
	}
	return 0;
}

static void sp_tx_spread_enable(struct anx78xx *anx78xx, u8 benable)
{
	u8 c;

	sp_read_reg(anx78xx, TX_P0, SP_TX_DOWN_SPREADING_CTRL1, &c);

	if (benable) {
		c |= SP_TX_SSC_DWSPREAD;
		sp_write_reg(anx78xx, TX_P0, SP_TX_DOWN_SPREADING_CTRL1, c);

		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x01,
			DPCD_DOWNSPREAD_CTRL, 1, &c);
		c |= SPREAD_AMPLITUDE;
		sp_tx_aux_dpcdwrite_byte(anx78xx, 0x00, 0x01,
					DPCD_DOWNSPREAD_CTRL, c);
	} else {
		c &= ~SP_TX_SSC_DISABLE;
		sp_write_reg(anx78xx, TX_P0, SP_TX_DOWN_SPREADING_CTRL1, c);

		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x01,
			DPCD_DOWNSPREAD_CTRL, 1, &c);
		c &= ~SPREAD_AMPLITUDE;
		sp_tx_aux_dpcdwrite_byte(anx78xx, 0x00, 0x01,
			DPCD_DOWNSPREAD_CTRL, c);
	}
}

static void sp_tx_config_ssc(struct anx78xx *anx78xx,
			enum sp_ssc_dep sscdep)
{
	sp_write_reg(anx78xx, TX_P0, SP_TX_DOWN_SPREADING_CTRL1, 0x0);
	sp_write_reg(anx78xx, TX_P0, SP_TX_DOWN_SPREADING_CTRL1, sscdep);
	sp_tx_spread_enable(anx78xx, 1);
}

static void sp_tx_enhancemode_set(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c;

	sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x00, DPCD_MAX_LANE_COUNT,
				1, &c);
	if (c & ENHANCED_FRAME_CAP) {
		sp_write_reg_or(anx78xx, TX_P0, SP_TX_SYS_CTRL4_REG,
				ENHANCED_MODE);

		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x01,
			DPCD_LANE_COUNT_SET, 1, &c);
		c |= ENHANCED_FRAME_EN;
		sp_tx_aux_dpcdwrite_byte(anx78xx, 0x00, 0x01,
			DPCD_LANE_COUNT_SET, c);

		dev_dbg(dev, "Enhance mode enabled\n");
	} else {
		sp_write_reg_and(anx78xx, TX_P0, SP_TX_SYS_CTRL4_REG,
				~ENHANCED_MODE);

		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x01,
			DPCD_LANE_COUNT_SET, 1, &c);

		c &= ~ENHANCED_FRAME_EN;
		sp_tx_aux_dpcdwrite_byte(anx78xx, 0x00, 0x01,
			DPCD_LANE_COUNT_SET, c);

		dev_dbg(dev, "Enhance mode disabled\n");
	}
}

static u16 sp_tx_link_err_check(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u16 errl = 0, errh = 0;
	u8 bytebuf[2];

	sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x10, 2, bytebuf);
	usleep_range(5000, 10000);
	sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x10, 2, bytebuf);
	errh = bytebuf[1];

	if (errh & 0x80) {
		errl = bytebuf[0];
		errh = (errh & 0x7f) << 8;
		errl = errh + errl;
	}

	dev_err(dev, " Err of Lane = %d\n", errl);
	return errl;
}

static void sp_lt_finish(struct anx78xx *anx78xx, u8 temp_value)
{
	struct device *dev = &anx78xx->client->dev;

	sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x02, 1, &temp_value);
	if ((temp_value & 0x07) == 0x07) {
		/*
		 * if there is link error,
		 * adjust pre-emphsis to check error again.
		 * If there is no error,keep the setting,
		 * otherwise use 400mv0db
		 */
		if (!sp.tx_test_lt) {
			if (sp_tx_link_err_check(anx78xx)) {
				sp_read_reg(anx78xx, TX_P0,
					SP_TX_LT_SET_REG, &temp_value);
				if (!(temp_value & MAX_PRE_REACH)) {
					sp_write_reg(anx78xx, TX_P0,
						SP_TX_LT_SET_REG,
						(temp_value + 0x08));
					if (sp_tx_link_err_check(anx78xx))
						sp_write_reg(anx78xx, TX_P0,
						SP_TX_LT_SET_REG,
						temp_value);
				}
			}

			sp_read_reg(anx78xx, TX_P0,
				SP_TX_LINK_BW_SET_REG, &temp_value);
			if (temp_value == sp.changed_bandwidth) {
				dev_dbg(dev, "LT succeed, bw: %.2x",
					(u16) temp_value);
				dev_dbg(dev, "Lane0 Set: %.2x\n",
					(u16) sp_i2c_read_byte(anx78xx, TX_P0,
						SP_TX_LT_SET_REG));
				sp.tx_lt_state = LT_INIT;
				goto_next_system_state(anx78xx);
			} else {
				dev_dbg(dev, "cur:%.2x, per:%.2x\n",
					(u16)temp_value,
					(u16)sp.changed_bandwidth);
				sp.tx_lt_state = LT_ERROR;
			}
		} else {
			sp.tx_test_lt = 0;
			sp.tx_lt_state = LT_INIT;
			goto_next_system_state(anx78xx);
		}
	} else {
		dev_dbg(dev, "LANE0 Status error: %.2x\n",
			(u16)(temp_value & 0x07));
		sp.tx_lt_state = LT_ERROR;
	}
}

static void sp_link_training(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 temp_value, return_value, c;

	return_value = 1;
	dev_dbg(dev, "sp.tx_lt_state : %x\n",
			(int)sp.tx_lt_state);
	switch (sp.tx_lt_state) {
	case LT_INIT:
		sp_block_power_ctrl(anx78xx, SP_TX_PWR_VIDEO,
					SP_POWER_ON);
		sp_tx_video_mute(anx78xx, 1);
		sp_tx_enable_video_input(anx78xx, 0);
		sp.tx_lt_state++;
	/* fallthrough */
	case LT_WAIT_PLL_LOCK:
		if (!sp_tx_get_pll_lock_status(anx78xx)) {
			sp_read_reg(anx78xx, TX_P0, SP_TX_PLL_CTRL_REG,
				&temp_value);

			temp_value |= PLL_RST;
			sp_write_reg(anx78xx, TX_P0, SP_TX_PLL_CTRL_REG,
				temp_value);

			temp_value &= ~PLL_RST;
			sp_write_reg(anx78xx, TX_P0, SP_TX_PLL_CTRL_REG,
				temp_value);

			dev_dbg(dev, "PLL not lock!\n");
		} else
			sp.tx_lt_state = LT_CHECK_LINK_BW;
		SP_BREAK(LT_WAIT_PLL_LOCK, sp.tx_lt_state);
	/* fallthrough */
	case LT_CHECK_LINK_BW:
		sp_tx_get_rx_bw(anx78xx, &temp_value);
		if (temp_value < sp.changed_bandwidth) {
			dev_dbg(dev, "****Over bandwidth****\n");
			sp.changed_bandwidth = temp_value;
		} else
			sp.tx_lt_state++;
	/* fallthrough */
	case LT_START:
		if (sp.tx_test_lt) {
			sp.changed_bandwidth = sp.tx_test_bw;
			sp_write_reg_and(anx78xx, TX_P2, SP_TX_VID_CTRL2_REG,
					0x8f);
		} else
			sp_write_reg(anx78xx, TX_P0, SP_TX_LT_SET_REG, 0x00);

		sp_write_reg_and(anx78xx, TX_P0, SP_TX_ANALOG_PD_REG, ~CH0_PD);

		sp_tx_config_ssc(anx78xx, SSC_DEP_4000PPM);
		sp_tx_set_link_bw(anx78xx, sp.changed_bandwidth);
		sp_tx_enhancemode_set(anx78xx);

		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x00, 0x00, 0x01, &c);
		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x06, 0x00, 0x01,
					&temp_value);
		if (c >= 0x12)
			temp_value &= 0xf8;
		else
			temp_value &= 0xfc;
		temp_value |= 0x01;
		sp_tx_aux_dpcdwrite_byte(anx78xx, 0x00, 0x06, 0x00, temp_value);


		sp_write_reg(anx78xx, TX_P0, LT_CTRL, SP_TX_LT_EN);
		sp.tx_lt_state = LT_WAITTING_FINISH;
	/* fallthrough */
	case LT_WAITTING_FINISH:
		/* here : waiting interrupt to change training state. */
		break;

	case LT_ERROR:
		sp_write_reg_or(anx78xx, TX_P2, RST_CTRL2, SERDES_FIFO_RST);
		msleep(20);
		sp_write_reg_and(anx78xx, TX_P2, RST_CTRL2, (~SERDES_FIFO_RST));
		dev_err(dev, "LT ERROR Status: SERDES FIFO reset.");
		redo_cur_system_state(anx78xx);
		sp.tx_lt_state = LT_INIT;
		break;

	case LT_FINISH:
		sp_lt_finish(anx78xx, temp_value);
		break;
	default:
		break;
	}

}
/******************End Link training process********************/

/******************Start Output video process********************/
static void sp_tx_set_colorspace(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 color_space;

	if (sp.down_sample_en) {
		sp_read_reg(anx78xx, RX_P1, HDMI_RX_AVI_DATA00_REG,
			&color_space);
		color_space &= 0x60;
		if (color_space == 0x20) {
			dev_dbg(dev, "YCbCr4:2:2 ---> PASS THROUGH.\n");
			sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL6_REG, 0x00);
			sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL5_REG, 0x00);
			sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL2_REG, 0x11);
		} else if (color_space == 0x40) {
			dev_dbg(dev, "YCbCr4:4:4 ---> YCbCr4:2:2\n");
			sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL6_REG, 0x41);
			sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL5_REG, 0x00);
			sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL2_REG, 0x12);
		} else if (color_space == 0x00) {
			dev_dbg(dev, "RGB4:4:4 ---> YCbCr4:2:2\n");
			sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL6_REG, 0x41);
			sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL5_REG, 0x83);
			sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL2_REG, 0x10);
		}
	} else {
		sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL6_REG, 0x00);
		sp_write_reg(anx78xx, TX_P2, SP_TX_VID_CTRL5_REG, 0x00);
	}
}

static void sp_tx_avi_setup(struct anx78xx *anx78xx)
{
	u8 c;
	int i;

	for (i = 0; i < 13; i++) {
		sp_read_reg(anx78xx, RX_P1, (HDMI_RX_AVI_DATA00_REG + i), &c);
		sp.tx_packet_avi.avi_data[i] = c;
	}
}

static void sp_tx_load_packet(struct anx78xx *anx78xx,
			enum packets_type type)
{
	int i;
	u8 c;

	switch (type) {
	case AVI_PACKETS:
		sp_write_reg(anx78xx, TX_P2, SP_TX_AVI_TYPE, 0x82);
		sp_write_reg(anx78xx, TX_P2, SP_TX_AVI_VER, 0x02);
		sp_write_reg(anx78xx, TX_P2, SP_TX_AVI_LEN, 0x0d);

		for (i = 0; i < 13; i++) {
			sp_write_reg(anx78xx, TX_P2, SP_TX_AVI_DB0 + i,
					sp.tx_packet_avi.avi_data[i]);
		}

		break;

	case SPD_PACKETS:
		sp_write_reg(anx78xx, TX_P2, SP_TX_SPD_TYPE, 0x83);
		sp_write_reg(anx78xx, TX_P2, SP_TX_SPD_VER, 0x01);
		sp_write_reg(anx78xx, TX_P2, SP_TX_SPD_LEN, 0x19);

		for (i = 0; i < 25; i++) {
			sp_write_reg(anx78xx, TX_P2, SP_TX_SPD_DB0 + i,
					sp.tx_packet_spd.spd_data[i]);
		}

		break;

	case VSI_PACKETS:
		sp_write_reg(anx78xx, TX_P2, SP_TX_MPEG_TYPE, 0x81);
		sp_write_reg(anx78xx, TX_P2, SP_TX_MPEG_VER, 0x01);
		sp_read_reg(anx78xx, RX_P1, HDMI_RX_MPEG_LEN_REG, &c);
		sp_write_reg(anx78xx, TX_P2, SP_TX_MPEG_LEN, c);

		for (i = 0; i < 10; i++) {
			sp_write_reg(anx78xx, TX_P2, SP_TX_MPEG_DB0 + i,
					sp.tx_packet_mpeg.mpeg_data[i]);
		}

		break;
	case MPEG_PACKETS:
		sp_write_reg(anx78xx, TX_P2, SP_TX_MPEG_TYPE, 0x85);
		sp_write_reg(anx78xx, TX_P2, SP_TX_MPEG_VER, 0x01);
		sp_write_reg(anx78xx, TX_P2, SP_TX_MPEG_LEN, 0x0d);

		for (i = 0; i < 10; i++) {
			sp_write_reg(anx78xx, TX_P2, SP_TX_MPEG_DB0 + i,
					sp.tx_packet_mpeg.mpeg_data[i]);
		}

		break;
	case AUDIF_PACKETS:
		sp_write_reg(anx78xx, TX_P2, SP_TX_AUD_TYPE, 0x84);
		sp_write_reg(anx78xx, TX_P2, SP_TX_AUD_VER, 0x01);
		sp_write_reg(anx78xx, TX_P2, SP_TX_AUD_LEN, 0x0a);
		for (i = 0; i < 10; i++) {
			sp_write_reg(anx78xx, TX_P2, SP_TX_AUD_DB0 + i,
					sp.tx_audioinfoframe.pb_byte[i]);
		}

		break;

	default:
		break;
	}
}

static void sp_tx_config_packets(struct anx78xx *anx78xx,
				enum packets_type type)
{
	u8 c;

	switch (type) {
	case AVI_PACKETS:

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~AVI_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);
		sp_tx_load_packet(anx78xx, AVI_PACKETS);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AVI_IF_UD;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AVI_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		break;

	case SPD_PACKETS:
		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~SPD_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);
		sp_tx_load_packet(anx78xx, SPD_PACKETS);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= SPD_IF_UD;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |=  SPD_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		break;

	case VSI_PACKETS:
		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~MPEG_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		sp_tx_load_packet(anx78xx, VSI_PACKETS);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_UD;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		break;
	case MPEG_PACKETS:
		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~MPEG_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);


		sp_tx_load_packet(anx78xx, MPEG_PACKETS);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_UD;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= MPEG_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		break;
	case AUDIF_PACKETS:
		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c &= ~AUD_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);


		sp_tx_load_packet(anx78xx, AUDIF_PACKETS);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AUD_IF_UP;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		sp_read_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, &c);
		c |= AUD_IF_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_PKT_EN_REG, c);

		break;

	default:
		break;
	}

}

static void sp_config_video_output(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 temp_value;

	switch (sp.tx_vo_state) {
	default:
	case VO_WAIT_VIDEO_STABLE:
		sp_read_reg(anx78xx, RX_P0, HDMI_RX_SYS_STATUS_REG,
			&temp_value);
		if ((temp_value & (TMDS_DE_DET | TMDS_CLOCK_DET)) == 0x03) {
			sp_tx_bw_lc_sel(anx78xx, sp_tx_pclk_calc(anx78xx));
			sp_tx_enable_video_input(anx78xx, 0);
			sp_tx_avi_setup(anx78xx);
			sp_tx_config_packets(anx78xx, AVI_PACKETS);
			sp_tx_set_colorspace(anx78xx);
			sp_tx_lvttl_bit_mapping(anx78xx);
			if (sp_i2c_read_byte(anx78xx, RX_P0, RX_PACKET_REV_STA)
			& VSI_RCVD)
				sp_hdmi_rx_new_vsi_int(anx78xx);
			sp_tx_enable_video_input(anx78xx, 1);
			sp.tx_vo_state = VO_WAIT_TX_VIDEO_STABLE;
		} else
			dev_dbg(dev, "HDMI input video not stable!\n");
		SP_BREAK(VO_WAIT_VIDEO_STABLE, sp.tx_vo_state);
	/* fallthrough */
	case VO_WAIT_TX_VIDEO_STABLE:
		sp_read_reg(anx78xx, TX_P0, SP_TX_SYS_CTRL2_REG, &temp_value);
		sp_write_reg(anx78xx, TX_P0, SP_TX_SYS_CTRL2_REG, temp_value);
		sp_read_reg(anx78xx, TX_P0, SP_TX_SYS_CTRL2_REG, &temp_value);
		if (temp_value & CHA_STA)
			dev_dbg(dev, "Stream clock not stable!\n");
		else {
			sp_read_reg(anx78xx, TX_P0, SP_TX_SYS_CTRL3_REG,
				&temp_value);
			sp_write_reg(anx78xx, TX_P0, SP_TX_SYS_CTRL3_REG,
				temp_value);
			sp_read_reg(anx78xx, TX_P0, SP_TX_SYS_CTRL3_REG,
				&temp_value);
			if (!(temp_value & STRM_VALID))
				dev_err(dev, "video stream not valid!\n");
			else
				sp.tx_vo_state = VO_CHECK_VIDEO_INFO;
		}
		SP_BREAK(VO_WAIT_TX_VIDEO_STABLE, sp.tx_vo_state);
	/* fallthrough */
	case VO_CHECK_VIDEO_INFO:
		if (!sp_tx_bw_lc_sel(anx78xx, sp_tx_pclk_calc(anx78xx)))
			sp.tx_vo_state++;
		else
			sp_tx_set_sys_state(anx78xx, STATE_LINK_TRAINING);
		SP_BREAK(VO_CHECK_VIDEO_INFO, sp.tx_vo_state);
	/* fallthrough */
	case VO_FINISH:
		sp_block_power_ctrl(anx78xx, SP_TX_PWR_AUDIO,
				SP_POWER_DOWN);
		hdmi_rx_mute_video(anx78xx, 0);
		sp_tx_video_mute(anx78xx, 0);
		sp_tx_show_information(anx78xx);
		goto_next_system_state(anx78xx);
		break;
	}
}
/******************End Output video process********************/

/******************Start HDCP process********************/
static inline void sp_tx_hdcp_encryption_disable(struct anx78xx *anx78xx)
{
	sp_write_reg_and(anx78xx, TX_P0, TX_HDCP_CTRL0, ~ENC_EN);
}

static inline void sp_tx_hdcp_encryption_enable(struct anx78xx *anx78xx)
{
	sp_write_reg_or(anx78xx, TX_P0, TX_HDCP_CTRL0, ENC_EN);
}

static void sp_tx_hw_hdcp_enable(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c;

	sp_write_reg_and(anx78xx, TX_P0, TX_HDCP_CTRL0,
			~ENC_EN & ~HARD_AUTH_EN);
	sp_write_reg_or(anx78xx, TX_P0, TX_HDCP_CTRL0,
			HARD_AUTH_EN | BKSV_SRM_PASS | KSVLIST_VLD | ENC_EN);

	sp_read_reg(anx78xx, TX_P0, TX_HDCP_CTRL0, &c);
	dev_dbg(dev, "TX_HDCP_CTRL0 = %.2x\n", (u16)c);
	sp_write_reg(anx78xx, TX_P0, SP_TX_WAIT_R0_TIME, 0xb0);
	sp_write_reg(anx78xx, TX_P0, SP_TX_WAIT_KSVR_TIME, 0xc8);

	dev_dbg(dev, "Hardware HDCP is enabled.\n");
}

static void sp_hdcp_process(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	switch (sp.hcdp_state) {
	case HDCP_CAPABLE_CHECK:
		sp.ds_vid_stb_cntr = 0;
		sp.hdcp_fail_count = 0;
		if (is_anx_dongle(anx78xx))
			sp.hcdp_state = HDCP_WAITTING_VID_STB;
		else
			sp.hcdp_state = HDCP_HW_ENABLE;
		if (sp.block_en == 0) {
			if (sp_hdcp_cap_check(anx78xx) == 0)
				sp.hcdp_state = HDCP_NOT_SUPPORT;
		}
		/*
		 * Just for debug, pin: P2-2
		 * There is a switch to disable/enable HDCP.
		 */
		sp.hcdp_state = HDCP_NOT_SUPPORT;
		/*****************************************/
		SP_BREAK(HDCP_CAPABLE_CHECK, sp.hcdp_state);
	/* fallthrough */
	case HDCP_WAITTING_VID_STB:
		msleep(100);
		sp.hcdp_state = HDCP_HW_ENABLE;
		SP_BREAK(HDCP_WAITTING_VID_STB, sp.hcdp_state);
	/* fallthrough */
	case HDCP_HW_ENABLE:
		sp_tx_video_mute(anx78xx, 1);
		sp_tx_clean_hdcp_status(anx78xx);
		sp_block_power_ctrl(anx78xx, SP_TX_PWR_HDCP,
				SP_POWER_DOWN);
		msleep(20);
		sp_block_power_ctrl(anx78xx, SP_TX_PWR_HDCP, SP_POWER_ON);
		sp_write_reg(anx78xx, TX_P2, SP_COMMON_INT_MASK2, 0x01);
		msleep(50);
		sp_tx_hw_hdcp_enable(anx78xx);
		sp.hcdp_state = HDCP_WAITTING_FINISH;
	/* fallthrough */
	case HDCP_WAITTING_FINISH:
		break;
	case HDCP_FINISH:
		sp_tx_hdcp_encryption_enable(anx78xx);
		 hdmi_rx_mute_video(anx78xx, 0);
		sp_tx_video_mute(anx78xx, 0);
		goto_next_system_state(anx78xx);
		sp.hcdp_state = HDCP_CAPABLE_CHECK;
		dev_dbg(dev, "@@@@@@@hdcp_auth_pass@@@@@@\n");
		break;
	case HDCP_FAILE:
		if (sp.hdcp_fail_count > 5) {
			sp_vbus_power_off(anx78xx);
			reg_hardware_reset(anx78xx);
			sp.hcdp_state = HDCP_CAPABLE_CHECK;
			sp.hdcp_fail_count = 0;
			dev_dbg(dev, "*********hdcp_auth_failed*********\n");
		} else {
			sp.hdcp_fail_count++;
			sp.hcdp_state = HDCP_WAITTING_VID_STB;
		}
		break;
	default:
	case HDCP_NOT_SUPPORT:
		dev_dbg(dev, "Sink is not capable HDCP\n");
		sp_block_power_ctrl(anx78xx, SP_TX_PWR_HDCP,
			SP_POWER_DOWN);
		sp_tx_video_mute(anx78xx, 0);
		goto_next_system_state(anx78xx);
		sp.hcdp_state = HDCP_CAPABLE_CHECK;
		break;
	}
}
/******************End HDCP process********************/

/******************Start Audio process********************/
static void sp_tx_audioinfoframe_setup(struct anx78xx *anx78xx)
{
	int i;
	u8 c;

	sp_read_reg(anx78xx, RX_P1, HDMI_RX_AUDIO_TYPE_REG, &c);
	sp.tx_audioinfoframe.type = c;
	sp_read_reg(anx78xx, RX_P1, HDMI_RX_AUDIO_VER_REG, &c);
	sp.tx_audioinfoframe.version = c;
	sp_read_reg(anx78xx, RX_P1, HDMI_RX_AUDIO_LEN_REG, &c);
	sp.tx_audioinfoframe.length = c;

	for (i = 0; i < 11; i++) {
		sp_read_reg(anx78xx, RX_P1, (HDMI_RX_AUDIO_DATA00_REG + i), &c);
		sp.tx_audioinfoframe.pb_byte[i] = c;
	}
}

static void sp_tx_enable_audio_output(struct anx78xx *anx78xx, u8 benable)
{
	u8 c;

	sp_read_reg(anx78xx, TX_P0, SP_TX_AUD_CTRL, &c);
	if (benable) {
		if (c & AUD_EN) {
			c &= ~AUD_EN;
			sp_write_reg(anx78xx, TX_P0, SP_TX_AUD_CTRL, c);
		}
		sp_tx_audioinfoframe_setup(anx78xx);
		sp_tx_config_packets(anx78xx, AUDIF_PACKETS);

		c |= AUD_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_AUD_CTRL, c);

	} else {
		c &= ~AUD_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_AUD_CTRL, c);
		sp_write_reg_and(anx78xx, TX_P0, SP_TX_PKT_EN_REG, ~AUD_IF_EN);
	}
}

static void sp_tx_config_audio(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c;
	int i;
	ulong M_AUD, LS_Clk = 0;
	ulong AUD_Freq = 0;

	dev_dbg(dev, "**Config audio **\n");
	sp_block_power_ctrl(anx78xx, SP_TX_PWR_AUDIO, SP_POWER_ON);
	sp_read_reg(anx78xx, RX_P0, 0xCA, &c);

	switch (c & 0x0f) {
	case 0x00:
		AUD_Freq = 44.1;
		break;
	case 0x02:
		AUD_Freq = 48;
		break;
	case 0x03:
		AUD_Freq = 32;
		break;
	case 0x08:
		AUD_Freq = 88.2;
		break;
	case 0x0a:
		AUD_Freq = 96;
		break;
	case 0x0c:
		AUD_Freq = 176.4;
		break;
	case 0x0e:
		AUD_Freq = 192;
		break;
	default:
		break;
	}


	switch (sp_tx_get_link_bw(anx78xx)) {
	case LINK_1P62G:
		LS_Clk = 162000;
		break;
	case LINK_2P7G:
		LS_Clk = 270000;
		break;
	case LINK_5P4G:
		LS_Clk = 540000;
		break;
	case LINK_6P75G:
		LS_Clk = 675000;
		break;
	default:
		break;
	}

	dev_dbg(dev, "AUD_Freq = %ld , LS_CLK = %ld\n", AUD_Freq, LS_Clk);

	M_AUD = ((512 * AUD_Freq) / LS_Clk) * 32768;
	M_AUD = M_AUD + 0x05;
	sp_write_reg(anx78xx, TX_P1, SP_TX_AUD_INTERFACE_CTRL4, (M_AUD & 0xff));
	M_AUD = M_AUD >> 8;
	sp_write_reg(anx78xx, TX_P1, SP_TX_AUD_INTERFACE_CTRL5, (M_AUD & 0xff));
	sp_write_reg(anx78xx, TX_P1, SP_TX_AUD_INTERFACE_CTRL6, 0x00);

	sp_write_reg_and(anx78xx, TX_P1, SP_TX_AUD_INTERFACE_CTRL0,
			(u8)~AUD_INTERFACE_DISABLE);

	sp_write_reg_or(anx78xx, TX_P1, SP_TX_AUD_INTERFACE_CTRL2,
			M_AUD_ADJUST_ST);

	sp_read_reg(anx78xx, RX_P0, HDMI_STATUS, &c);
	if (c & HDMI_AUD_LAYOUT)
		sp_write_reg_or(anx78xx, TX_P2, SP_TX_AUD_CH_NUM_REG5,
				CH_NUM_8 | AUD_LAYOUT);
	else
		sp_write_reg_and(anx78xx, TX_P2, SP_TX_AUD_CH_NUM_REG5,
				(u8)(~CH_NUM_8) & (~AUD_LAYOUT));

	/* transfer audio chaneel status from HDMI Rx to Slinmport Tx */
	for (i = 0; i < 5; i++) {
		sp_read_reg(anx78xx, RX_P0, (HDMI_RX_AUD_IN_CH_STATUS1_REG + i),
			 &c);
		sp_write_reg(anx78xx, TX_P2, (SP_TX_AUD_CH_STATUS_REG1 + i), c);
	}

	/* enable audio */
	sp_tx_enable_audio_output(anx78xx, 1);
}

static void sp_config_audio_output(struct anx78xx *anx78xx)
{
	static u8 count;

	switch (sp.tx_ao_state) {
	default:
	case AO_INIT:
	case AO_CTS_RCV_INT:
	case AO_AUDIO_RCV_INT:
		if (!(sp_i2c_read_byte(anx78xx, RX_P0, HDMI_STATUS)
		    & HDMI_MODE)) {
			sp.tx_ao_state = AO_INIT;
			goto_next_system_state(anx78xx);
		}
		break;
	case AO_RCV_INT_FINISH:
		if (count++ > 2)
			sp.tx_ao_state = AO_OUTPUT;
		else
			sp.tx_ao_state = AO_INIT;
		SP_BREAK(AO_INIT, sp.tx_ao_state);
	/* fallthrough */
	case AO_OUTPUT:
		count = 0;
		sp.tx_ao_state = AO_INIT;
		 hdmi_rx_mute_audio(anx78xx, 0);
		sp_tx_config_audio(anx78xx);
		goto_next_system_state(anx78xx);
		break;
	}
}
/******************End Audio process********************/

void sp_initialization(struct anx78xx *anx78xx)
{
	/* Waitting Hot plug event! */
	if (!(sp.common_int_status.common_int[3] & PLUG))
		return;

	sp.read_edid_flag = 0;

	/* Power on all modules */
	sp_write_reg(anx78xx, TX_P2, SP_POWERD_CTRL_REG, 0x00);
	/* Driver Version */
	sp_write_reg(anx78xx, TX_P1, FW_VER_REG, FW_VERSION);
	hdmi_rx_initialization(anx78xx);
	sp_tx_initialization(anx78xx);
	msleep(200);
	goto_next_system_state(anx78xx);
}

static void sp_hdcp_external_ctrl_flag_monitor(struct anx78xx *anx78xx)
{
	static u8 cur_flag;

	if (sp.block_en != cur_flag) {
		cur_flag = sp.block_en;
		system_state_change_with_case(anx78xx, STATE_HDCP_AUTH);
	}
}

static void sp_state_process(struct anx78xx *anx78xx)
{
	switch (sp.tx_system_state) {
	case STATE_WAITTING_CABLE_PLUG:
		sp_waiting_cable_plug_process(anx78xx);
		SP_BREAK(STATE_WAITTING_CABLE_PLUG, sp.tx_system_state);
	/* fallthrough */
	case STATE_SP_INITIALIZED:
		sp_initialization(anx78xx);
		SP_BREAK(STATE_SP_INITIALIZED, sp.tx_system_state);
	/* fallthrough */
	case STATE_SINK_CONNECTION:
		sp_sink_connection(anx78xx);
		SP_BREAK(STATE_SINK_CONNECTION, sp.tx_system_state);
	/* fallthrough */
	case STATE_PARSE_EDID:
		sp_edid_process(anx78xx);
		SP_BREAK(STATE_PARSE_EDID, sp.tx_system_state);
	/* fallthrough */
	case STATE_LINK_TRAINING:
		sp_link_training(anx78xx);
		SP_BREAK(STATE_LINK_TRAINING, sp.tx_system_state);
	/* fallthrough */
	case STATE_VIDEO_OUTPUT:
		sp_config_video_output(anx78xx);
		SP_BREAK(STATE_VIDEO_OUTPUT, sp.tx_system_state);
	/* fallthrough */
	case STATE_HDCP_AUTH:
		sp_hdcp_process(anx78xx);
		SP_BREAK(STATE_HDCP_AUTH, sp.tx_system_state);
	/* fallthrough */
	case STATE_AUDIO_OUTPUT:
		sp_config_audio_output(anx78xx);
		SP_BREAK(STATE_AUDIO_OUTPUT, sp.tx_system_state);
	/* fallthrough */
	case STATE_PLAY_BACK:
		SP_BREAK(STATE_PLAY_BACK, sp.tx_system_state);
	/* fallthrough */
	default:
		break;
	}
}

/******************Start INT process********************/
static void sp_tx_int_rec(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 *p = sp.common_int_status.common_int;

	sp_read_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1, &p[0]);
	sp_write_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1, p[0]);

	sp_read_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1 + 1, &p[1]);
	sp_write_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1 + 1, p[1]);

	sp_read_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1 + 2, &p[2]);
	sp_write_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1 + 2, p[2]);

	sp_read_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1 + 3, &p[3]);
	sp_write_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1 + 3, p[3]);

	sp_read_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1 + 6, &p[4]);
	sp_write_reg(anx78xx, TX_P2, SP_COMMON_INT_STATUS1 + 6, p[4]);
	if (p[0] | p[1] | p[2] | p[3] | p[4])
		dev_dbg(dev, "%s: status1: %02x %02x %02x %02x %02x\n", __func__,
			p[0], p[1], p[2], p[3], p[4]);
	if (anx78xx->first_time) {
		anx78xx->first_time = 0;
		p[3] |= PLUG;
	}
}

static void sp_hdmi_rx_int_rec(struct anx78xx *anx78xx)
{
	 sp_read_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS1_REG,
		&sp.hdmi_rx_int_status.hdmi_rx_int[0]);
	 sp_write_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS1_REG,
		sp.hdmi_rx_int_status.hdmi_rx_int[0]);
	 sp_read_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS2_REG,
		&sp.hdmi_rx_int_status.hdmi_rx_int[1]);
	 sp_write_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS2_REG,
		sp.hdmi_rx_int_status.hdmi_rx_int[1]);
	 sp_read_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS3_REG,
		&sp.hdmi_rx_int_status.hdmi_rx_int[2]);
	 sp_write_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS3_REG,
		sp.hdmi_rx_int_status.hdmi_rx_int[2]);
	 sp_read_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS4_REG,
		&sp.hdmi_rx_int_status.hdmi_rx_int[3]);
	 sp_write_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS4_REG,
		sp.hdmi_rx_int_status.hdmi_rx_int[3]);
	 sp_read_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS5_REG,
		&sp.hdmi_rx_int_status.hdmi_rx_int[4]);
	 sp_write_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS5_REG,
		sp.hdmi_rx_int_status.hdmi_rx_int[4]);
	 sp_read_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS6_REG,
		&sp.hdmi_rx_int_status.hdmi_rx_int[5]);
	 sp_write_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS6_REG,
		sp.hdmi_rx_int_status.hdmi_rx_int[5]);
	 sp_read_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS7_REG,
		&sp.hdmi_rx_int_status.hdmi_rx_int[6]);
	 sp_write_reg(anx78xx, RX_P0, HDMI_RX_INT_STATUS7_REG,
		sp.hdmi_rx_int_status.hdmi_rx_int[6]);
}

static void sp_int_rec(struct anx78xx *anx78xx)
{
	sp_tx_int_rec(anx78xx);
	sp_hdmi_rx_int_rec(anx78xx);
}
/******************End INT process********************/

/******************Start task process********************/
static void sp_tx_pll_changed_int_handler(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	if (sp.tx_system_state >= STATE_LINK_TRAINING) {
		if (!sp_tx_get_pll_lock_status(anx78xx)) {
			dev_dbg(dev, "PLL:PLL not lock!\n");
			sp_tx_set_sys_state(anx78xx, STATE_LINK_TRAINING);
		}
	}
}

static void sp_tx_hdcp_link_chk_fail_handler(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	system_state_change_with_case(anx78xx, STATE_HDCP_AUTH);

	dev_dbg(dev, "hdcp_link_chk_fail:HDCP Sync lost!\n");
}

static void sp_tx_phy_auto_test(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 b_sw;
	u8 c1;
	u8 bytebuf[16];
	u8 link_bw;

	/* DPCD 0x219 TEST_LINK_RATE */
	sp_tx_aux_dpcdread_bytes(anx78xx, 0x0, 0x02, 0x19, 1, bytebuf);
	dev_dbg(dev, "DPCD:0x00219 = %.2x\n", (u16)bytebuf[0]);
	switch (bytebuf[0]) {
	case 0x06:
	case 0x0A:
	case 0x14:
	case 0x19:
		sp_tx_set_link_bw(anx78xx, bytebuf[0]);
		sp.tx_test_bw = bytebuf[0];
		break;
	default:
		sp_tx_set_link_bw(anx78xx, 0x19);
		sp.tx_test_bw = 0x19;
		break;
	}


	/* DPCD 0x248 PHY_TEST_PATTERN */
	sp_tx_aux_dpcdread_bytes(anx78xx, 0x0, 0x02, 0x48, 1, bytebuf);
	dev_dbg(dev, "DPCD:0x00248 = %.2x\n", (u16)bytebuf[0]);
	switch (bytebuf[0]) {
	case 0:
		break;
	case 1:
		sp_write_reg(anx78xx, TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x04);
		break;
	case 2:
		sp_write_reg(anx78xx, TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x08);
		break;
	case 3:
		sp_write_reg(anx78xx, TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x0c);
		break;
	case 4:
		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x50, 0xa,
			bytebuf);
		sp_write_reg(anx78xx, TX_P1, 0x80, bytebuf[0]);
		sp_write_reg(anx78xx, TX_P1, 0x81, bytebuf[1]);
		sp_write_reg(anx78xx, TX_P1, 0x82, bytebuf[2]);
		sp_write_reg(anx78xx, TX_P1, 0x83, bytebuf[3]);
		sp_write_reg(anx78xx, TX_P1, 0x84, bytebuf[4]);
		sp_write_reg(anx78xx, TX_P1, 0x85, bytebuf[5]);
		sp_write_reg(anx78xx, TX_P1, 0x86, bytebuf[6]);
		sp_write_reg(anx78xx, TX_P1, 0x87, bytebuf[7]);
		sp_write_reg(anx78xx, TX_P1, 0x88, bytebuf[8]);
		sp_write_reg(anx78xx, TX_P1, 0x89, bytebuf[9]);
		sp_write_reg(anx78xx, TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x30);
		break;
	case 5:
		sp_write_reg(anx78xx, TX_P0, 0xA9, 0x00);
		sp_write_reg(anx78xx, TX_P0, 0xAA, 0x01);
		sp_write_reg(anx78xx, TX_P0, SP_TX_TRAINING_PTN_SET_REG, 0x14);
		break;
	}

	sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x00, 0x03, 1, bytebuf);
	dev_dbg(dev, "DPCD:0x00003 = %.2x\n", (u16)bytebuf[0]);
	switch (bytebuf[0] & 0x01) {
	case 0:
		sp_tx_spread_enable(anx78xx, 0);
		break;
	case 1:
		sp_read_reg(anx78xx, TX_P0, SP_TX_LINK_BW_SET_REG, &c1);
		switch (c1) {
		case 0x06:
			link_bw = 0x06;
			break;
		case 0x0a:
			link_bw = 0x0a;
			break;
		case 0x14:
			link_bw = 0x14;
			break;
		case 0x19:
			link_bw = 0x19;
			break;
		default:
			link_bw = 0x00;
			break;
		}
		sp_tx_config_ssc(anx78xx, SSC_DEP_4000PPM);
		break;
	}

	/* get swing and emphasis adjust request */
	sp_read_reg(anx78xx, TX_P0, 0xA3, &b_sw);

	sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x06, 1, bytebuf);
	dev_dbg(dev, "DPCD:0x00206 = %.2x\n", (u16)bytebuf[0]);
	c1 = bytebuf[0] & 0x0f;
	switch (c1) {
	case 0x00:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x00);
		break;
	case 0x01:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x01);
		break;
	case 0x02:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x02);
		break;
	case 0x03:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x03);
		break;
	case 0x04:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x08);
		break;
	case 0x05:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x09);
		break;
	case 0x06:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x0a);
		break;
	case 0x08:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x10);
		break;
	case 0x09:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x11);
		break;
	case 0x0c:
		sp_write_reg(anx78xx, TX_P0, 0xA3, (b_sw & ~0x1b) | 0x18);
		break;
	default:
		break;
	}
}

static void sp_hpd_irq_process(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c, c1;
	u8 test_vector;
	u8 data_buf[6];

	sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x00, 6, data_buf);
	dev_dbg(dev, "+++++++++++++Get HPD IRQ %x\n", (int)data_buf[1]);

	if (data_buf[1] != 0)
		sp_tx_aux_dpcdwrite_bytes(anx78xx, 0x00, 0x02,
			DPCD_SERVICE_IRQ_VECTOR, 1, &(data_buf[1]));

	/* HDCP IRQ */
	if (data_buf[1] & CP_IRQ) {
		if (sp.hcdp_state > HDCP_WAITTING_FINISH
			|| sp.tx_system_state > STATE_HDCP_AUTH) {
			sp_tx_aux_dpcdread_bytes(anx78xx, 0x06, 0x80, 0x29, 1,
						&c1);
			if (c1 & 0x04) {
				system_state_change_with_case(anx78xx,
						STATE_HDCP_AUTH);
				dev_dbg(dev, "IRQ:_______HDCP Sync lost!\n");
			}
		}
	}

	/* AUTOMATED TEST IRQ */
	if (data_buf[1] & TEST_IRQ) {
		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x18, 1,
				&test_vector);

		if (test_vector & 0x01) {
			sp.tx_test_lt = 1;

			sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x19, 1,
						&c);
			switch (c) {
			case 0x06:
			case 0x0A:
			case 0x14:
			case 0x19:
				sp_tx_set_link_bw(anx78xx, c);
				sp.tx_test_bw = c;
				break;
			default:
				sp_tx_set_link_bw(anx78xx, 0x19);
				sp.tx_test_bw = 0x19;
				break;
			}

			dev_dbg(dev, " test_bw = %.2x\n", (u16)sp.tx_test_bw);

			sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x60, 1,
						&c);
			c = c | TEST_ACK;
			sp_tx_aux_dpcdwrite_bytes(anx78xx, 0x00, 0x02, 0x60, 1,
						&c);

			dev_dbg(dev, "Set TEST_ACK!\n");
			if (sp.tx_system_state >= STATE_LINK_TRAINING) {
				sp.tx_lt_state = LT_INIT;
				sp_tx_set_sys_state(anx78xx,
						STATE_LINK_TRAINING);
			}
			dev_dbg(dev, "IRQ:test-LT request!\n");
		}

		if (test_vector & 0x02) {
			sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x60, 1,
						&c);
			c = c | TEST_ACK;
			sp_tx_aux_dpcdwrite_bytes(anx78xx, 0x00, 0x02, 0x60, 1,
						&c);
		}
		if (test_vector & 0x04) {
			if (sp.tx_system_state > STATE_PARSE_EDID)
				sp_tx_set_sys_state(anx78xx, STATE_PARSE_EDID);
			sp.tx_test_edid = 1;
			dev_dbg(dev, "Test EDID Requested!\n");
		}

		if (test_vector & 0x08) {
			sp.tx_test_lt = 1;

			sp_tx_phy_auto_test(anx78xx);

			sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x02, 0x60, 1,
						&c);
			c = c | 0x01;
			sp_tx_aux_dpcdwrite_bytes(anx78xx, 0x00, 0x02, 0x60, 1,
						&c);
		}
	}

	if (sp.tx_system_state > STATE_LINK_TRAINING) {
		if (!(data_buf[4] & 0x01)
		|| ((data_buf[2] & (0x01 | 0x04)) != 0x05)) {
			sp_tx_set_sys_state(anx78xx, STATE_LINK_TRAINING);
			dev_dbg(dev, "INT:re-LT request!\n");
			return;
		}

		dev_dbg(dev, "Lane align %x\n", (u16)data_buf[4]);
		dev_dbg(dev, "Lane clock recovery %x\n", (u16)data_buf[2]);
	}
}

static void sp_tx_vsi_setup(struct anx78xx *anx78xx)
{
	u8 c;
	int i;

	for (i = 0; i < 10; i++) {
		sp_read_reg(anx78xx, RX_P1, (HDMI_RX_MPEG_DATA00_REG + i), &c);
		sp.tx_packet_mpeg.mpeg_data[i] = c;
	}
}

static void sp_tx_mpeg_setup(struct anx78xx *anx78xx)
{
	u8 c;
	int i;

	for (i = 0; i < 10; i++) {
		sp_read_reg(anx78xx, RX_P1, (HDMI_RX_MPEG_DATA00_REG + i), &c);
		sp.tx_packet_mpeg.mpeg_data[i] = c;
	}
}

static void sp_tx_auth_done_int_handler(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 bytebuf[2];

	if (sp.hcdp_state > HDCP_HW_ENABLE
		&& sp.tx_system_state == STATE_HDCP_AUTH) {
		sp_read_reg(anx78xx, TX_P0, SP_TX_HDCP_STATUS, bytebuf);
		if (bytebuf[0] & SP_TX_HDCP_AUTH_PASS) {
			sp_tx_aux_dpcdread_bytes(anx78xx, 0x06, 0x80, 0x2A, 2,
						bytebuf);
			if ((bytebuf[1] & 0x08) || (bytebuf[0] & 0x80)) {
				dev_dbg(dev, "max cascade/devs exceeded!\n");
				sp_tx_hdcp_encryption_disable(anx78xx);
			} else
				dev_dbg(dev, "%s\n",
					"Authentication pass in Auth_Done");

			sp.hcdp_state = HDCP_FINISH;
		} else {
			dev_err(dev, "Authentication failed in AUTH_done\n");
			sp_tx_video_mute(anx78xx, 1);
			sp_tx_clean_hdcp_status(anx78xx);
			sp.hcdp_state = HDCP_FAILE;
		}
	}

	dev_dbg(dev, "sp_tx_auth_done_int_handler\n");
}

static void sp_tx_lt_done_int_handler(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c;

	if (sp.tx_lt_state == LT_WAITTING_FINISH
		&& sp.tx_system_state == STATE_LINK_TRAINING) {
		sp_read_reg(anx78xx, TX_P0, LT_CTRL, &c);
		if (c & 0x70) {
			c = (c & 0x70) >> 4;
			dev_dbg(dev, "LT failed in interrupt, ERR = %.2x\n",
				(u16) c);
			sp.tx_lt_state = LT_ERROR;
		} else {
			dev_dbg(dev, "lt_done: LT Finish\n");
			sp.tx_lt_state = LT_FINISH;
		}
	}

}

static void sp_hdmi_rx_clk_det_int(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	dev_dbg(dev, "*HDMI_RX Interrupt: Pixel Clock Change.\n");
	if (sp.tx_system_state > STATE_VIDEO_OUTPUT) {
		sp_tx_video_mute(anx78xx, 1);
		sp_tx_enable_audio_output(anx78xx, 0);
		sp_tx_set_sys_state(anx78xx, STATE_VIDEO_OUTPUT);
	}
}

static void sp_hdmi_rx_sync_det_int(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	dev_dbg(dev, "*HDMI_RX Interrupt: Sync Detect.\n");
}

static void sp_hdmi_rx_hdmi_dvi_int(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c;

	dev_dbg(dev, "sp_hdmi_rx_hdmi_dvi_int.\n");
	sp_read_reg(anx78xx, RX_P0, HDMI_STATUS, &c);
	sp.hdmi_dvi_status = 1;
	if ((c & BIT(0)) != (sp.hdmi_dvi_status & BIT(0))) {
		dev_dbg(dev, "hdmi_dvi_int: Is HDMI MODE: %x.\n",
			(u16)(c & HDMI_MODE));
		sp.hdmi_dvi_status = (c & BIT(0));
		hdmi_rx_mute_audio(anx78xx, 1);
		system_state_change_with_case(anx78xx, STATE_LINK_TRAINING);
	}
}

static void sp_hdmi_rx_new_avi_int(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	dev_dbg(dev, "*HDMI_RX Interrupt: New AVI Packet.\n");
	sp_tx_lvttl_bit_mapping(anx78xx);
	sp_tx_set_colorspace(anx78xx);
	sp_tx_avi_setup(anx78xx);
	sp_tx_config_packets(anx78xx, AVI_PACKETS);
}

static void sp_hdmi_rx_new_vsi_int(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 hdmi_video_format, v3d_structure;

	dev_dbg(dev, "*HDMI_RX Interrupt: NEW VSI packet.\n");

	sp_write_reg_and(anx78xx, TX_P0, SP_TX_3D_VSC_CTRL, ~INFO_FRAME_VSC_EN);
	/* VSI package header */
	if ((sp_i2c_read_byte(anx78xx, RX_P1, HDMI_RX_MPEG_TYPE_REG) != 0x81)
		|| (sp_i2c_read_byte(anx78xx, RX_P1, HDMI_RX_MPEG_VER_REG)
		!= 0x01))
		return;

	dev_dbg(dev, "Setup VSI package!\n");

	sp_tx_vsi_setup(anx78xx);
	sp_tx_config_packets(anx78xx, VSI_PACKETS);

	sp_read_reg(anx78xx, RX_P1, HDMI_RX_MPEG_DATA03_REG,
		&hdmi_video_format);

	if ((hdmi_video_format & 0xe0) == 0x40) {
		dev_dbg(dev, "3D VSI packet detected. Config VSC packet\n");

		sp_read_reg(anx78xx, RX_P1, HDMI_RX_MPEG_DATA05_REG,
			&v3d_structure);

		switch (v3d_structure&0xf0) {
		case 0x00:
			v3d_structure = 0x02;
			break;
		case 0x20:
			v3d_structure = 0x03;
			break;
		case 0x30:
			v3d_structure = 0x04;
			break;
		default:
			v3d_structure = 0x00;
			dev_dbg(dev, "3D structure is not supported\n");
			break;
		}
		sp_write_reg(anx78xx, TX_P0, SP_TX_VSC_DB1, v3d_structure);
	}
	sp_write_reg_or(anx78xx, TX_P0, SP_TX_3D_VSC_CTRL, INFO_FRAME_VSC_EN);
	sp_write_reg_and(anx78xx, TX_P0, SP_TX_PKT_EN_REG, ~SPD_IF_EN);
	sp_write_reg_or(anx78xx, TX_P0, SP_TX_PKT_EN_REG, SPD_IF_UD);
	sp_write_reg_or(anx78xx, TX_P0, SP_TX_PKT_EN_REG, SPD_IF_EN);
}

static void sp_hdmi_rx_no_vsi_int(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c;

	sp_read_reg(anx78xx, TX_P0, SP_TX_3D_VSC_CTRL, &c);
	if (c & INFO_FRAME_VSC_EN) {
		dev_dbg(dev, "No new VSI is received, disable  VSC packet\n");
		c &= ~INFO_FRAME_VSC_EN;
		sp_write_reg(anx78xx, TX_P0, SP_TX_3D_VSC_CTRL, c);
		sp_tx_mpeg_setup(anx78xx);
		sp_tx_config_packets(anx78xx, MPEG_PACKETS);
	}
}

static void sp_hdmi_rx_restart_audio_chk(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	dev_dbg(dev, "WAIT_AUDIO: sp_hdmi_rx_restart_audio_chk.\n");
	system_state_change_with_case(anx78xx, STATE_AUDIO_OUTPUT);
}

static void sp_hdmi_rx_cts_rcv_int(struct anx78xx *anx78xx)
{
	if (sp.tx_ao_state == AO_INIT)
		sp.tx_ao_state = AO_CTS_RCV_INT;
	else if (sp.tx_ao_state == AO_AUDIO_RCV_INT)
		sp.tx_ao_state = AO_RCV_INT_FINISH;
}

static void sp_hdmi_rx_audio_rcv_int(struct anx78xx *anx78xx)
{
	if (sp.tx_ao_state == AO_INIT)
		sp.tx_ao_state = AO_AUDIO_RCV_INT;
	else if (sp.tx_ao_state == AO_CTS_RCV_INT)
		sp.tx_ao_state = AO_RCV_INT_FINISH;
}

static void sp_hdmi_rx_audio_samplechg_int(struct anx78xx *anx78xx)
{
	u16 i;
	u8 c;
	/* transfer audio chaneel status from HDMI Rx to Slinmport Tx */
	for (i = 0; i < 5; i++) {
		sp_read_reg(anx78xx, RX_P0, (HDMI_RX_AUD_IN_CH_STATUS1_REG + i),
			&c);
		sp_write_reg(anx78xx, TX_P2, (SP_TX_AUD_CH_STATUS_REG1 + i), c);
	}
}

static void sp_hdmi_rx_hdcp_error_int(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	static u8 count;

	dev_dbg(dev, "*HDMI_RX Interrupt: hdcp error.\n");
	if (count >= 40) {
		count = 0;
		dev_dbg(dev, "Lots of hdcp error occurred ...\n");
		hdmi_rx_mute_audio(anx78xx, 1);
		hdmi_rx_mute_video(anx78xx, 1);
		hdmi_rx_set_hpd(anx78xx, 0);
		usleep_range(10000, 11000);
		hdmi_rx_set_hpd(anx78xx, 1);
	} else
		count++;
}

static void sp_hdmi_rx_new_gcp_int(struct anx78xx *anx78xx)
{
	u8 c;

	sp_read_reg(anx78xx, RX_P1, HDMI_RX_GENERAL_CTRL, &c);
	if (c & SET_AVMUTE) {
		hdmi_rx_mute_video(anx78xx, 1);
		hdmi_rx_mute_audio(anx78xx, 1);
	} else if (c & CLEAR_AVMUTE) {
		hdmi_rx_mute_video(anx78xx, 0);
		hdmi_rx_mute_audio(anx78xx, 0);
	}
}

static void sp_tx_hpd_int_handler(struct anx78xx *anx78xx, u8 hpd_source)
{
	struct device *dev = &anx78xx->client->dev;

	dev_dbg(dev, "sp_tx_hpd_int_handler\n");

	switch (hpd_source) {
	case HPD_LOST:
		hdmi_rx_set_hpd(anx78xx, 0);
		sp_tx_set_sys_state(anx78xx, STATE_WAITTING_CABLE_PLUG);
		break;
	case HPD_CHANGE:
		dev_dbg(dev, "HPD:____________HPD changed!\n");
		usleep_range(2000, 4000);
		if (sp.common_int_status.common_int[3] & HPD_IRQ)
			sp_hpd_irq_process(anx78xx);

		if (sp_i2c_read_byte(anx78xx, TX_P0,
		   SP_TX_SYS_CTRL3_REG) & HPD_STATUS) {
			if (sp.common_int_status.common_int[3] & HPD_IRQ)
				sp_hpd_irq_process(anx78xx);
		} else {
			if (sp_i2c_read_byte(anx78xx, TX_P0,
			    SP_TX_SYS_CTRL3_REG)
				& HPD_STATUS) {
				hdmi_rx_set_hpd(anx78xx, 0);
				sp_tx_set_sys_state(anx78xx,
					STATE_WAITTING_CABLE_PLUG);
			}
		}
		break;
	case PLUG:
		dev_dbg(dev, "HPD:____________HPD changed!\n");
		if (sp.tx_system_state < STATE_SP_INITIALIZED)
			sp_tx_set_sys_state(anx78xx, STATE_SP_INITIALIZED);
		break;
	default:
		break;
	}
}

static void sp_system_isr_handler(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	if (sp.common_int_status.common_int[3] & HPD_CHANGE)
		sp_tx_hpd_int_handler(anx78xx, HPD_CHANGE);
	if (sp.common_int_status.common_int[3] & HPD_LOST)
		sp_tx_hpd_int_handler(anx78xx, HPD_LOST);
	if (sp.common_int_status.common_int[3] & HPD_IRQ)
		dev_dbg(dev, "++++++++++++++++========HDCP_IRQ interrupt\n");
	if (sp.common_int_status.common_int[0] & PLL_LOCK_CHG)
		sp_tx_pll_changed_int_handler(anx78xx);

	if (sp.common_int_status.common_int[1] & HDCP_AUTH_DONE)
		sp_tx_auth_done_int_handler(anx78xx);

	if (sp.common_int_status.common_int[2] & HDCP_LINK_CHECK_FAIL)
		sp_tx_hdcp_link_chk_fail_handler(anx78xx);

	if (sp.common_int_status.common_int[4] & TRAINING_Finish)
		sp_tx_lt_done_int_handler(anx78xx);

	if (sp.tx_system_state > STATE_SINK_CONNECTION) {
		if (sp.hdmi_rx_int_status.hdmi_rx_int[5] & NEW_AVI)
			sp_hdmi_rx_new_avi_int(anx78xx);
	}

	if (sp.tx_system_state > STATE_VIDEO_OUTPUT) {
		if (sp.hdmi_rx_int_status.hdmi_rx_int[6] & NEW_VS) {
			sp.hdmi_rx_int_status.hdmi_rx_int[6] &= ~NO_VSI;
			sp_hdmi_rx_new_vsi_int(anx78xx);
		}
		if (sp.hdmi_rx_int_status.hdmi_rx_int[6] & NO_VSI)
			sp_hdmi_rx_no_vsi_int(anx78xx);
	}

	if (sp.tx_system_state >= STATE_VIDEO_OUTPUT) {
		if (sp.hdmi_rx_int_status.hdmi_rx_int[0] & CKDT_CHANGE)
			sp_hdmi_rx_clk_det_int(anx78xx);

		if (sp.hdmi_rx_int_status.hdmi_rx_int[0] & SCDT_CHANGE)
			sp_hdmi_rx_sync_det_int(anx78xx);

		if (sp.hdmi_rx_int_status.hdmi_rx_int[0] & HDMI_DVI)
			sp_hdmi_rx_hdmi_dvi_int(anx78xx);

		if ((sp.hdmi_rx_int_status.hdmi_rx_int[5] & NEW_AUD)
		     || (sp.hdmi_rx_int_status.hdmi_rx_int[2] & AUD_MODE_CHANGE
		   ))
			sp_hdmi_rx_restart_audio_chk(anx78xx);

		if (sp.hdmi_rx_int_status.hdmi_rx_int[5] & CTS_RCV)
			sp_hdmi_rx_cts_rcv_int(anx78xx);

		if (sp.hdmi_rx_int_status.hdmi_rx_int[4] & AUDIO_RCV)
			sp_hdmi_rx_audio_rcv_int(anx78xx);


		if (sp.hdmi_rx_int_status.hdmi_rx_int[1] & HDCP_ERR)
			sp_hdmi_rx_hdcp_error_int(anx78xx);

		if (sp.hdmi_rx_int_status.hdmi_rx_int[5] & NEW_CP)
			sp_hdmi_rx_new_gcp_int(anx78xx);

		if (sp.hdmi_rx_int_status.hdmi_rx_int[1] & AUDIO_SAMPLE_CHANGE)
			sp_hdmi_rx_audio_samplechg_int(anx78xx);
	}
}

static void sp_tx_show_information(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 c, c1;
	u16 h_res, h_act, v_res, v_act;
	u16 h_fp, h_sw, h_bp, v_fp, v_sw, v_bp;
	ulong fresh_rate;
	ulong pclk;

	dev_dbg(dev, "\n***************SP Video Information****************\n");

	sp_read_reg(anx78xx, TX_P0, SP_TX_LINK_BW_SET_REG, &c);
	switch (c) {
	case 0x06:
		dev_dbg(dev, "BW = 1.62G\n");
		break;
	case 0x0a:
		dev_dbg(dev, "BW = 2.7G\n");
		break;
	case 0x14:
		dev_dbg(dev, "BW = 5.4G\n");
		break;
	case 0x19:
		dev_dbg(dev, "BW = 6.75G\n");
		break;
	default:
		break;
	}

	pclk = sp_tx_pclk_calc(anx78xx);
	pclk = pclk / 10;

	sp_read_reg(anx78xx, TX_P2, SP_TX_TOTAL_LINE_STA_L, &c);
	sp_read_reg(anx78xx, TX_P2, SP_TX_TOTAL_LINE_STA_H, &c1);

	v_res = c1;
	v_res = v_res << 8;
	v_res = v_res + c;


	sp_read_reg(anx78xx, TX_P2, SP_TX_ACT_LINE_STA_L, &c);
	sp_read_reg(anx78xx, TX_P2, SP_TX_ACT_LINE_STA_H, &c1);

	v_act = c1;
	v_act = v_act << 8;
	v_act = v_act + c;


	sp_read_reg(anx78xx, TX_P2, SP_TX_TOTAL_PIXEL_STA_L, &c);
	sp_read_reg(anx78xx, TX_P2, SP_TX_TOTAL_PIXEL_STA_H, &c1);

	h_res = c1;
	h_res = h_res << 8;
	h_res = h_res + c;


	sp_read_reg(anx78xx, TX_P2, SP_TX_ACT_PIXEL_STA_L, &c);
	sp_read_reg(anx78xx, TX_P2, SP_TX_ACT_PIXEL_STA_H, &c1);

	h_act = c1;
	h_act = h_act << 8;
	h_act = h_act + c;

	sp_read_reg(anx78xx, TX_P2, SP_TX_H_F_PORCH_STA_L, &c);
	sp_read_reg(anx78xx, TX_P2, SP_TX_H_F_PORCH_STA_H, &c1);

	h_fp = c1;
	h_fp = h_fp << 8;
	h_fp = h_fp + c;

	sp_read_reg(anx78xx, TX_P2, SP_TX_H_SYNC_STA_L, &c);
	sp_read_reg(anx78xx, TX_P2, SP_TX_H_SYNC_STA_H, &c1);

	h_sw = c1;
	h_sw = h_sw << 8;
	h_sw = h_sw + c;

	sp_read_reg(anx78xx, TX_P2, SP_TX_H_B_PORCH_STA_L, &c);
	sp_read_reg(anx78xx, TX_P2, SP_TX_H_B_PORCH_STA_H, &c1);

	h_bp = c1;
	h_bp = h_bp << 8;
	h_bp = h_bp + c;

	sp_read_reg(anx78xx, TX_P2, SP_TX_V_F_PORCH_STA, &c);
	v_fp = c;

	sp_read_reg(anx78xx, TX_P2, SP_TX_V_SYNC_STA, &c);
	v_sw = c;

	sp_read_reg(anx78xx, TX_P2, SP_TX_V_B_PORCH_STA, &c);
	v_bp = c;

	dev_dbg(dev, "Total resolution is %d * %d\n", h_res, v_res);

	dev_dbg(dev, "HF=%d, HSW=%d, HBP=%d\n", h_fp, h_sw, h_bp);
	dev_dbg(dev, "VF=%d, VSW=%d, VBP=%d\n", v_fp, v_sw, v_bp);
	dev_dbg(dev, "Active resolution is %d * %d", h_act, v_act);

	if (h_res == 0 || v_res == 0)
		fresh_rate = 0;
	else {
		fresh_rate = pclk * 1000;
		fresh_rate = fresh_rate / h_res;
		fresh_rate = fresh_rate * 1000;
		fresh_rate = fresh_rate / v_res;
	}
	dev_dbg(dev, "   @ %ldHz\n", fresh_rate);

	sp_read_reg(anx78xx, TX_P0, SP_TX_VID_CTRL, &c);

	if ((c & 0x06) == 0x00)
		dev_dbg(dev, "ColorSpace: RGB,");
	else if ((c & 0x06) == 0x02)
		dev_dbg(dev, "ColorSpace: YCbCr422,");
	else if ((c & 0x06) == 0x04)
		dev_dbg(dev, "ColorSpace: YCbCr444,");

	sp_read_reg(anx78xx, TX_P0, SP_TX_VID_CTRL, &c);

	if ((c & 0xe0) == 0x00)
		dev_dbg(dev, "6 BPC\n");
	else if ((c & 0xe0) == 0x20)
		dev_dbg(dev, "8 BPC\n");
	else if ((c & 0xe0) == 0x40)
		dev_dbg(dev, "10 BPC\n");
	else if ((c & 0xe0) == 0x60)
		dev_dbg(dev, "12 BPC\n");


	if (is_anx_dongle(anx78xx)) {
		sp_tx_aux_dpcdread_bytes(anx78xx, 0x00, 0x05, 0x23, 1, &c);
		dev_dbg(dev, "Analogix Dongle FW Ver %.2x\n", (u16)(c & 0x7f));
	}

	dev_dbg(dev, "\n**************************************************\n");
}

static void sp_clean_system_status(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;

	if (sp.need_clean_status) {
		dev_dbg(dev, "sp_clean_system_status. A -> B;\n");
		dev_dbg(dev, "A:");
		sp_print_sys_state(anx78xx, sp.tx_system_state_bak);
		dev_dbg(dev, "B:");
		sp_print_sys_state(anx78xx, sp.tx_system_state);

		sp.need_clean_status = 0;
		if (sp.tx_system_state_bak >= STATE_LINK_TRAINING) {
			if (sp.tx_system_state >= STATE_AUDIO_OUTPUT)
				hdmi_rx_mute_audio(anx78xx, 1);
			else {
				hdmi_rx_mute_video(anx78xx, 1);
				sp_tx_video_mute(anx78xx, 1);
			}
		}
		if (sp.tx_system_state_bak >= STATE_HDCP_AUTH
			&& sp.tx_system_state <= STATE_HDCP_AUTH) {
			if (sp_i2c_read_byte(anx78xx, TX_P0, TX_HDCP_CTRL0)
			    & 0xFC)
				sp_tx_clean_hdcp_status(anx78xx);
		}

		if (sp.hcdp_state != HDCP_CAPABLE_CHECK)
			sp.hcdp_state = HDCP_CAPABLE_CHECK;

		if (sp.tx_sc_state != SC_INIT)
			sp.tx_sc_state = SC_INIT;
		if (sp.tx_lt_state != LT_INIT)
			sp.tx_lt_state = LT_INIT;
		if (sp.tx_vo_state != VO_WAIT_VIDEO_STABLE)
			sp.tx_vo_state = VO_WAIT_VIDEO_STABLE;
	}
}

/******************add for HDCP cap check********************/
static u8 sp_hdcp_cap_check(struct anx78xx *anx78xx)
{
	struct device *dev = &anx78xx->client->dev;
	u8 g_hdcp_cap = 0;
	u8 temp;

	if (AUX_OK == sp_tx_aux_dpcdread_bytes(anx78xx, 0x06, 0x80, 0x28, 1,
						&temp))
		g_hdcp_cap = (temp & 0x01) ? 1 : 0;
	else
		dev_dbg(dev, "HDCP CAPABLE: read AUX err!\n");

	dev_dbg(dev, "hdcp cap check: %s Supported\n",
		g_hdcp_cap ? "" : "No");

	return g_hdcp_cap;
}
/******************End HDCP cap check********************/

static void sp_tasks_handler(struct anx78xx *anx78xx)
{
	sp_system_isr_handler(anx78xx);
	sp_hdcp_external_ctrl_flag_monitor(anx78xx);
	sp_clean_system_status(anx78xx);
	/*clear up backup system state*/
	if (sp.tx_system_state_bak != sp.tx_system_state)
		sp.tx_system_state_bak = sp.tx_system_state;
}
/******************End task  process********************/

void sp_main_process(struct anx78xx *anx78xx)
{
	sp_state_process(anx78xx);
	if (sp.tx_system_state > STATE_WAITTING_CABLE_PLUG) {
		sp_int_rec(anx78xx);
		sp_tasks_handler(anx78xx);
	}
}

