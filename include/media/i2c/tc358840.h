/* SPDX-License-Identifier: GPL-2.0 */
/*
 * tc358840.h - Toshiba UH2C/D HDMI-CSI bridge driver
 *
 * Copyright (c) 2015, Armin Weiss <weii@zhaw.ch>
 * Copyright 2015-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef _TC358840_
#define _TC358840_

enum tc358840_csi_port {
	CSI_TX_NONE = 0,
	CSI_TX_0,
	CSI_TX_1,
	CSI_TX_BOTH
};

enum tc358840_clock_mode {
	CSI_NON_CONT_CLK = 0,
	CSI_CONT_CLK
};

enum tc358840_ddc5v_delays {
	DDC5V_DELAY_0MS,
	DDC5V_DELAY_50MS,
	DDC5V_DELAY_100MS,
	DDC5V_DELAY_200MS,
	DDC5V_DELAY_MAX = DDC5V_DELAY_200MS,
};

struct tc358840_platform_data {
	/* GPIOs */
	int reset_gpio;		/* Pin K8 */

	struct v4l2_fwnode_endpoint endpoint;

	/* System clock connected to REFCLK (pin K9) */
	u32 refclk_hz;		/* 40 - 50 MHz */

	/* DDC +5V debounce delay */
	enum tc358840_ddc5v_delays ddc5v_delay;

	/* CSI Output */
	enum tc358840_csi_port csi_port;

	/* CSI */
	/* The values in brackets can serve as a starting point. */
	u32 lineinitcnt;	/* (0x00000FA0) */
	u32 lptxtimecnt;	/* (0x00000004) */
	u32 tclk_headercnt;	/* (0x00180203) */
	u32 tclk_trailcnt;	/* (0x00040005) */
	u32 ths_headercnt;	/* (0x000D0004) */
	u32 twakeup;		/* (0x00003E80) */
	u32 tclk_postcnt;	/* (0x0000000A) */
	u32 ths_trailcnt;	/* (0x00080006) */
	u32 hstxvregcnt;	/* (0x00000020) */
	u32 btacnt;		/* (0x00040003) */

	/* PLL */
	/* Bps per lane is (refclk_hz / (prd + 1) * (fbd + 1)) / 2^frs */
	u16 pll_prd;
	u16 pll_fbd;
	u16 pll_frs;
};

/* custom controls */
#define V4L2_CID_USER_TC358840_BASE (V4L2_CID_BASE + 0x1000)

/* Audio sample rate in Hz */
#define TC358840_CID_AUDIO_SAMPLING_RATE (V4L2_CID_USER_TC358840_BASE + 1)
/* Audio present status */
#define TC358840_CID_AUDIO_PRESENT       (V4L2_CID_USER_TC358840_BASE + 2)
/* Splitter width */
#define TC358840_CID_SPLITTER_WIDTH      (V4L2_CID_USER_TC358840_BASE + 3)
/* TMDS present status */
#define TC358840_CID_TMDS_PRESENT        (V4L2_CID_USER_TC358840_BASE + 4)
/* EQ_BYPS mode */
#define TC358840_CID_EQ_BYPS_MODE        (V4L2_CID_USER_TC358840_BASE + 5)
#define TC358840_CID_EQ_BYPS_AUTO 0
#define TC358840_CID_EQ_BYPS_CLR  1
#define TC358840_CID_EQ_BYPS_SET  2

#endif /* _TC358840_ */
