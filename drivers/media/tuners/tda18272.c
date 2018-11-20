/*
	TDA18272 Silicon tuner driver
	Copyright (C) Manu Abraham <abraham.manu@gmail.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "dvb_frontend.h"

#include "tda18272.h"
#include "tda18272_reg.h"

static unsigned int verbose;
module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "Set Verbosity level");

#define FE_ERROR				0
#define FE_NOTICE				1
#define FE_INFO					2
#define FE_DEBUG				3
#define FE_DEBUGREG				4

#define dprintk(__y, __z, format, arg...) do {						\
	if (__z) {									\
		if	((verbose > FE_ERROR) && (verbose > __y))			\
			printk(KERN_ERR "%s: " format "\n", __func__, ##arg);		\
		else if	((verbose > FE_NOTICE) && (verbose > __y))			\
			printk(KERN_NOTICE "%s: " format "\n", __func__, ##arg);	\
		else if ((verbose > FE_INFO) && (verbose > __y))			\
			printk(KERN_INFO "%s: " format "\n", __func__, ##arg);		\
		else if ((verbose > FE_DEBUG) && (verbose > __y))			\
			printk(KERN_DEBUG "%s: " format "\n", __func__, ##arg);	\
	} else {									\
		if (verbose > __y)							\
			printk(format, ##arg);						\
	}										\
} while (0)

#define TDA18272_SETFIELD(mask, bitf, val)						\
	(mask = (mask & (~(((1 << TDA18272_WIDTH_##bitf) - 1) <<			\
				  TDA18272_OFFST_##bitf))) | 				\
			  (val << TDA18272_OFFST_##bitf))

#define TDA18272_GETFIELD(bitf, val)							\
	((val >> TDA18272_OFFST_##bitf) & 						\
	((1 << TDA18272_WIDTH_##bitf) - 1))


enum tda18272_lpf {
	TDA18272_LPF_6MHz	= 0,
	TDA18272_LPF_7MHz,
	TDA18272_LPF_8MHz,
	TDA18272_LPF_9MHz,
	TDA18272_LPF_1_5MHz
};

enum tda18272_lpf_offset {
	TDA18272_LPFOFFSET_0PC	= 0,
	TDA18272_LPFOFFSET_4PC,
	TDA18272_LPFOFFSET_8PC,
	TDA18272_LPFOFFSET_12PC
};

enum tda18272_agcgain {
	TDA18272_AGCGAIN_2VPP	= 0,
	TDA18272_AGCGAIN_1_25VPP,
	TDA18272_AGCGAIN_1VPP,
	TDA18272_AGCGAIN_0_8VPP,
	TDA18272_AGCGAIN_0_85VPP,
	TDA18272_AGCGAIN_0_7VPP,
	TDA18272_AGCGAIN_0_6VPP,
	TDA18272_AGCGAIN_0_5VPP
};

enum tda18272_notch {
	TDA18272_NOTCH_DISABLED	= 0,
	TDA18272_NOTCH_ENABLED,
};

enum tda18272_hpf {
	TDA18272_HPF_DISABLED	= 0,
	TDA18272_HPF_0_4MHz,
	TDA18272_HPF_0_85MHz,
	TDA18272_HPF_1MHz,
	TDA18272_HPF_1_5Mhz
};

enum tda18272_lnatop {
	TDA18272_LNATOP_95_89 = 0,
	TDA18272_LNATOP_95_93, /* unused */
	TDA18272_LNATOP_95_94, /* unused */
	TDA18272_LNATOP_95_95, /* unused */
	TDA18272_LNATOP_99_89,
	TDA18272_LNATOP_99_93,
	TDA18272_LNATOP_99_94,
	TDA18272_LNATOP_99_95,
	TDA18272_LNATOP_99_95s,
	TDA18272_LNATOP_100_93,
	TDA18272_LNATOP_100_94,
	TDA18272_LNATOP_100_95,
	TDA18272_LNATOP_100_95s,
	TDA18272_LNATOP_101_93d,
	TDA18272_LNATOP_101_94d,
	TDA18272_LNATOP_101_95,
	TDA18272_LNATOP_101_95s,
};

enum tda18272_rfatttop {
	TDA18272_RFATTTOP_89_81	= 0,
	TDA18272_RFATTTOP_91_83,
	TDA18272_RFATTTOP_93_85,
	TDA18272_RFATTTOP_95_87,
	TDA18272_RFATTTOP_88_88,
	TDA18272_RFATTTOP_89_82,
	TDA18272_RFATTTOP_90_83,
	TDA18272_RFATTTOP_91_84,
	TDA18272_RFATTTOP_92_85,
	TDA18272_RFATTTOP_93_86,
	TDA18272_RFATTTOP_94_87,
	TDA18272_RFATTTOP_95_88,
	TDA18272_RFATTTOP_87_81,
	TDA18272_RFATTTOP_88_82,
	TDA18272_RFATTTOP_89_83,
	TDA18272_RFATTTOP_90_84,
	TDA18272_RFATTTOP_91_85,
	TDA18272_RFATTTOP_92_86,
	TDA18272_RFATTTOP_93_87,
	TDA18272_RFATTTOP_94_88,
	TDA18272_RFATTTOP_95_89,
};


#define TDA18272_AGC3_RF_AGC_TOP_FREQ_LIM	291000000

enum tda18272_rfagctop {
	TDA18272_RFAGCTOP_94 = 0,
	TDA18272_RFAGCTOP_96,
	TDA18272_RFAGCTOP_98,
	TDA18272_RFAGCTOP_100,
	TDA18272_RFAGCTOP_102,
	TDA18272_RFAGCTOP_104,
	TDA18272_RFAGCTOP_106,
	TDA18272_RFAGCTOP_107,
};

enum tda18272_irmixtop {
	TDA18272_IRMIXTOP_105_99	= 0,
	TDA18272_IRMIXTOP_105_100,
	TDA18272_IRMIXTOP_105_101,
	TDA18272_IRMIXTOP_107_101,
	TDA18272_IRMIXTOP_107_102,
	TDA18272_IRMIXTOP_107_103,
	TDA18272_IRMIXTOP_108_103,
	TDA18272_IRMIXTOP_109_103,
	TDA18272_IRMIXTOP_109_104,
	TDA18272_IRMIXTOP_109_105,
	TDA18272_IRMIXTOP_110_104,
	TDA18272_IRMIXTOP_110_105,
	TDA18272_IRMIXTOP_110_106,
	TDA18272_IRMIXTOP_112_106,
	TDA18272_IRMIXTOP_112_107,
	TDA18272_IRMIXTOP_112_108,
};

enum tda18272_ifagctop {
	TDA18272_IFAGCTOP_105_99	= 0,
	TDA18272_IFAGCTOP_105_100,
	TDA18272_IFAGCTOP_105_101,
	TDA18272_IFAGCTOP_107_101,
	TDA18272_IFAGCTOP_107_102,
	TDA18272_IFAGCTOP_107_103,
	TDA18272_IFAGCTOP_108_103,
	TDA18272_IFAGCTOP_109_103,
	TDA18272_IFAGCTOP_109_104,
	TDA18272_IFAGCTOP_109_105,
	TDA18272_IFAGCTOP_110_104,
	TDA18272_IFAGCTOP_110_105,
	TDA18272_IFAGCTOP_110_106,
	TDA18272_IFAGCTOP_112_106,
	TDA18272_IFAGCTOP_112_107,
	TDA18272_IFAGCTOP_112_108,
};

enum tda18272_dethpf {
	TDA18272_DETHPF_DISABLED	= 0,
	TDA18272_DETHPF_ENABLED
};

enum tda18272_agc3adapt {
	TDA18272_AGC3ADAPT_ENABLED	= 0,
	TDA18272_AGC3ADAPT_DISABLED,
};

enum tda18272_agc3adapt_top {
	TDA18272_AGC3ADAPT_TOP_0	= 0,
	TDA18272_AGC3ADAPT_TOP_1,
	TDA18272_AGC3ADAPT_TOP_2,
	TDA18272_AGC3ADAPT_TOP_3
};

enum tda18272_3dbatt {
	TDA18272_3DBATT_DISABLED	= 0,
	TDA18272_3DBATT_ENABLED,
};


enum tda18272_vhffilt6 {
	TDA18272_VHFFILT6_DISABLED	= 0,
	TDA18272_VHFFILT6_ENABLED,
};

enum tda18272_lpfgain {
	TDA18272_LPFGAIN_UNKNOWN	= 0,
	TDA18272_LPFGAIN_FROZEN,
	TDA18272_LPFGAIN_FREE
};


enum tda18272_stdmode {
	TDA18272_DVBT_6MHz = 0,
	TDA18272_DVBT_7MHz,
	TDA18272_DVBT_8MHz,
	TDA18272_QAM_6MHz,
	TDA18272_QAM_8MHz,
	TDA18272_ISDBT_6MHz,
	TDA18272_ATSC_6MHz,
	TDA18272_DMBT_8MHz,
	TDA18272_ANLG_MN,
	TDA18272_ANLG_B,
	TDA18272_ANLG_GH,
	TDA18272_ANLG_I,
	TDA18272_ANLG_DK,
	TDA18272_ANLG_L,
	TDA18272_ANLG_LL,
	TDA18272_FM_RADIO,
	TDA18272_Scanning,
	TDA18272_ScanXpress,
};

static struct tda18272_coeff {
	u8				desc[16];
	u32				if_val;
	s32				cf_off;
	enum tda18272_lpf		lpf;
	enum tda18272_lpf_offset	lpf_off;
	enum tda18272_agcgain		if_gain;
	enum tda18272_notch		if_notch;
	enum tda18272_hpf		if_hpf;
	enum tda18272_notch		dc_notch;
	enum tda18272_lnatop		lna_top;
	enum tda18272_rfatttop		rfatt_top;
	enum tda18272_rfagctop		loband_rfagc_top;
	enum tda18272_rfagctop		hiband_rfagc_top;
	enum tda18272_irmixtop		irmix_top;
	enum tda18272_ifagctop		ifagc_top;
	enum tda18272_dethpf		det_hpf;
	enum tda18272_agc3adapt		agc3_adapt;
	enum tda18272_agc3adapt_top	agc3_adapt_top;

	enum tda18272_3dbatt		att3db;
	u8				gsk;
	enum tda18272_vhffilt6		filter;
	enum tda18272_lpfgain		lpf_gain;
	int				agc1_freeze;
	int				ltosto_immune;
} coeft[] = {
	{
		.desc			= "DVB-T 6MHz",
		.if_val			= 3250000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_6MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_1VPP,
		.if_notch		= TDA18272_NOTCH_ENABLED,
		.if_hpf			= TDA18272_HPF_0_4MHz,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_102,
		.irmix_top		= TDA18272_IRMIXTOP_110_105,
		.ifagc_top		= TDA18272_IFAGCTOP_110_105,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_ENABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_2,
		.att3db			= TDA18272_3DBATT_ENABLED,
		.gsk			= 0x02,
		.filter			= TDA18272_VHFFILT6_ENABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FREE,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "DVB-T 7MHz",
		.if_val			= 3500000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_7MHz,
		.lpf_off		= TDA18272_LPFOFFSET_8PC,
		.if_gain		= TDA18272_AGCGAIN_1VPP,
		.if_notch		= TDA18272_NOTCH_ENABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_102,
		.irmix_top		= TDA18272_IRMIXTOP_110_105,
		.ifagc_top		= TDA18272_IFAGCTOP_110_105,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_ENABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_2,
		.att3db			= TDA18272_3DBATT_ENABLED,
		.gsk			= 0x02,
		.filter			= TDA18272_VHFFILT6_ENABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FREE,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "DVB-T 8MHz",
		.if_val			= 4000000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_8MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_1VPP,
		.if_notch		= TDA18272_NOTCH_ENABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_102,
		.irmix_top		= TDA18272_IRMIXTOP_110_105,
		.ifagc_top		= TDA18272_IFAGCTOP_110_105,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_ENABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_2,
		.att3db			= TDA18272_3DBATT_ENABLED,
		.gsk			= 0x02,
		.filter			= TDA18272_VHFFILT6_ENABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FREE,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "QAM 6MHz",
		.if_val			= 3600000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_6MHz,
		.lpf_off		= TDA18272_LPFOFFSET_8PC,
		.if_gain		= TDA18272_AGCGAIN_1VPP,
		.if_notch		= TDA18272_NOTCH_DISABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.irmix_top		= TDA18272_IRMIXTOP_110_105,
		.ifagc_top		= TDA18272_IFAGCTOP_110_105,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x02,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FREE,
		.agc1_freeze		= 1,
		.ltosto_immune		= 1
	}, {
		.desc			= "QAM 8MHz",
		.if_val			= 5000000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_9MHz,
		.lpf_off		= TDA18272_LPFOFFSET_8PC,
		.if_gain		= TDA18272_AGCGAIN_1VPP,
		.if_notch		= TDA18272_NOTCH_DISABLED,
		.if_hpf			= TDA18272_HPF_0_85MHz,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.irmix_top		= TDA18272_IRMIXTOP_110_105,
		.ifagc_top		= TDA18272_IFAGCTOP_110_105,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x02,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FREE,
		.agc1_freeze		= 1,
		.ltosto_immune		= 1
	}, {
		.desc			= "ISDB-T 6MHz",
		.if_val			= 3250000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_6MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_6VPP,
		.if_notch		= TDA18272_NOTCH_ENABLED,
		.if_hpf			= TDA18272_HPF_0_4MHz,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_102,
		.irmix_top		= TDA18272_IRMIXTOP_110_105,
		.ifagc_top		= TDA18272_IFAGCTOP_110_105,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_ENABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_2,
		.att3db			= TDA18272_3DBATT_ENABLED,
		.gsk			= 0x02,
		.filter			= TDA18272_VHFFILT6_ENABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FREE,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "ATSC 6MHz",
		.if_val			= 3250000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_6MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_6VPP,
		.if_notch		= TDA18272_NOTCH_ENABLED,
		.if_hpf			= TDA18272_HPF_0_4MHz,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_100_94,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_104,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_104,
		.irmix_top		= TDA18272_IRMIXTOP_112_107,
		.ifagc_top		= TDA18272_IFAGCTOP_112_107,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_ENABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_3,
		.att3db			= TDA18272_3DBATT_ENABLED,
		.gsk			= 0x02,
		.filter			= TDA18272_VHFFILT6_ENABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FREE,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "DMB-T 8MHz",
		.if_val			= 4000000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_8MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_1VPP,
		.if_notch		= TDA18272_NOTCH_ENABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_102,
		.irmix_top		= TDA18272_IRMIXTOP_110_105,
		.ifagc_top		= TDA18272_IFAGCTOP_110_105,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_ENABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_2,
		.att3db			= TDA18272_3DBATT_ENABLED,
		.gsk			= 0x02,
		.filter			= TDA18272_VHFFILT6_ENABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FREE,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "ATV M/N",
		.if_val			= 5400000,
		.cf_off			= 1750000,
		.lpf			= TDA18272_LPF_6MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_7VPP,
		.if_notch		= TDA18272_NOTCH_DISABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_DISABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.irmix_top		= TDA18272_IRMIXTOP_105_100,
		.ifagc_top		= TDA18272_IFAGCTOP_105_100,
		.det_hpf		= TDA18272_DETHPF_ENABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x01,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FROZEN,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "ATV B",
		.if_val			= 6400000,
		.cf_off			= 2250000,
		.lpf			= TDA18272_LPF_7MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_7VPP,
		.if_notch		= TDA18272_NOTCH_DISABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_DISABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.irmix_top		= TDA18272_IRMIXTOP_105_100,
		.ifagc_top		= TDA18272_IFAGCTOP_105_100,
		.det_hpf		= TDA18272_DETHPF_ENABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x01,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FROZEN,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "ATV G/H",
		.if_val			= 6750000,
		.cf_off			= 2750000,
		.lpf			= TDA18272_LPF_8MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_7VPP,
		.if_notch		= TDA18272_NOTCH_DISABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_DISABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.irmix_top		= TDA18272_IRMIXTOP_105_100,
		.ifagc_top		= TDA18272_IFAGCTOP_105_100,
		.det_hpf		= TDA18272_DETHPF_ENABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x01,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FROZEN,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "ATV I",
		.if_val			= 7250000,
		.cf_off			= 2750000,
		.lpf			= TDA18272_LPF_8MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_7VPP,
		.if_notch		= TDA18272_NOTCH_DISABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_DISABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.irmix_top		= TDA18272_IRMIXTOP_105_100,
		.ifagc_top		= TDA18272_IFAGCTOP_105_100,
		.det_hpf		= TDA18272_DETHPF_ENABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x01,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FROZEN,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "ATV DK",
		.if_val			= 6850000,
		.cf_off			= 2750000,
		.lpf			= TDA18272_LPF_8MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_7VPP,
		.if_notch		= TDA18272_NOTCH_ENABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_DISABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.irmix_top		= TDA18272_IRMIXTOP_105_100,
		.ifagc_top		= TDA18272_IFAGCTOP_105_100,
		.det_hpf		= TDA18272_DETHPF_ENABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x01,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FROZEN,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "ATV L",
		.if_val			= 6750000,
		.cf_off			= 2750000,
		.lpf			= TDA18272_LPF_8MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_7VPP,
		.if_notch		= TDA18272_NOTCH_ENABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_DISABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.irmix_top		= TDA18272_IRMIXTOP_105_100,
		.ifagc_top		= TDA18272_IFAGCTOP_105_100,
		.det_hpf		= TDA18272_DETHPF_ENABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x01,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FROZEN,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "ATV Lc",
		.if_val			= 1250000,
		.cf_off			= -2750000,
		.lpf			= TDA18272_LPF_8MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_7VPP,
		.if_notch		= TDA18272_NOTCH_DISABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_DISABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.irmix_top		= TDA18272_IRMIXTOP_105_100,
		.ifagc_top		= TDA18272_IFAGCTOP_105_100,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x01,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FROZEN,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "FM Radio",
		.if_val			= 1250000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_1_5MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_7VPP,
		.if_notch		= TDA18272_NOTCH_DISABLED,
		.if_hpf			= TDA18272_HPF_0_85MHz,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.irmix_top		= TDA18272_IRMIXTOP_105_100,
		.ifagc_top		= TDA18272_IFAGCTOP_105_100,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x02,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FROZEN,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "PAL I Blindscan",
		.if_val			= 7250000,
		.cf_off			= 2750000,
		.lpf			= TDA18272_LPF_8MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_0_7VPP,
		.if_notch		= TDA18272_NOTCH_DISABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_DISABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_96,
		.irmix_top		= TDA18272_IRMIXTOP_105_100,
		.ifagc_top		= TDA18272_IFAGCTOP_105_100,
		.det_hpf		= TDA18272_DETHPF_ENABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_DISABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_0,
		.att3db			= TDA18272_3DBATT_DISABLED,
		.gsk			= 0x01,
		.filter			= TDA18272_VHFFILT6_DISABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FROZEN,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, {
		.desc			= "XpressScan",
		.if_val			= 5000000,
		.cf_off			= 0,
		.lpf			= TDA18272_LPF_9MHz,
		.lpf_off		= TDA18272_LPFOFFSET_0PC,
		.if_gain		= TDA18272_AGCGAIN_1VPP,
		.if_notch		= TDA18272_NOTCH_ENABLED,
		.if_hpf			= TDA18272_HPF_DISABLED,
		.dc_notch		= TDA18272_NOTCH_ENABLED,
		.lna_top		= TDA18272_LNATOP_95_89,
		.rfatt_top		= TDA18272_RFATTTOP_90_84,
		.loband_rfagc_top	= TDA18272_RFAGCTOP_100,
		.hiband_rfagc_top	= TDA18272_RFAGCTOP_102,
		.irmix_top		= TDA18272_IRMIXTOP_110_105,
		.ifagc_top		= TDA18272_IFAGCTOP_110_105,
		.det_hpf		= TDA18272_DETHPF_DISABLED,
		.agc3_adapt		= TDA18272_AGC3ADAPT_ENABLED,
		.agc3_adapt_top		= TDA18272_AGC3ADAPT_TOP_2,
		.att3db			= TDA18272_3DBATT_ENABLED,
		.gsk			= 0x0e,
		.filter			= TDA18272_VHFFILT6_ENABLED,
		.lpf_gain		= TDA18272_LPFGAIN_FREE,
		.agc1_freeze		= 0,
		.ltosto_immune		= 0
	}, { }
};

#define TDA18272_REGMAPSIZ	68

struct tda18272_state {
	const struct tda18272_coeff	*coe;
	u8				lna_top;
	u8				psm_agc;
	u8				agc1;
	u8				mode;

	u8				ms;

	u32				bandwidth;
	u32				frequency;

	u8				regs[TDA18272_REGMAPSIZ];
	struct dvb_frontend		*fe;
	struct i2c_adapter		*i2c;
	const struct tda18272_config	*config;
};

static int tda18272_rd_regs(struct tda18272_state *tda18272, u8 reg, u8 *data, int count)
{
	int ret;
	const struct tda18272_config *config	= tda18272->config;
	struct dvb_frontend *fe			= tda18272->fe;
	struct i2c_msg msg[]			= {
		{ .addr = config->addr, .flags = 0, 	   .buf = &reg, .len = 1 },
		{ .addr = config->addr, .flags = I2C_M_RD, .buf = data, .len = count }
	};

	BUG_ON(count >= 255);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	ret = i2c_transfer(tda18272->i2c, msg, 2);
	if (ret != 2) {
		dprintk(FE_ERROR, 1, "I/O Error");
		ret = -EREMOTEIO;
	} else {
		ret = 0;
	}

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	return ret;
}

static int tda18272_wr_regs(struct tda18272_state *tda18272, u8 start, u8 *data, u8 count)
{
	int ret;
	const struct tda18272_config *config	= tda18272->config;
	struct dvb_frontend *fe			= tda18272->fe;
	u8 buf[0x45];
	struct i2c_msg msg = { .addr = config->addr, .flags = 0, .buf = buf, .len = count + 1 };

	BUG_ON(count >= 0x44);
	BUG_ON(start >= 0x43);
	BUG_ON(start + count > 0x44);

	buf[0] = start;
	memcpy(&buf[1], data, count);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	ret = i2c_transfer(tda18272->i2c, &msg, 1);
	if (ret != 1) {
		dprintk(FE_ERROR, 1, "I/O Error");
		ret = -EREMOTEIO;
	} else {
		ret = 0;
	}

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	return ret;
}

static int tda18272_wr(struct tda18272_state *tda18272, u8 reg, u8 data)
{
	return tda18272_wr_regs(tda18272, reg, &data, 1);
}

static int tda18272_rd(struct tda18272_state *tda18272, u8 reg, u8 *data)
{
	return tda18272_rd_regs(tda18272, reg, data, 1);
}

static int tda18272_cal_wait(struct tda18272_state *tda18272)
{
	int ret = 0;
	u8 xtal_cal, count = 20;

	while (count > 0) {
		ret = tda18272_rd(tda18272, TDA18272_IRQ_STATUS, &tda18272->regs[TDA18272_IRQ_STATUS]);
		xtal_cal = TDA18272_GETFIELD(IRQ_STATUS_XTALCAL_STATUS, tda18272->regs[TDA18272_IRQ_STATUS]);
		if (ret)
			break;

		if (xtal_cal)
			break;

		msleep(5);
		--count;
		if (!count) {
			ret = -1;
			break;
		}
	}
	dprintk(FE_DEBUG, 1, "ret=%d", ret);
	return ret;
}

enum tda18272_power {
	TDA18272_NORMAL = 0,
	TDA18272_STDBY_1,
	TDA18272_STDBY_2,
	TDA18272_STDBY
};

static int tda18272_pstate(struct tda18272_state *tda18272, enum tda18272_power pstate)
{
	int ret;

	ret = tda18272_rd_regs(tda18272, TDA18272_POWERSTATE_BYTE_2, &tda18272->regs[TDA18272_POWERSTATE_BYTE_2], 15);
	if (ret)
		goto err;

	if (pstate != TDA18272_NORMAL) {
		TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_DIGITAL_CLOCK, 0);
		ret = tda18272_wr(tda18272, TDA18272_REFERENCE, tda18272->regs[TDA18272_REFERENCE]);
		if (ret)
			goto err;
	}

	switch (pstate) {
	case TDA18272_NORMAL:
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM, 0x00);
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM_PLL, 0x00);
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM_LNA, 0x00);
		break;
	case TDA18272_STDBY_1:
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM, 0x01);
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM_PLL, 0x00);
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM_LNA, 0x00);
		break;
	case TDA18272_STDBY_2:
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM, 0x01);
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM_PLL, 0x01);
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM_LNA, 0x00);
		break;
	case TDA18272_STDBY:
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM, 0x01);
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM_PLL, 0x01);
		TDA18272_SETFIELD(tda18272->regs[TDA18272_POWERSTATE_BYTE_2], POWERSTATE_BYTE_2_SM_LNA, 0x01);
		break;
	}
	ret = tda18272_wr(tda18272, TDA18272_POWERSTATE_BYTE_2, tda18272->regs[TDA18272_POWERSTATE_BYTE_2]);
	if (ret)
		goto err;

	if (pstate == TDA18272_NORMAL) {
		if (tda18272->ms)
			TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_XTOUT, 0x03);

		TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_DIGITAL_CLOCK, 0x01);
		ret = tda18272_wr(tda18272, TDA18272_REFERENCE, tda18272->regs[TDA18272_REFERENCE]);
		if (ret)
			goto err;
	}
err:
	dprintk(FE_DEBUG, 1, "ret=%d", ret);
	return ret;
}

static int tda18272_wait_irq(struct tda18272_state *tda18272, u32 timeout, u32 step, u8 status)
{
	int ret;
	u8 irq_status;
	u32 count = timeout / step;

	BUG_ON(!count);
	do {
		ret = tda18272_rd(tda18272, TDA18272_IRQ_STATUS, &tda18272->regs[TDA18272_IRQ_STATUS]);
		if (ret)
			break;

		if (TDA18272_GETFIELD(IRQ_STATUS_IRQ_STATUS, tda18272->regs[TDA18272_IRQ_STATUS]))
			break;

		if (status) {
			irq_status = tda18272->regs[TDA18272_IRQ_STATUS] & 0x1f;
			if (status == irq_status)
				break;
		}
		msleep(step);
		--count;
		if (!count) {
			ret = -1;
			break;
		}
	} while (count);
	dprintk(FE_DEBUG, 1, "ret=%d", ret);
	return ret;
}

static int tda18272_reset(struct tda18272_state *tda18272)
{
	int ret;

	ret = tda18272_rd_regs(tda18272, TDA18272_ID_BYTE_1, tda18272->regs, TDA18272_REGMAPSIZ);
	if (ret)
		goto err;
	TDA18272_SETFIELD(tda18272->regs[TDA18272_POWER_BYTE_2], POWER_BYTE_2_RSSI_CK_SPEED, 0x00);
	ret = tda18272_wr(tda18272, TDA18272_POWER_BYTE_2, tda18272->regs[TDA18272_POWER_BYTE_2]);
	if (ret)
		goto err;
	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_2], AGC1_BYTE_2_AGC1_DO_STEP, 0x02);
	ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_2, tda18272->regs[TDA18272_AGC1_BYTE_2]);
	if (ret)
		goto err;
	TDA18272_SETFIELD(tda18272->regs[TDA18272_RF_FILTER_BYTE_3], RF_FILTER_BYTE_3_AGC2_DO_STEP, 0x01);
	ret = tda18272_wr(tda18272, TDA18272_RF_FILTER_BYTE_3, tda18272->regs[TDA18272_RF_FILTER_BYTE_3]);
	if (ret)
		goto err;
	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGCK_BYTE_1], AGC1_BYTE_1_AGCs_UP_STEP_ASYM, 0x03);
	ret = tda18272_wr(tda18272, TDA18272_AGCK_BYTE_1, tda18272->regs[TDA18272_AGCK_BYTE_1]);
	if (ret)
		goto err;
	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC5_BYTE_1], AGC5_BYTE_1_AGCs_DO_STEP_ASYM, 0x02);
	ret = tda18272_wr(tda18272, TDA18272_AGC5_BYTE_1, tda18272->regs[TDA18272_AGC5_BYTE_1]);
	if (ret)
		goto err;
	ret = tda18272_wr(tda18272, TDA18272_IRQ_CLEAR, 0x9f);
	if (ret)
		goto err;
	ret = tda18272_pstate(tda18272, TDA18272_NORMAL);
	if (ret) {
		dprintk(FE_ERROR, 1, "Power state switch failed, ret=%d", ret);
		goto err;
	}
	tda18272->regs[TDA18272_MSM_BYTE_1] = 0x38;
	tda18272->regs[TDA18272_MSM_BYTE_2] = 0x01;
	ret = tda18272_wr_regs(tda18272, TDA18272_MSM_BYTE_1, &tda18272->regs[TDA18272_MSM_BYTE_1], 2);
	if (ret)
		goto err;

	ret = tda18272_wait_irq(tda18272, 1500, 50, 0x1f);
	if (ret)
		goto err;
err:
	dprintk(FE_DEBUG, 1, "ret=%d", ret);
	return ret;
}

static int tda18272_init(struct dvb_frontend *fe)
{
	struct tda18272_state *tda18272 = fe->tuner_priv;
	int ret;

	if (tda18272->mode) {
		dprintk(FE_DEBUG, 1, "Initializing Master ..");
		ret = tda18272_cal_wait(tda18272);
		if (ret)
			goto err;
	} else {
		dprintk(FE_DEBUG, 1, "Initializing Slave ..");
		TDA18272_SETFIELD(tda18272->regs[TDA18272_FLO_MAX_BYTE], FLO_MAX_BYTE_FMAX_LO, 0x00);
		ret = tda18272_wr(tda18272, TDA18272_FLO_MAX_BYTE, tda18272->regs[TDA18272_FLO_MAX_BYTE]);
		if (ret)
			goto err;
		TDA18272_SETFIELD(tda18272->regs[TDA18272_CP_CURRENT], CP_CURRENT_N_CP_CURRENT, 0x68);
		ret = tda18272_wr(tda18272, TDA18272_CP_CURRENT, tda18272->regs[TDA18272_CP_CURRENT]);
	}
	ret = tda18272_reset(tda18272);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_FLO_MAX_BYTE], FLO_MAX_BYTE_FMAX_LO, 0x0a);
	ret = tda18272_wr(tda18272, TDA18272_FLO_MAX_BYTE, tda18272->regs[TDA18272_FLO_MAX_BYTE]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_1], AGC1_BYTE_1_LT_ENABLE, tda18272->lna_top);
	ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_1, tda18272->regs[TDA18272_AGC1_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_PSM_BYTE_1], PSM_BYTE_1_PSM_AGC1, tda18272->psm_agc);
	ret = tda18272_wr(tda18272, TDA18272_PSM_BYTE_1, tda18272->regs[TDA18272_PSM_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_1], AGC1_BYTE_1_AGC1_6_15DB, tda18272->agc1);
	ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_1, tda18272->regs[TDA18272_AGC1_BYTE_1]);
	if (ret)
		goto err;
err:
	dprintk(FE_DEBUG, 1, "ret=%d", ret);
	return ret;
}

static int tda18272_clear_irq(struct tda18272_state *tda18272, u8 status)
{
	tda18272->regs[TDA18272_IRQ_CLEAR] = status & 0x1f;
	TDA18272_SETFIELD(tda18272->regs[TDA18272_IRQ_CLEAR], IRQ_CLEAR_IRQ_CLEAR, 0x80);
	return tda18272_wr(tda18272, TDA18272_IRQ_CLEAR, tda18272->regs[TDA18272_IRQ_CLEAR]);
}

static int tda18272_set_rf(struct tda18272_state *tda18272, u32 freq)
{
	u32 tmp;
	int ret;

	ret = tda18272_clear_irq(tda18272, 0x0c);
	if (ret)
		goto err;

	ret = tda18272_pstate(tda18272, TDA18272_NORMAL);
	if (ret)
		goto err;

	tmp = freq / 1000;
	tda18272->regs[TDA18272_RF_FREQUENCY_BYTE_1] = (u8) ((tmp & 0xff0000) >> 16);
	tda18272->regs[TDA18272_RF_FREQUENCY_BYTE_2] = (u8) ((tmp & 0x00ff00) >>  8);
	tda18272->regs[TDA18272_RF_FREQUENCY_BYTE_3] = (u8)  (tmp & 0x0000ff);
	ret = tda18272_wr_regs(tda18272, TDA18272_RF_FREQUENCY_BYTE_1, &tda18272->regs[TDA18272_RF_FREQUENCY_BYTE_1], 3);
	if (ret)
		goto err;

	tda18272->regs[TDA18272_MSM_BYTE_1] = 0x41;
	tda18272->regs[TDA18272_MSM_BYTE_2] = 0x01;
	ret = tda18272_wr_regs(tda18272, TDA18272_MSM_BYTE_1, &tda18272->regs[TDA18272_MSM_BYTE_1], 2);
	if (ret)
		goto err;

	ret = tda18272_wait_irq(tda18272, 50, 5, 0x0c);
	if (ret)
		goto err;
err:
	dprintk(FE_DEBUG, 1, "ret=%d", ret);
	return ret;
}

static int tda18272_set_frequency(struct tda18272_state *tda18272, u32 frequency)
{
	int ret;

	u8 ratio_l, ratio_h;
	u32 delta_l, delta_h;
	u8 loop_off, rffilt_gv = 0;

	u8 g1, count, agc1, agc1_steps, done = 0;
	s16 steps_up, steps_down;

	const struct tda18272_coeff *coe = tda18272->coe;

	dprintk(FE_DEBUG, 1, "set freq=%d", frequency);

	TDA18272_SETFIELD(tda18272->regs[TDA18272_IF_BYTE_1], IF_BYTE_1_LP_FC, coe->lpf); /* LPF */
	ret = tda18272_wr(tda18272, TDA18272_IF_BYTE_1, tda18272->regs[TDA18272_IF_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_IF_BYTE_1], IF_BYTE_1_LP_FC_OFFSET, coe->lpf_off);
	ret = tda18272_wr(tda18272, TDA18272_IF_BYTE_1, tda18272->regs[TDA18272_IF_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_IFAGC], IFAGC_IF_LEVEL, coe->if_gain);
	ret = tda18272_wr(tda18272, TDA18272_IFAGC, tda18272->regs[TDA18272_IFAGC]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_IF_BYTE_1], IF_BYTE_1_IF_NOTCH, coe->if_notch);
	ret = tda18272_wr(tda18272, TDA18272_IF_BYTE_1, tda18272->regs[TDA18272_IF_BYTE_1]);
	if (ret)
		goto err;

	if (coe->if_hpf == TDA18272_HPF_DISABLED) {
		TDA18272_SETFIELD(tda18272->regs[TDA18272_IRMIXER_BYTE_2], IRMIXER_BYTE_2_HI_PASS, 0x0);
		ret = tda18272_wr(tda18272, TDA18272_IRMIXER_BYTE_2, tda18272->regs[TDA18272_IRMIXER_BYTE_2]);
		if (ret)
			goto err;
	} else {
		TDA18272_SETFIELD(tda18272->regs[TDA18272_IRMIXER_BYTE_2], IRMIXER_BYTE_2_HI_PASS, 0x1);
		ret = tda18272_wr(tda18272, TDA18272_IRMIXER_BYTE_2, tda18272->regs[TDA18272_IRMIXER_BYTE_2]);
		if (ret)
			goto err;
		TDA18272_SETFIELD(tda18272->regs[TDA18272_IF_BYTE_1], IF_BYTE_1_IF_HP_FC, (coe->if_hpf - 1));
		ret = tda18272_wr(tda18272, TDA18272_IF_BYTE_1, tda18272->regs[TDA18272_IF_BYTE_1]);
		if (ret)
			goto err;
	}

	TDA18272_SETFIELD(tda18272->regs[TDA18272_IRMIXER_BYTE_2], IRMIXER_BYTE_2_DC_NOTCH, coe->dc_notch);
	ret = tda18272_wr(tda18272, TDA18272_IRMIXER_BYTE_2, tda18272->regs[TDA18272_IRMIXER_BYTE_2]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_1], AGC1_BYTE_1_AGC1_TOP, coe->lna_top);
	ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_1, tda18272->regs[TDA18272_AGC1_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC2_BYTE_1], AGC2_BYTE_1_AGC2_TOP, coe->rfatt_top);
	ret = tda18272_wr(tda18272, TDA18272_AGC2_BYTE_1, tda18272->regs[TDA18272_AGC2_BYTE_1]);
	if (ret)
		goto err;

	if (frequency < TDA18272_AGC3_RF_AGC_TOP_FREQ_LIM)
		TDA18272_SETFIELD(tda18272->regs[TDA18272_RFAGC_BYTE_1], RFAGC_BYTE_1_AGC3_TOP, coe->loband_rfagc_top);
	else
		TDA18272_SETFIELD(tda18272->regs[TDA18272_RFAGC_BYTE_1], RFAGC_BYTE_1_AGC3_TOP, coe->hiband_rfagc_top);
	ret = tda18272_wr(tda18272, TDA18272_RFAGC_BYTE_1, tda18272->regs[TDA18272_RFAGC_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_IRMIXER_BYTE_1], IRMIXER_BYTE_1_AGC4_TOP, coe->irmix_top);
	ret = tda18272_wr(tda18272, TDA18272_IRMIXER_BYTE_1, tda18272->regs[TDA18272_IRMIXER_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC5_BYTE_1], AGC5_BYTE_1_AGC5_TOP, coe->ifagc_top);
	ret = tda18272_wr(tda18272, TDA18272_AGC5_BYTE_1, tda18272->regs[TDA18272_AGC5_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_RFAGC_BYTE_1], RFAGC_BYTE_1_PD_RFAGC_ADAPT, coe->agc3_adapt);
	ret = tda18272_wr(tda18272, TDA18272_RFAGC_BYTE_1, tda18272->regs[TDA18272_RFAGC_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_RFAGC_BYTE_1], RFAGC_BYTE_1_RFAGC_ADAPT_TOP, coe->agc3_adapt_top);
	ret = tda18272_wr(tda18272, TDA18272_RFAGC_BYTE_1, tda18272->regs[TDA18272_RFAGC_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_RFAGC_BYTE_1], RFAGC_BYTE_1_RF_ATTEN_3DB, coe->att3db);
	ret = tda18272_wr(tda18272, TDA18272_RFAGC_BYTE_1, tda18272->regs[TDA18272_RFAGC_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC5_BYTE_1], AGC5_BYTE_1_AGC5_HPF, coe->det_hpf);
	ret = tda18272_wr(tda18272, TDA18272_AGC5_BYTE_1, tda18272->regs[TDA18272_AGC5_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGCK_BYTE_1], AGCK_BYTE_1_AGCK_MODE, coe->gsk & 0x03);
	ret = tda18272_wr(tda18272, TDA18272_AGCK_BYTE_1, tda18272->regs[TDA18272_AGCK_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_AGCK_BYTE_1], AGCK_BYTE_1_AGCK_STEP, (coe->gsk & 0x0c) >> 2);
	ret = tda18272_wr(tda18272, TDA18272_AGCK_BYTE_1, tda18272->regs[TDA18272_AGCK_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_PSM_BYTE_1], PSM_BYTE_1_PSM_STOB, coe->filter);
	ret = tda18272_wr(tda18272, TDA18272_PSM_BYTE_1, tda18272->regs[TDA18272_PSM_BYTE_1]);
	if (ret)
		goto err;

	TDA18272_SETFIELD(tda18272->regs[TDA18272_IF_FREQUENCY], IF_FREQUENCY_IF_FREQ, (coe->if_val - coe->cf_off) / 50000);
	ret = tda18272_wr(tda18272, TDA18272_IF_FREQUENCY, tda18272->regs[TDA18272_IF_FREQUENCY]);
	if (ret)
		goto err;

	if (coe->ltosto_immune && tda18272->mode) {
		ret = tda18272_rd(tda18272, TDA18272_RF_AGC_GAIN_BYTE_1, &tda18272->regs[TDA18272_RF_AGC_GAIN_BYTE_1]);
		if (ret)
			goto err;
		rffilt_gv = TDA18272_GETFIELD(RF_AGC_GAIN_BYTE_1_RF_FILTER_GAIN, tda18272->regs[TDA18272_RF_AGC_GAIN_BYTE_1]);

		TDA18272_SETFIELD(tda18272->regs[TDA18272_RF_FILTER_BYTE_1], RF_FILTER_BYTE_1_RF_FILTER_GV, rffilt_gv);
		ret = tda18272_wr(tda18272, TDA18272_RF_FILTER_BYTE_1, tda18272->regs[TDA18272_RF_FILTER_BYTE_1]);
		if (ret)
			goto err;

		TDA18272_SETFIELD(tda18272->regs[TDA18272_RF_FILTER_BYTE_1], RF_FILTER_BYTE_1_FORCE_AGC2_GAIN, 0x1);
		ret = tda18272_wr(tda18272, TDA18272_RF_FILTER_BYTE_1, tda18272->regs[TDA18272_RF_FILTER_BYTE_1]);
		if (ret)
			goto err;

		if (rffilt_gv) {
			do {
				TDA18272_SETFIELD(tda18272->regs[TDA18272_RF_FILTER_BYTE_1], RF_FILTER_BYTE_1_RF_FILTER_GV, (rffilt_gv - 1));
				ret = tda18272_wr(tda18272, TDA18272_RF_FILTER_BYTE_1, tda18272->regs[TDA18272_RF_FILTER_BYTE_1]);
				if (ret)
					goto err;

				msleep(10);
				rffilt_gv -= 1;
			} while (rffilt_gv > 0);
		}
		TDA18272_SETFIELD(tda18272->regs[TDA18272_RFAGC_BYTE_1], RFAGC_BYTE_1_RF_ATTEN_3DB, 0x01);
		ret = tda18272_wr(tda18272, TDA18272_RFAGC_BYTE_1, tda18272->regs[TDA18272_RFAGC_BYTE_1]);
		if (ret)
			goto err;
	}
	ret = tda18272_set_rf(tda18272, frequency + coe->cf_off);
	if (ret)
		goto err;

	if (coe->ltosto_immune && tda18272->mode) {
		TDA18272_SETFIELD(tda18272->regs[TDA18272_RFAGC_BYTE_1], RFAGC_BYTE_1_RF_ATTEN_3DB, 0x00);
		ret = tda18272_wr(tda18272, TDA18272_RFAGC_BYTE_1, tda18272->regs[TDA18272_RFAGC_BYTE_1]);
		if (ret)
			goto err;

		msleep(50);
		TDA18272_SETFIELD(tda18272->regs[TDA18272_RF_FILTER_BYTE_1], RF_FILTER_BYTE_1_FORCE_AGC2_GAIN, 0x1);
		ret = tda18272_wr(tda18272, TDA18272_RF_FILTER_BYTE_1, tda18272->regs[TDA18272_RF_FILTER_BYTE_1]);
		if (ret)
			goto err;
	}
	ratio_l = (u8)(frequency / 16000000);
	ratio_h = (u8)(frequency / 16000000) + 1;
	delta_l = (frequency - (ratio_l * 16000000));
	delta_h = ((ratio_h * 16000000) - frequency);

	if (frequency < 72000000) {
		TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_DIGITAL_CLOCK, 0x1);
	} else if (frequency < 104000000) {
		TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_DIGITAL_CLOCK, 0x0);
	} else if (frequency <= 120000000) {
		TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_DIGITAL_CLOCK, 0x1);
	} else {
		if (delta_l <= delta_h) {
			if (ratio_l & 0x000001)
				TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_DIGITAL_CLOCK, 0x0);
			else
				TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_DIGITAL_CLOCK, 0x1);
		} else {
			if (ratio_l & 0x000001)
				TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_DIGITAL_CLOCK, 0x1);
			else
				TDA18272_SETFIELD(tda18272->regs[TDA18272_REFERENCE], REFERENCE_DIGITAL_CLOCK, 0x0);
		}
	}
	ret = tda18272_wr(tda18272, TDA18272_REFERENCE, tda18272->regs[TDA18272_REFERENCE]);
	if (ret)
		goto err;

	if (coe->agc1_freeze) {
		tda18272_rd(tda18272, TDA18272_AGC1_BYTE_2, &tda18272->regs[TDA18272_AGC1_BYTE_2]);
		loop_off = TDA18272_GETFIELD(AGC1_BYTE_2_AGC1_LOOP_OFF, tda18272->regs[TDA18272_AGC1_BYTE_2]);
		if (!loop_off) {
			TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_2], AGC1_BYTE_2_AGC1_LOOP_OFF, 0x1);
			ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_2, tda18272->regs[TDA18272_AGC1_BYTE_2]);
			if (ret)
				goto err;
			TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_2], AGC1_BYTE_2_FORCE_AGC1_GAIN, 0x01);
			ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_2, tda18272->regs[TDA18272_AGC1_BYTE_2]);
			if (ret)
				goto err;
		}
		if (!TDA18272_GETFIELD(AGC1_BYTE_1_AGC1_6_15DB, tda18272->regs[TDA18272_AGC1_BYTE_1])) {
			agc1 = 0;
			agc1_steps = 10;
		} else {
			agc1 = 6;
			agc1_steps = 4;
		}

		while (done < agc1_steps) {
			count		 = 0;
			steps_up	 = 0;
			steps_down	 = 0;
			done		+= 1;

			while ((count++) < 40) {
				ret = tda18272_rd(tda18272, TDA18272_AGC_DET_OUT, &tda18272->regs[TDA18272_AGC_DET_OUT]);
				if (ret)
					goto err;
				steps_down += (TDA18272_GETFIELD(AGC_DET_OUT_DO_AGC1, tda18272->regs[TDA18272_AGC_DET_OUT]) ? 14 : -1);
				steps_up += (TDA18272_GETFIELD(AGC_DET_OUT_UP_AGC1, tda18272->regs[TDA18272_AGC_DET_OUT]) ? 1 : -4);
				msleep(1);
			}
			if (steps_up >= 15 && (TDA18272_GETFIELD(AGC1_BYTE_2_AGC1_GAIN, tda18272->regs[TDA18272_AGC1_BYTE_2]) != 9)) {
				g1 = TDA18272_GETFIELD(AGC1_BYTE_2_AGC1_GAIN, tda18272->regs[TDA18272_AGC1_BYTE_2]) + 1;
				TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_2], AGC1_BYTE_2_AGC1_GAIN, g1);
				ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_2, tda18272->regs[TDA18272_AGC1_BYTE_2]);
				if (ret)
					goto err;
			} else if (steps_down >= 10 && TDA18272_GETFIELD(AGC1_BYTE_2_AGC1_GAIN, tda18272->regs[TDA18272_AGC1_BYTE_2]) != agc1) {
				g1 = TDA18272_GETFIELD(AGC1_BYTE_2_AGC1_GAIN, tda18272->regs[TDA18272_AGC1_BYTE_2]) - 1;
				TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_2], AGC1_BYTE_2_AGC1_GAIN, g1);
				ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_2, tda18272->regs[TDA18272_AGC1_BYTE_2]);
				if (ret)
					goto err;
			} else {
				done = agc1_steps;
			}
		}
	} else {
		TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_2], AGC1_BYTE_2_FORCE_AGC1_GAIN, 0x00);
		ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_2, tda18272->regs[TDA18272_AGC1_BYTE_2]);
		if (ret)
			goto err;

		TDA18272_SETFIELD(tda18272->regs[TDA18272_AGC1_BYTE_2], AGC1_BYTE_2_AGC1_LOOP_OFF, 0x00);
		ret = tda18272_wr(tda18272, TDA18272_AGC1_BYTE_2, tda18272->regs[TDA18272_AGC1_BYTE_2]);
		if (ret)
			goto err;
	}
err:
	dprintk(FE_DEBUG, 1, "ret=%d", ret);
	return ret;
}

static int tda18272_get_status(struct dvb_frontend *fe, u32 *status)
{
	struct tda18272_state *tda18272 = fe->tuner_priv;
	int ret = 0;

	*status = 0;

	ret = tda18272_wr(tda18272, TDA18272_THERMO_BYTE_2, 0x01);
	if (ret)
		goto err;

	ret = tda18272_rd(tda18272, TDA18272_THERMO_BYTE_1, &tda18272->regs[TDA18272_THERMO_BYTE_1]);
	if (ret)
		goto err;

	ret = tda18272_rd_regs(tda18272, TDA18272_POWERSTATE_BYTE_1, &tda18272->regs[TDA18272_POWERSTATE_BYTE_1], 3);
	if (ret)
		goto err;

	if (TDA18272_GETFIELD(POWERSTATE_BYTE_1_LO_LOCK, tda18272->regs[TDA18272_POWERSTATE_BYTE_1])) {
		dprintk(FE_ERROR, 1, "PLL Locked");
		*status |= 0x01;
	}
	if ((tda18272->regs[TDA18272_POWERSTATE_BYTE_2] >> 1) == 0)
		dprintk(FE_ERROR, 1, "Normal MODE");
	if ((tda18272->regs[TDA18272_POWERSTATE_BYTE_2] >> 1) == 7)
		dprintk(FE_ERROR, 1, "Standby MODE, LNA=ON, PLL=OFF");
	if ((tda18272->regs[TDA18272_POWERSTATE_BYTE_2] >> 1) == 6)
		dprintk(FE_ERROR, 1, "Standby MODE, LNA=ON, PLL=OFF");
	if ((tda18272->regs[TDA18272_POWERSTATE_BYTE_2] >> 1) == 4)
		dprintk(FE_ERROR, 1, "Standby MODE, LNA=ON, PLL=ON");

	dprintk(FE_ERROR, 1, "Junction Temperature:%d Power level:%d",
		tda18272->regs[TDA18272_THERMO_BYTE_1],
		tda18272->regs[TDA18272_INPUT_POWERLEVEL]);

err:
	dprintk(FE_DEBUG, 1, "ret=%d", ret);
	return ret;
}
#if 0
static int tda18272_set_state(struct dvb_frontend *fe, enum tuner_param param, struct tuner_state *state)
{
	return -EINVAL;
}

static int tda18272_get_state(struct dvb_frontend *fe, enum tuner_param param, struct tuner_state *state)
{
	struct tda18272_state *tda18272		= fe->tuner_priv;
	const struct tda18272_coeff *coe	= tda18272->coe;
	int ret;

	switch (param) {
	case DVBFE_TUNER_FREQUENCY:
		state->frequency = tda18272->frequency;
		ret = 0;
		break;
	case DVBFE_TUNER_TUNERSTEP:
		state->tunerstep = fe->ops.tuner_ops.info.frequency_step;
		ret = 0;
		break;
	case DVBFE_TUNER_IFFREQ:
		state->ifreq = coe->if_val;
		ret = 0;
		break;
	case DVBFE_TUNER_BANDWIDTH:
		if (fe->ops.info.type == FE_OFDM)
			state->bandwidth = tda18272->bandwidth;
		ret = 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
#endif
static int tda18272_set_params(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct tda18272_state *tda18272 = fe->tuner_priv;
	struct tda18272_coeff *coe	= NULL;
	u32 status;
	u32 delsys = c->delivery_system;
	u32 bw = c->bandwidth_hz;
	u32 freq = c->frequency;
	int ret;

	BUG_ON(!tda18272);

	dprintk(FE_DEBUG, 1, "freq=%d, bw=%d", freq, bw);
	switch (delsys) {
	case SYS_ATSC:
		coe = coeft + TDA18272_ATSC_6MHz;
		break;
	case SYS_DVBT:
	case SYS_DVBT2:
		switch (bw) {
		case 6000000:
			coe = coeft + TDA18272_DVBT_6MHz;
			break;
		case 7000000:
			coe = coeft + TDA18272_DVBT_7MHz;
			break;
		case 8000000:
			coe = coeft + TDA18272_DVBT_8MHz;
			break;
		default:
			coe = NULL;
			ret = -EINVAL;
			goto err;
		}
		break;
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		coe = coeft + TDA18272_QAM_8MHz;
		break;
	case SYS_DVBC_ANNEX_B:
		coe = coeft + TDA18272_QAM_6MHz;
		break;
	}
	BUG_ON(!coe);
	tda18272->coe = coe;
	dprintk(FE_DEBUG, 1, "Loading %s coeffecients...", coe->desc);
	ret = tda18272_set_frequency(tda18272, freq);
	if (ret)
		goto err;
	msleep(100);
	ret = tda18272_get_status(fe, &status);
	if (ret)
		goto err;

	if (status == 0x01) {
		tda18272->frequency = freq;
		if (fe->ops.info.type == FE_OFDM)
			tda18272->bandwidth = bw;
	}
err:
	dprintk(FE_DEBUG, 1, "ret=%d", ret);
	return ret;
}

static int tda18272_get_ifreq(struct dvb_frontend *fe, u32 *frequency)
{
	struct tda18272_state *tda18272	= fe->tuner_priv;
	const struct tda18272_coeff *coe = tda18272->coe;

	*frequency = coe->if_val;
	return 0;
}

static int tda18272_release(struct dvb_frontend *fe)
{
	struct tda18272_state *tda18272 = fe->tuner_priv;

	BUG_ON(!tda18272);
	fe->tuner_priv = NULL;
	kfree(tda18272);
	return 0;
}

static struct dvb_tuner_ops tda18272_ops = {
	.info = {
		.name		= "TDA18272 Silicon Tuner",
		.frequency_min  =  42000000,
		.frequency_max  = 870000000,
		.frequency_step	= 50000,
	},
	.init			= tda18272_init,
	.get_status		= tda18272_get_status,
	.set_params		= tda18272_set_params,
	//.set_state		= tda18272_set_state,
	//.get_state		= tda18272_get_state,
	.get_frequency		= tda18272_get_ifreq,
	.release		= tda18272_release
};


#define TDA18272_CHIP_ID	18272
#define TDA18272_MAJOR_REV	1
#define TDA18272_MINOR_REV	1

struct dvb_frontend *tda18272_attach(struct dvb_frontend *fe,
				     struct i2c_adapter *i2c,
				     const struct tda18272_config *config)
{
	struct tda18272_state *tda18272;
	u8 major = 0, minor = 0, mode = 0;
	int id = 0, ret;

	tda18272 = kzalloc(sizeof (struct tda18272_state), GFP_KERNEL);
	if (!tda18272)
		goto err;

	BUG_ON(!i2c);
	BUG_ON(!config);

	tda18272->i2c		= i2c;
	tda18272->config	= config;
	tda18272->fe		= fe;

	fe->tuner_priv		= tda18272;
	fe->ops.tuner_ops	= tda18272_ops;

	ret = tda18272_rd_regs(tda18272, TDA18272_ID_BYTE_1, &tda18272->regs[TDA18272_ID_BYTE_1], 3);
	if (ret)
		goto err;

	id    = (TDA18272_GETFIELD(ID_BYTE_1_IDENT, tda18272->regs[TDA18272_ID_BYTE_1]) << 8) |
		 TDA18272_GETFIELD(ID_BYTE_2_IDENT, tda18272->regs[TDA18272_ID_BYTE_2]);

	major = TDA18272_GETFIELD(ID_BYTE_3_MAJOR_REV, tda18272->regs[TDA18272_ID_BYTE_3]);
	minor = TDA18272_GETFIELD(ID_BYTE_3_MINOR_REV, tda18272->regs[TDA18272_ID_BYTE_3]);
	mode  = TDA18272_GETFIELD(ID_BYTE_1_MASTER_SLAVE, tda18272->regs[TDA18272_ID_BYTE_1]);

	if (id == TDA18272_CHIP_ID) {
		dprintk(FE_ERROR, 1, "Found TDA%d %s Rev:%d.%d", id, mode ? "Master" : "Slave", major, minor);
		if ((major != TDA18272_MAJOR_REV) || (minor != TDA18272_MINOR_REV))
			dprintk(FE_ERROR, 1, "Unknown Version:%d.%d, trying anyway ..", major, minor);

		tda18272->mode	  = mode;
		if (config->mode == TDA18272_SLAVE && tda18272->mode == 1)
			dprintk(FE_ERROR, 1, "Config as TDA18272 Slave, but TDA18272 Master found ???");

		if (config->mode == TDA18272_MASTER)
			tda18272->ms = 1;
		else
			tda18272->ms = 0;

		tda18272->lna_top = 0;
		tda18272->psm_agc = 1;
		tda18272->agc1    = 0;

		ret = tda18272_init(fe);
		if (ret) {
			dprintk(FE_ERROR, 1, "Error Initializing!");
			goto err1;
		}

		dprintk(FE_DEBUG, 1, "Done");
		return tda18272->fe;
	}
err:
	dprintk(FE_ERROR, 1, "TDA18272 not found!, ID=0x%02x exiting..", id);
err1:
	kfree(tda18272);
	return NULL;
}
EXPORT_SYMBOL(tda18272_attach);

MODULE_AUTHOR("Manu Abraham");
MODULE_DESCRIPTION("TDA18272 Silicon tuner");
MODULE_LICENSE("GPL");
