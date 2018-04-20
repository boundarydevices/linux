/*
 * include/linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ext.h
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

#ifndef __HDMI_TX_EXT_H__
#define __HDMI_TX_EXT_H__

void direct_hdcptx14_start(void);
void direct_hdcptx14_stop(void);

/*
 * HDMI TX output enable, such as ACRPacket/AudInfo/AudSample
 * enable: 1, normal output;  0: disable output
 */
void hdmitx_ext_set_audio_output(int enable);

/*
 * return Audio output status
 * 1: normal output status;  0: output disabled
 */
int hdmitx_ext_get_audio_status(void);

/*
 * For I2S interface, there are four input ports
 * I2S_0/I2S_1/I2S_2/I2S_3
 * ch_num: must be 2/4/6/8
 * ch_msk: Mask for channel_num
 * 2ch via I2S_0, set ch_num = 2 and ch_msk = 1
 * 4ch via I2S_1/I2S_2, set set ch_num = 4 and ch_msk = 6
 */
void hdmitx_ext_set_i2s_mask(char ch_num, char ch_msk);

/*
 * get I2S mask
 */
char hdmitx_ext_get_i2s_mask(void);

#endif
