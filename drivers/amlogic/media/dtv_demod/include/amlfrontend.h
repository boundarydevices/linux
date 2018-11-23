/*
 * drivers/amlogic/media/dtv_demod/include/amlfrontend.h
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

#ifndef _AMLFRONTEND_H
#define _AMLFRONTEND_H
/**/
#include "depend.h" /**/

#define KERNEL_4_9_EN		1

struct amlfe_config {
	int fe_mode;
	int i2c_id;
	int tuner_type;
	int tuner_addr;
};
enum Gxtv_Demod_Tuner_If {
	Si2176_5M_If = 5,
	Si2176_6M_If = 6
};
/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC */
enum Gxtv_Demod_Dvb_Mode {
	Gxtv_Dvbc = 0,
	Gxtv_Dvbt_Isdbt = 1,
	Gxtv_Atsc = 2,
	Gxtv_Dtmb = 3,
};
#define Adc_Clk_35M             35714	/* adc clk    dvbc */
#define Demod_Clk_71M   71428	/* demod clk */

#define Adc_Clk_24M             24000
#define Demod_Clk_72M       72000
#define Demod_Clk_60M       60000

#define Adc_Clk_28M             28571	/* dvbt,isdbt */
#define Demod_Clk_66M   66666

#define Adc_Clk_26M                     26000	/* atsc  air */
#define Demod_Clk_78M     78000	/*  */

#define Adc_Clk_25_2M                   25200	/* atsc  cable */
#define Demod_Clk_75M     75600	/*  */

#define Adc_Clk_25M                     25000	/* dtmb */
#define Demod_Clk_100M    100000	/*  */
#define Demod_Clk_180M    180000	/*  */
#define Demod_Clk_200M    200000	/*  */
#define Demod_Clk_225M    225000
#define Demod_Clk_250M    250000

#define Adc_Clk_27M                     27777	/* atsc */
#define Demod_Clk_83M     83333	/*  */

enum M6_Demod_Pll_Mode {
	Cry_mode = 0,
	Adc_mode = 1
};

/*
 * e: enum
 * s: system
 */
enum es_map_addr {
	ES_MAP_ADDR_DEMOD,
	ES_MAP_ADDR_IOHIU,
	ES_MAP_ADDR_AOBUS,
	ES_MAP_ADDR_RESET,
	ES_MAP_ADDR_NUM
};
struct ss_reg_phy {
	unsigned int phy_addr;
	unsigned int size;
	/*void __iomem *p;*/
	/*int flag;*/
};
struct ss_reg_vt {
	void __iomem *v;
	int flag;
};

/* rigister offset page */
#define IC_OFFS_V2	(0x02)	/*MG9TV, GXTVBB, TXL*/
#define IC_OFFS_V3	(0x03)	/*TXLX, GXLX, TXHD*/
#define IC_OFFS_V4	(0x04)	/*TL1*/



#define IC_MD_NONE	(0x00)

#define IC_DVBC_V2	(0x02)	/*MG9TV, GXTVBB, TXL*/
#define IC_DVBC_V3	(0x03)	/*TXLX, GXLX*/

#define IC_DTMB_V2	(0x02)	/*MG9TV, GXTVBB*/
#define IC_DTMB_V3	(0x03)	/*TXL, TXHD*/

#define IC_DVBT_V2	(0x02)	/*TXLX*/
#define IC_ATSC_V2	(0x02)	/*TXLX*/

/*-----------------------*/
#define IC_VER_GTVBB	(0x00)
#define IC_VER_TXL	(0x01)
#define IC_VER_TXLX	(0x02)
#define IC_VER_GXLX	(0x03)
#define IC_VER_TXHD	(0x04)
#define IC_VER_TL1	(0x05)

#define IC_VER_NUB	(0x06)


/*-----------------------*/
#if 0
struct ic_cfg_s {
	/* register */
#if 0
	unsigned int reg_demod_st;
	unsigned int reg_demod_size;
	unsigned int reg_iohiu_st;
	unsigned int reg_iohiu_size;
	unsigned int reg_aobus_st;	/*ao bus*/
	unsigned int reg_aobus_size;
	unsigned int reg_reset_st;	/*reset*/
	unsigned int reg_reset_size;
#endif

	/* module version */
	union {
		unsigned int all;
		struct {
			unsigned int atsc:4, dvbt:4, dtmb:4, dvbc:4,
				reserved:4, offset:4, ic:8;
		} b;
	} hwver;
};
#endif

struct ic_ver {
	unsigned int atsc:4, dvbt:4, dtmb:4, dvbc:4,
					reserved:4, offset:4, ic:8;

};
struct ddemod_reg_off {
	/* register address offset for demod*/
	unsigned int off_demod_top;
	unsigned int off_dvbc;
	unsigned int off_dtmb;
	unsigned int off_dvbt;
	unsigned int off_atsc;
	unsigned int off_front;
	unsigned int off_isdbt;

#if 0
	/*vertual address for dtv demod*/
	void __iomem *base_demod;
	void __iomem *base_iohiu;
	void __iomem *base_aobus;
	void __iomem *base_reset;
#else

#endif
};
struct meson_ddemod_data {
	const char *name;
	struct ic_ver icver;
	/*struct ddemod_reg_off regoff;*/
};
enum DTVDEMOD_ST {
	DTVDEMOD_ST_NOT_INI,	/*driver is not init or init failed*/
	DTVDEMOD_ST_IDLE,	/*leave mode*/
	DTVDEMOD_ST_WORK,	/*enter_mode*/
};

/*polling*/
struct poll_machie_s {
	unsigned int flg_stop;	/**/
	unsigned int flg_restart;

	unsigned int state;	/*idel, work,*/


	/**/
	unsigned int delayms;
	unsigned int flg_updelay;

	unsigned int crrcnt;
	unsigned int maxcnt;

	enum fe_status last_s;
	unsigned int bch;


};
struct amlfe_exp_config {
	/*config by aml_fe ?*/
	/* */
	int set_mode;
};

struct amldtvdemod_device_s {

	struct class *clsp;
	struct device *dev;
	enum DTVDEMOD_ST state;
	struct mutex lock;	/*aml_lock*/

	/*struct ic_cfg_s iccfg;*/
	struct ic_ver icver;
	struct ss_reg_phy reg_p[ES_MAP_ADDR_NUM];
	struct ss_reg_vt reg_v[ES_MAP_ADDR_NUM];

	struct ddemod_reg_off ireg;
	struct meson_ddemod_data *data;
	bool flg_cma_allc;
	bool act_dtmb;

	struct poll_machie_s poll_machie;

	unsigned int en_detect;
#ifdef KERNEL_4_9_EN
	/* clktree */
	unsigned int clk_gate_state;
	struct clk *vdac_clk_gate;
#endif
	/*agc pin mux*/
	struct pinctrl *pin;
	const char *pin_name;

#if 1 /*move to aml_dtv_demod*/
	/*for mem reserved*/
	int			mem_start;
	int			mem_end;
	int			mem_size;
	int			cma_flag;
#ifdef CONFIG_CMA
	struct platform_device	*this_pdev;
	struct page			*venc_pages;
	unsigned int			cma_mem_size;/* BYTE */
	unsigned int			cma_mem_alloc;
#endif

	/*for dtv spectrum*/
	int			spectrum;
	/*for atsc version*/
	int			atsc_version;
	/*for dtv priv*/
#endif
	enum aml_fe_n_mode_t n_mode;	/*temp for mode*/
	enum fe_delivery_system last_delsys;
#ifndef DVB_49
	struct dvb_frontend *frontend;	/**/
#else
	struct dvb_frontend frontend;	/**/
#endif
	const struct amlfe_exp_config *afe_cfg;
	struct dentry *demod_root;
};
extern struct amldtvdemod_device_s *dtvdd_devp;	/**/

/*int M6_Demod_Dtmb_Init(struct aml_fe_dev *dev);*/
int convert_snr(int in_snr);
extern int vdac_enable_check_dtv(void);


extern unsigned  int ats_thread_flg;

/*version*/
static inline int is_atsc_ver(unsigned int ver)
{
	return dtvdd_devp->icver.atsc == ver;
}
static inline int is_dvbt_ver(unsigned int ver)
{
	return dtvdd_devp->icver.dvbt == ver;
}
static inline int is_dtmb_ver(unsigned int ver)
{
	return dtvdd_devp->icver.dtmb == ver;
}
static inline int is_dvbc_ver(unsigned int ver)
{
	return dtvdd_devp->icver.dvbc == ver;
}

static inline int is_offset_ver(unsigned int ver)
{
	return dtvdd_devp->icver.offset == ver;
}
static inline int get_atsc_ver(void)
{
	return dtvdd_devp->icver.atsc;
}
static inline int get_dvbt_ver(void)
{
	return dtvdd_devp->icver.dvbt;
}

static inline int get_dtmb_ver(void)
{
	return dtvdd_devp->icver.dtmb;
}
static inline int get_dvbc_ver(void)
{
	return dtvdd_devp->icver.dvbc;
}

static inline int is_ic_ver(unsigned int ver)
{
	return dtvdd_devp->icver.ic == ver;
}
static inline int get_ic_ver(void)
{
	return dtvdd_devp->icver.ic;
}

#if 0
static inline void __iomem *gbase_dvbt(void)
{
	return dtvdd_devp->ireg.base_demod + dtvdd_devp->ireg.off_dvbt;
}
static inline void __iomem *gbase_dvbc(void)
{
	return dtvdd_devp->ireg.base_demod + dtvdd_devp->ireg.off_dvbc;
}
static inline void __iomem *gbase_dtmb(void)
{
	return dtvdd_devp->ireg.base_demod + dtvdd_devp->ireg.off_dtmb;
}

static inline void __iomem *gbase_atsc(void)
{
	return dtvdd_devp->ireg.base_demod + dtvdd_devp->ireg.off_atsc;
}
static inline void __iomem *gbase_demod(void)
{
	return dtvdd_devp->ireg.base_demod + dtvdd_devp->ireg.off_demod;
}

static inline void __iomem *gbase_aobus(void)
{
	return dtvdd_devp->ireg.base_aobus;
}

static inline void __iomem *gbase_iohiu(void)
{
	return dtvdd_devp->ireg.base_iohiu;
}
static inline void __iomem *gbase_reset(void)
{
	return dtvdd_devp->ireg.base_reset;
}

static inline unsigned int gphybase_demod(void)
{
	return dtvdd_devp->iccfg.reg_demod_st + dtvdd_devp->ireg.off_demod;
}
static inline unsigned int gphybase_hiu(void)
{
	return dtvdd_devp->iccfg.reg_iohiu_st;
}
#else
static inline void __iomem *gbase_dvbt(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v
					+ dtvdd_devp->ireg.off_dvbt;
}
static inline void __iomem *gbase_dvbc(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v
					+ dtvdd_devp->ireg.off_dvbc;
}
static inline void __iomem *gbase_dtmb(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v
		+ dtvdd_devp->ireg.off_dtmb;
}

static inline void __iomem *gbase_atsc(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v
		+ dtvdd_devp->ireg.off_atsc;
}
static inline void __iomem *gbase_demod(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v
		+ dtvdd_devp->ireg.off_demod_top;
}

static inline void __iomem *gbase_isdbt(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v
		+ dtvdd_devp->ireg.off_isdbt;
}

static inline void __iomem *gbase_front(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_DEMOD].v
		+ dtvdd_devp->ireg.off_front;
}

static inline void __iomem *gbase_aobus(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_AOBUS].v;
}

static inline void __iomem *gbase_iohiu(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_IOHIU].v;
}
static inline void __iomem *gbase_reset(void)
{
	return dtvdd_devp->reg_v[ES_MAP_ADDR_RESET].v;
}

static inline unsigned int gphybase_demod(void)
{
	return dtvdd_devp->reg_p[ES_MAP_ADDR_DEMOD].phy_addr;
}
static inline unsigned int gphybase_demodcfg(void)
{
	return dtvdd_devp->reg_p[ES_MAP_ADDR_DEMOD].phy_addr
			+ dtvdd_devp->ireg.off_demod_top;
}

static inline unsigned int gphybase_hiu(void)
{
	return dtvdd_devp->reg_p[ES_MAP_ADDR_IOHIU].phy_addr;
}

#endif
/*poll*/
extern void dtmb_poll_start(void);
extern void dtmb_poll_stop(void);
extern unsigned int dtmb_is_update_delay(void);
extern unsigned int dtmb_get_delay_clear(void);
extern unsigned int dtmb_is_have_check(void);
extern void dtmb_poll_v3(void);
#endif
