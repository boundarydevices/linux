/*
 * drivers/amlogic/media/dtv_demod/amlfrontend.c
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

/*****************************************************************
 **  author :
 **	     Shijie.Rong@amlogic.com
 **  version :
 **	v1.0	  12/3/13
 **	v2.0	 15/10/12
 **	v3.0	  17/11/15
 *****************************************************************/
#define __DVB_CORE__	/*ary 2018-1-31*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <linux/err.h>	/*IS_ERR*/
#include <linux/clk.h>	/*clk tree*/
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>

#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
/* #include <mach/am_regs.h> */
#endif
#include <linux/i2c.h>
#include <linux/gpio.h>


#include <linux/dvb/aml_demod.h>
#include "demod_func.h"

#include "depend.h" /**/

#include "amlfrontend.h"

/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>


MODULE_PARM_DESC(debug_aml, "\n\t\t Enable frontend debug information");
static int debug_aml = 1;
module_param(debug_aml, int, 0644);

MODULE_PARM_DESC(auto_search_std, "\n\t\t atsc-c std&hrc search");
static unsigned int auto_search_std;
module_param(auto_search_std, int, 0644);

MODULE_PARM_DESC(std_lock_timeout, "\n\t\t atsc-c std lock timeout");
static unsigned int std_lock_timeout = 1000;
module_param(std_lock_timeout, int, 0644);
/*0.001for field,0.002 for performance*/
static char *demod_version = "V0.03";


int aml_demod_debug = DBG_INFO;


#if 0
#define PR_DBG(fmt, args ...) \
	do { \
		if (aml_demod_debug & DBG_INFO) { \
			pr_info("dtv_dmd:"fmt, ##args); \
		} \
	} while (0)

#endif

/* debug info------------------------*/


module_param(aml_demod_debug, int, 0644);
MODULE_PARM_DESC(aml_demod_debug, "set debug level (info=bit1, reg=bit2, atsc=bit4,");

/*-----------------------------------*/
struct amldtvdemod_device_s *dtvdd_devp;

static int last_lock = -1;	/*debug only*/
#define DEMOD_DEVICE_NAME  "Amlogic_Demod"
static int cci_thread;
static int freq_dvbc;
static struct aml_demod_sta demod_status;
static enum fe_modulation atsc_mode = VSB_8;
static struct aml_demod_para para_demod;
static int atsc_flag;

static int memstart = 0x1ef00000;/* move to aml_dtv_demod*/

long *mem_buf;

MODULE_PARM_DESC(frontend_mode, "\n\t\t Frontend mode 0-DVBC, 1-DVBT");
static int frontend_mode = -1;
module_param(frontend_mode, int, 0444);

MODULE_PARM_DESC(frontend_i2c, "\n\t\t IIc adapter id of frontend");
static int frontend_i2c = -1;
module_param(frontend_i2c, int, 0444);

MODULE_PARM_DESC(frontend_tuner,
		 "\n\t\t Frontend tuner type 0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316");
static int frontend_tuner = -1;
module_param(frontend_tuner, int, 0444);

MODULE_PARM_DESC(frontend_tuner_addr, "\n\t\t Tuner IIC address of frontend");
static int frontend_tuner_addr = -1;
module_param(frontend_tuner_addr, int, 0444);

MODULE_PARM_DESC(demod_thread, "\n\t\t demod thread");
static int demod_thread = 1;
module_param(demod_thread, int, 0644);


static int dvb_tuner_delay = 100;
module_param(dvb_tuner_delay, int, 0644);
MODULE_PARM_DESC(dvb_atsc_count, "dvb_tuner_delay");
#define THRD_TUNER_STRENTH_ATSC (-87)
#define THRD_TUNER_STRENTH_J83 (-76)


static int autoflags, autoFlagsTrig;

/*static struct mutex aml_lock;move to dtvdd_devp->lock*/
const char *name_reg[] = {
	"demod",
	"iohiu",
	"aobus",
	"reset",
};
/*also see: IC_VER_NUB*/
const char *name_ic[] = {
	"gxtvbb",
	"txl",
	"txlx",
	"gxlx",
	"txhd",
	"tl1",
};

#define END_SYS_DELIVERY	19
const char *name_fe_delivery_system[] = {
	"UNDEFINED",
	"DVBC_ANNEX_A",
	"DVBC_ANNEX_B",
	"DVBT",
	"DSS",
	"DVBS",
	"DVBS2",
	"DVBH",
	"ISDBT",
	"ISDBS",
	"ISDBC",
	"ATSC",
	"ATSCMH",
	"DTMB",
	"CMMB",
	"DAB",
	"DVBT2",
	"TURBO",
	"DVBC_ANNEX_C",
	"ANALOG",	/*19*/
};
void dbg_delsys(unsigned char id)
{
	if (id <= END_SYS_DELIVERY)
		PR_INFO("%s:%s:\n", __func__, name_fe_delivery_system[id]);
	else
		PR_INFO("%s:%d\n", __func__, id);

}
static void dtvdemod_vdac_enable(bool on);
static void dtvdemod_set_agc_pinmux(int on);



static int Gxtv_Demod_Dvbc_Init(/*struct aml_fe_dev *dev, */int mode);
static ssize_t dvbc_auto_sym_show(struct class *cls,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "dvbc_autoflags: %s\n", autoflags ? "on" : "off");
}

static ssize_t dvbc_auto_sym_store(struct class *cls,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{

	return 0;
}

static unsigned int dtmb_mode;
static unsigned int atsc_mode_para;
static unsigned int demod_mode_para;


enum {
	DTMB_READ_STRENGTH = 0,
	DTMB_READ_SNR = 1,
	DTMB_READ_LOCK = 2,
	DTMB_READ_BCH = 3,
};

enum {
	ATSC_READ_STRENGTH = 0,
	ATSC_READ_SNR = 1,
	ATSC_READ_LOCK = 2,
	ATSC_READ_SER = 3,
	ATSC_READ_FREQ = 4,
};

enum {
	UNKNOWN = 0,
	AML_DVBC,
	AML_DTMB,
	AML_DVBT,
	AML_ATSC,
	AML_J83B,
	AML_ISDBT,
	AML_DVBT2
};





int convert_snr(int in_snr)
{
	int out_snr;
	static int calce_snr[40] = {
		5, 6, 8, 10, 13,
		16, 20, 25, 32, 40,
		50, 63, 80, 100, 126,
		159, 200, 252, 318, 400,
		504, 634, 798, 1005, 1265,
		1592, 2005, 2524, 3177, 4000,
		5036, 6340, 7981, 10048, 12649,
		15924, 20047, 25238, 31773, 40000};
	for (out_snr = 1 ; out_snr < 40; out_snr++)
		if (in_snr <= calce_snr[out_snr])
			break;

	return out_snr;
}

static int freq_p;
static ssize_t atsc_para_show(struct class *cls,
				  struct class_attribute *attr, char *buf)
{
	int snr, lock_status, ser;
	struct dvb_frontend *dvbfe;
	int strength = 0;

	if (atsc_mode != VSB_8)
		return 0;

	if (atsc_mode_para == ATSC_READ_STRENGTH) {
		dvbfe = aml_get_fe();/*get_si2177_tuner();*/
#if 0
		if (dvbfe != NULL) {
			if (dvbfe->ops.tuner_ops.get_strength) {
				strength =
				dvbfe->ops.tuner_ops.get_strength(dvbfe);
			}
			strength -= 100;
		}
#else
		strength = tuner_get_ch_power2();
#endif
		return sprintf(buf, "strength is %d\n", strength);
	} else if (atsc_mode_para == ATSC_READ_SNR) {
		snr = atsc_read_snr();
		return sprintf(buf, "snr is %d\n", snr);
	} else if (atsc_mode_para == ATSC_READ_LOCK) {
		lock_status =
			atsc_read_reg(0x0980);
		return sprintf(buf, "lock_status is %x\n", lock_status);
	} else if (atsc_mode_para == ATSC_READ_SER) {
		ser = atsc_read_ser();
		return sprintf(buf, "ser is %d\n", ser);
	} else if (atsc_mode_para == ATSC_READ_FREQ) {
		return sprintf(buf, "freq is %d\n", freq_p);
	} else {
		return sprintf(buf, "atsc_para_show can't match mode\n");
	}
}

static ssize_t atsc_para_store(struct class *cls,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	if (buf[0] == '0')
		atsc_mode_para = ATSC_READ_STRENGTH;
	else if (buf[0] == '1')
		atsc_mode_para = ATSC_READ_SNR;
	else if (buf[0] == '2')
		atsc_mode_para = ATSC_READ_LOCK;
	else if (buf[0] == '3')
		atsc_mode_para = ATSC_READ_SER;
	else if (buf[0] == '4')
		atsc_mode_para = ATSC_READ_FREQ;

	return count;
}

static ssize_t demod_para_store(struct class *cls,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	if (buf[0] == '0')
		demod_mode_para = UNKNOWN;
	else if (buf[0] == '1')
		demod_mode_para = AML_DVBC;
	else if (buf[0] == '2')
		demod_mode_para = AML_DTMB;
	else if (buf[0] == '3')
		demod_mode_para = AML_DVBT;
	else if (buf[0] == '4')
		demod_mode_para = AML_ATSC;
	else if (buf[0] == '5')
		demod_mode_para = AML_J83B;
	else if (buf[0] == '6')
		demod_mode_para = AML_ISDBT;
	else if (buf[0] == '7')
		demod_mode_para = AML_DVBT2;

	return count;
}

void store_dvbc_qam_mode(int qam_mode, int symbolrate)
{
	int qam_para;

	switch (qam_mode) {
	case QAM_16:
		qam_para = 4;
		break;
	case QAM_32:
		qam_para = 5;
		break;
	case QAM_64:
		qam_para = 6;
		break;
	case QAM_128:
		qam_para = 7;
		break;
	case QAM_256:
		qam_para = 8;
		break;
	case QAM_AUTO:
		qam_para = 6;
		break;
	default:
		qam_para = 6;
		break;
	}
	para_demod.dvbc_qam = qam_para;
	para_demod.dvbc_symbol = symbolrate;
	PR_DBG("dvbc_qam is %d, symbolrate is %d\n",
		para_demod.dvbc_qam, para_demod.dvbc_symbol);
}

static ssize_t demod_para_show(struct class *cls,
				  struct class_attribute *attr, char *buf)
{
	int rate, symolrate, qam, coderate, a;

	if (demod_mode_para == AML_DVBC) {
		symolrate = para_demod.dvbc_symbol;
		qam = para_demod.dvbc_qam;
		rate = 944*symolrate*qam;
		rate /= 1000;
	} else if (demod_mode_para == AML_DTMB) {
		coderate = para_demod.dtmb_coderate;
		qam = para_demod.dtmb_qam;
		a = 4725;
		rate = 25614*qam*coderate/a;
		rate *= 1024;
	} else if (demod_mode_para == AML_DVBT) {
		rate = 19855;
	} else if (demod_mode_para == AML_ATSC) {
		if (atsc_mode == VSB_8)
			rate = 19855;
		else if (atsc_mode == QAM_64)
			rate = 27617;
		else if (atsc_mode == QAM_256)
			rate = 39741;
		else
			rate = 19855;
	} else {
		return sprintf(buf, "can't match mode\n");
	}
	return sprintf(buf, "rate %d\n", rate);
}





static ssize_t dtmb_para_show(struct class *cls,
				  struct class_attribute *attr, char *buf)
{
	int snr, lock_status, bch, agc_if_gain[3];
	struct dvb_frontend *dvbfe;
	int strength = 0;

	if (dtmb_mode == DTMB_READ_STRENGTH) {
		dvbfe = aml_get_fe();/*get_si2177_tuner();*/

#if 0
		if (dvbfe != NULL)
			if (dvbfe->ops.tuner_ops.get_strength) {
				strength =
				dvbfe->ops.tuner_ops.get_strength(dvbfe);
			}
#else
		strength = tuner_get_ch_power2();
#endif
		if (strength <= -56) {
			dtmb_read_agc(DTMB_D9_IF_GAIN, &agc_if_gain[0]);
			strength = dtmb_get_power_strength(agc_if_gain[0]);
		}
		return sprintf(buf, "strength is %d\n", strength);
	} else if (dtmb_mode == DTMB_READ_SNR) {
		/*snr = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) & 0x3fff;*/
		snr = dtmb_reg_r_che_snr();
		snr = convert_snr(snr);
		return sprintf(buf, "snr is %d\n", snr);
	} else if (dtmb_mode == DTMB_READ_LOCK) {
		lock_status = dtmb_reg_r_fec_lock();
		return sprintf(buf, "lock_status is %d\n", lock_status);
	} else if (dtmb_mode == DTMB_READ_BCH) {
		bch = dtmb_reg_r_bch();
		return sprintf(buf, "bch is %d\n", bch);
	} else {
		return sprintf(buf, "dtmb_para_show can't match mode\n");
	}
}


static ssize_t dtmb_para_store(struct class *cls,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	if (buf[0] == '0')
		dtmb_mode = DTMB_READ_STRENGTH;
	else if (buf[0] == '1')
		dtmb_mode = DTMB_READ_SNR;
	else if (buf[0] == '2')
		dtmb_mode = DTMB_READ_LOCK;
	else if (buf[0] == '3')
		dtmb_mode = DTMB_READ_BCH;

	return count;
}

static int readregdata;

static ssize_t dvbc_reg_show(struct class *cls, struct class_attribute *attr,
			     char *buf)
{
/*      int readregaddr=0;*/
	char *pbuf = buf;

	pbuf += sprintf(pbuf, "%x", readregdata);

	PR_INFO("read dvbc_reg\n");
	return pbuf - buf;
}

static ssize_t dvbc_reg_store(struct class *cls, struct class_attribute *attr,
			      const char *buf, size_t count)
{
	return 0;
}


static ssize_t info_show(struct class *cls,
				  struct class_attribute *attr, char *buf)
{
	int pos = 0;
	unsigned int size = PAGE_SIZE;

	int snr, lock_status, bch, agc_if_gain[3], ser;
	struct dvb_frontend *dvbfe;
	int strength = 0;

	pos += snprintf(buf+pos, size-pos, "dtv demod info:\n");

	switch (demod_mode_para) {
	case AML_DVBC:
		pos += snprintf(buf+pos, size-pos, "mode:AML_DVBC\n");
		break;

	case AML_DTMB:
		pos += snprintf(buf+pos, size-pos, "mode:AML_DTMB\n");

		/* DTMB_READ_STRENGTH */
		dvbfe = aml_get_fe();/*get_si2177_tuner();*/
#if 0
		if (dvbfe != NULL)
			if (dvbfe->ops.tuner_ops.get_strength) {
				strength =
				dvbfe->ops.tuner_ops.get_strength(dvbfe);
			}
#else
		strength = tuner_get_ch_power2();
#endif
		if (strength <= -56) {
			dtmb_read_agc(DTMB_D9_IF_GAIN, &agc_if_gain[0]);
			strength = dtmb_get_power_strength(agc_if_gain[0]);
		}
		pos += snprintf(buf+pos, size-pos,
					"strength: %d\n", strength);

		/* DTMB_READ_SNR */
		/*snr = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) & 0x3fff;*/
		snr = dtmb_reg_r_che_snr();
		snr = convert_snr(snr);
		pos += snprintf(buf+pos, size-pos, "snr: %d\n", snr);

		/* DTMB_READ_LOCK */
		/*lock_status = */
		/*	(dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) >> 14) & 0x1;*/
		lock_status = dtmb_reg_r_fec_lock();
		pos += snprintf(buf+pos, size-pos, "lock: %d\n", lock_status);

		/* DTMB_READ_BCH */
		bch = dtmb_reg_r_bch();

		pos += snprintf(buf+pos, size-pos, "bch: %d\n", bch);

		break;
	case AML_DVBT:
		pos += snprintf(buf+pos, size-pos, "mode:AML_DVBT\n");
		break;
	case AML_ATSC:
		pos += snprintf(buf+pos, size-pos, "mode:AML_ATSC\n");
		if (atsc_mode != VSB_8)
			return pos;


		/* ATSC_READ_STRENGTH */
		dvbfe = aml_get_fe();/*get_si2177_tuner();*/
		if (dvbfe != NULL) {
#if 0
			if (dvbfe->ops.tuner_ops.get_strength) {
				strength =
				dvbfe->ops.tuner_ops.get_strength(dvbfe);
			}
#else
			strength = tuner_get_ch_power2();
#endif
			/*strength -= 100;*/
		}
		pos += snprintf(buf+pos, size-pos, "strength: %d\n", strength);


		/* ATSC_READ_SNR */
		snr = atsc_read_snr();
		pos += snprintf(buf+pos, size-pos, "snr: %d\n", snr);

		/* ATSC_READ_LOCK */
		lock_status = atsc_read_reg(0x0980);
		pos += snprintf(buf+pos, size-pos, "lock: %d\n", lock_status);

		/* ATSC_READ_SER */
		ser = atsc_read_ser();
		pos += snprintf(buf+pos, size-pos, "ser: %d\n", ser);

		/* ATSC_READ_FREQ */
		pos += snprintf(buf+pos, size-pos, "freq: %d\n", freq_p);

		break;
	case AML_J83B:
		pos += snprintf(buf+pos, size-pos, "mode:AML_J83B\n");
		break;
	case AML_ISDBT:
		pos += snprintf(buf+pos, size-pos, "mode:AML_ISDBT\n");
		break;
	case AML_DVBT2:
		pos += snprintf(buf+pos, size-pos, "mode:AML_DVBT2\n");
		break;
	default:
		pos += snprintf(buf+pos, size-pos, "mode:Unknown\n");
		break;
	}

	return pos;
}


static CLASS_ATTR(auto_sym, 0644, dvbc_auto_sym_show, dvbc_auto_sym_store);
static CLASS_ATTR(dtmb_para, 0644, dtmb_para_show, dtmb_para_store);
/*from 666 to 644*/
static CLASS_ATTR(dvbc_reg, 0644, dvbc_reg_show, dvbc_reg_store);
static CLASS_ATTR(atsc_para, 0644, atsc_para_show, atsc_para_store);
static CLASS_ATTR(demod_rate, 0644, demod_para_show, demod_para_store);
/* DebugInfo */
static CLASS_ATTR(info, 0444, info_show, NULL);


/*static void dtvdemod_version(struct aml_fe_dev *dev)*/
static void dtvdemod_version(struct amldtvdemod_device_s *dev)

{
	char soc_version[20];
	char *atsc_version = "1";
	int ic_v = get_ic_ver();

	if (dev->atsc_version == 1)
		atsc_version = "1";
	else if (dev->atsc_version == 2)
		atsc_version = "2";
	else
		atsc_version = "1";
	atsc_set_version(dev->atsc_version);

	if (ic_v < IC_VER_NUB && strlen(name_ic[ic_v]) < 10) {
		strcpy(soc_version, name_ic[ic_v]);
		strcat(soc_version, "-");
	} else {
		strcpy(soc_version, "other-");
	}
	if (strlen(demod_version) < 8)
		strcat(soc_version, demod_version);
	strcat(soc_version, atsc_version);
	PR_INFO("[dtvdemod_version] [%s]\n", soc_version);
}


static int amdemod_qam(enum fe_modulation qam)
{
	switch (qam) {
	case QAM_16:
		return 0;
	case QAM_32:
		return 1;
	case QAM_64:
		return 2;
	case QAM_128:
		return 3;
	case QAM_256:
		return 4;
	case VSB_8:
		return 5;
	case QAM_AUTO:
		return 6;
	default:
		return 2;
	}
	return 2;
}

static int amdemod_stat_islock(/*struct aml_fe_dev *dev,*/ int mode)
{
	struct aml_demod_sts demod_sts;
	int lock_status;
	int dvbt_status1;
	int atsc_fsm;
	int ret = 0;

	if (mode == 0) {
		/*DVBC*/

		demod_sts.ch_sts = dvbc_get_ch_sts();
		return demod_sts.ch_sts & 0x1;
	} else if (mode == 1) {
		/*DVBT*/
		dvbt_status1 =
			((dvbt_read_reg((0x0a << 2)) >> 20) & 0x3ff);
		lock_status = (dvbt_read_reg((0x2a << 2))) & 0xf;
		if ((((lock_status) == 9) || ((lock_status) == 10))
		    && ((dvbt_status1) != 0))
			return 1;
		else
			return 0;
		/*((apb_read_reg(DVBT_BASE+0x0)>>12)&0x1);// */
		/* dvbt_get_status_ops()->get_status(&demod_sts, &demod_sta);*/
	} else if (mode == 2) {

	} else if (mode == 3) {
		/*ATSC*/
		if ((atsc_mode == QAM_64) || (atsc_mode == QAM_256)) {
			/*return (atsc_read_iqr_reg() >> 16) == 0x1f;*/
			if ((atsc_read_iqr_reg() >> 16) == 0x1f)
				ret = 1;
		} else if (atsc_mode == VSB_8) {
			if (is_ic_ver(IC_VER_TL1)) {
				if (atsc_read_reg_v4(0x2e) >= 0x76)
					ret = 1;
			} else {
			atsc_fsm = atsc_read_reg(0x0980);
			PR_DBGL("atsc status [%x]\n", atsc_fsm);
			/*return atsc_read_reg(0x0980) >= 0x79;*/
			if (atsc_read_reg(0x0980) >= 0x79)
				ret = 1;
			}
		} else {
			atsc_fsm = atsc_read_reg(0x0980);
			PR_DBGL("atsc status [%x]\n", atsc_fsm);
			/*return atsc_read_reg(0x0980) >= 0x79;*/
			if (atsc_read_reg(0x0980) >= 0x79)
				ret = 1;
		}

		return ret;
	} else if (mode == 4) {
		/*DTMB*/

		/*return (dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) >> 14) & 0x1;*/
		return dtmb_reg_r_fec_lock();
	}
	return 0;
}

#define amdemod_dvbc_stat_islock()  amdemod_stat_islock(0)
#define amdemod_dvbt_stat_islock()  amdemod_stat_islock(1)
#define amdemod_isdbt_stat_islock()  amdemod_stat_islock(2)
#define amdemod_atsc_stat_islock()  amdemod_stat_islock(3)
#define amdemod_dtmb_stat_islock()  amdemod_stat_islock(4)

#if 0
/*this function is not use for txlx*/
static int gxtv_demod_dvbc_set_qam_mode(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dvbc param;    /*mode 0:16, 1:32, 2:64, 3:128, 4:256*/

	memset(&param, 0, sizeof(param));
	param.mode = amdemod_qam(c->modulation);
	dvbc_set_qam_mode(param.mode);
	return 0;
}
static int aml_dtvdm_set_qam_mode(struct dvb_frontend *fe)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;
	int ret = 0;

	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		ret = gxtv_demod_dvbc_set_qam_mode(fe);
		break;
	case AM_FE_OFDM_N:

		break;
	case AM_FE_ATSC_N:
		ret = gxtv_demod_atsc_set_qam_mode(fe);
		break;
	case AM_FE_DTMB_N:

		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}

	return ret;
}
static int gxtv_demod_atsc_set_qam_mode(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_atsc param;    /*mode  3:64,  5:256, 7:vsb*/
	enum fe_modulation mode;

	memset(&param, 0, sizeof(param));
	mode = c->modulation;
	/*pr_dbg("mode is %d\n", mode);*/
	PR_ATSC("mode is %d\n", mode);
	atsc_qam_set(mode);
	return 0;
}

#endif
static void gxtv_demod_dvbc_release(struct dvb_frontend *fe)
{

}
#if 0
static int gxtv_demod_dvbc_read_status
	(struct dvb_frontend *fe, enum fe_status *status)
{
/*      struct aml_fe_dev *dev = afe->dtv_demod;*/
	struct aml_demod_sts demod_sts;
	struct aml_demod_sta demod_sta;
	/*struct aml_demod_i2c demod_i2c;*/
	int ilock;

	/*demod_sts.ch_sts = qam_read_reg(0x6);*/
	demod_sts.ch_sts = dvbc_get_ch_sts();
	dvbc_status(&demod_sta, /*&demod_i2c,*/ &demod_sts);
	if (demod_sts.ch_sts & 0x1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}
	if (last_lock != ilock) {
		PR_DBG("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}
#endif
struct timer_t {
	int enable;
	unsigned int start;
	unsigned int max;
};
static struct timer_t gtimer[4];
enum ddemod_timer_s {
	D_TIMER_DETECT,
	D_TIMER_SET,
	D_TIMER_DBG1,
	D_TIMER_DBG2,
};
int timer_set_max(enum ddemod_timer_s tmid, unsigned int max_val)
{
	gtimer[tmid].max = max_val;
	return 0;
}
int timer_begain(enum ddemod_timer_s tmid)
{
	gtimer[tmid].start = jiffies_to_msecs(jiffies);
	gtimer[tmid].enable = 1;

	PR_DBG("st %d=%d\n", tmid, (int)gtimer[tmid].start);
	return 0;
}
int timer_disable(enum ddemod_timer_s tmid)
{

	gtimer[tmid].enable = 0;

	return 0;
}

int timer_is_en(enum ddemod_timer_s tmid)
{
	return gtimer[tmid].enable;
}

int timer_not_enough(enum ddemod_timer_s tmid)
{
	int ret = 0;
	unsigned int time;

	if (gtimer[tmid].enable) {
		time = jiffies_to_msecs(jiffies);
		if ((time - gtimer[tmid].start) < gtimer[tmid].max) {
			PR_DBG("now=%d\n", (int)time);
			ret = 1;
		}
	}
	return ret;
}
int timer_is_enough(enum ddemod_timer_s tmid)
{
	int ret = 0;
	unsigned int time;

	/*Signal stability takes 200ms */
	if (gtimer[tmid].enable) {
		time = jiffies_to_msecs(jiffies);
		if ((time - gtimer[tmid].start) >= gtimer[tmid].max) {
			PR_DBG("now=%d\n", (int)time);
			ret = 1;
		}
	}
	return ret;
}

int timer_tuner_not_enough(void)
{
	int ret = 0;
	unsigned int time;
	enum ddemod_timer_s tmid;

	tmid = D_TIMER_DETECT;

	/*Signal stability takes 200ms */
	if (gtimer[tmid].enable) {
		time = jiffies_to_msecs(jiffies);
		if ((time - gtimer[tmid].start) < 200) {
			PR_DBG("nowt=%d\n", (int)time);
			ret = 1;
		}
	}
	return ret;
}




static int gxtv_demod_dvbc_read_status_timer
	(struct dvb_frontend *fe, enum fe_status *status)
{
	struct aml_demod_sts demod_sts;
	struct aml_demod_sta demod_sta;
	int strenth;

	int ilock;

	/*check tuner*/
	if (!timer_tuner_not_enough()) {
		strenth = tuner_get_ch_power2();
		if (strenth < -87) {
			*status = FE_TIMEDOUT;
			return 0;
		}
	}
	/*demod_sts.ch_sts = qam_read_reg(0x6);*/
	demod_sts.ch_sts = dvbc_get_ch_sts();
	dvbc_status(&demod_sta, /*&demod_i2c,*/ &demod_sts);
	if (demod_sts.ch_sts & 0x1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;

		if (timer_not_enough(D_TIMER_DETECT)) {
			*status = 0;
			PR_DBG("s=0\n");
		} else {
			*status = FE_TIMEDOUT;
			timer_disable(D_TIMER_DETECT);
		}
	}
	if (last_lock != ilock) {
		PR_DBG("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int gxtv_demod_dvbc_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	/*struct aml_fe_dev *dev = afe->dtv_demod;*/
	struct aml_demod_sts demod_sts;
	/*struct aml_demod_i2c demod_i2c;*/
	struct aml_demod_sta demod_sta;

	dvbc_status(&demod_sta, /*&demod_i2c,*/ &demod_sts);
	*ber = demod_sts.ch_ber;
	return 0;
}

static int gxtv_demod_dvbc_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{


#if 0
	if (fe->ops.tuner_ops.get_strength)
		tn_strength = fe->ops.tuner_ops.get_strength(fe);
	*strength = 256 - tn_strength;
#else
	*strength = tuner_get_ch_power3();
#endif

	return 0;
}

static int gxtv_demod_dvbc_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct aml_demod_sts demod_sts;
	/*struct aml_demod_i2c demod_i2c;*/
	struct aml_demod_sta demod_sta;

	dvbc_status(&demod_sta, /*&demod_i2c,*/ &demod_sts);
	*snr = demod_sts.ch_snr / 100;
	return 0;
}

static int gxtv_demod_dvbc_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

/*extern int aml_fe_analog_set_frontend(struct dvb_frontend *fe);*/

static int gxtv_demod_dvbc_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dvbc param;    /*mode 0:16, 1:32, 2:64, 3:128, 4:256*/
	struct aml_demod_sts demod_sts;

	PR_INFO("%s\n", __func__);
	/*timer_set_max(D_TIMER_DETECT, 4000);*/
	/*timer_begain(D_TIMER_DETECT);*/

	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;
	param.mode = amdemod_qam(c->modulation);
	param.symb_rate = c->symbol_rate / 1000;
	store_dvbc_qam_mode(c->modulation, param.symb_rate);

	if (!is_ic_ver(IC_VER_TL1)) {
		if ((param.mode == 3) && (demod_status.tmp != Adc_mode)) {
			Gxtv_Demod_Dvbc_Init(/*dev,*/ Adc_mode);
			/*pr_dbg("Gxtv_Demod_Dvbc_Init,Adc_mode\n");*/
		} else {
			/*Gxtv_Demod_Dvbc_Init(dev,Cry_mode);*/
		}
	}


	if (autoflags == 0) {
		/*pr_dbg("QAM_TUNING mode\n");*/
		/*flag=0;*/
	}
	if ((autoflags == 1) && (autoFlagsTrig == 0)
	    && (freq_dvbc == param.ch_freq)) {
		PR_DBG("now is auto symbrating\n");
		return 0;
	}
	autoFlagsTrig = 0;
	last_lock = -1;
	PR_DBG("[gxtv_demod_dvbc_set_frontend]PARA\t"
	       "param.ch_freq is %d||||param.symb_rate is %d,\t"
	       "param.mode is %d\n",
	       param.ch_freq, param.symb_rate, param.mode);
	tuner_set_params(fe);/*aml_fe_analog_set_frontend(fe);*/
	dvbc_set_ch(&demod_status, /*&demod_i2c,*/ &param);
	/*0xf33 dvbc mode, 0x10f33 j.83b mode*/
	#if 0
	if (is_meson_txlx_cpu() || is_meson_gxlx_cpu())
		/*qam_write_reg(0x7, 0xf33);*/
		dvbc_init_reg_ext();
	#endif
	if (is_dvbc_ver(IC_DVBC_V3))
		dvbc_init_reg_ext();

	if (autoflags == 1) {
		PR_DBG("QAM_PLAYING mode,start auto sym\n");
		dvbc_set_auto_symtrack();
		/*      flag=1;*/
	}
	dvbc_status(&demod_status, /*&demod_i2c,*/ &demod_sts);
	freq_dvbc = param.ch_freq;

	PR_DBG("AML amldemod => frequency=%d,symbol_rate=%d\r\n", c->frequency,
	       c->symbol_rate);
	return 0;
}

static int gxtv_demod_dvbc_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	u32 qam_mode;


	qam_mode = dvbc_get_qam_mode();
	c->modulation = qam_mode + 1;
	PR_DBG("[mode] is %d\n", c->modulation);

	return 0;
}

static int Gxtv_Demod_Dvbc_Init(/*struct aml_fe_dev *dev, */int mode)
{
	struct aml_demod_sys sys;
	/*struct aml_demod_i2c i2c;*/

	PR_DBG("%s\n", __func__);
	memset(&sys, 0, sizeof(sys));
	/*memset(&i2c, 0, sizeof(i2c));*/
	/*i2c.tuner = dev->drv->id;*/
	/*i2c.addr = dev->i2c_addr;*/
	/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC*/
	demod_status.dvb_mode = Gxtv_Dvbc;

	if (mode == Adc_mode) {
		sys.adc_clk = Adc_Clk_25M;
		sys.demod_clk = Demod_Clk_200M;
		demod_status.tmp = Adc_mode;
	} else {
		sys.adc_clk = Adc_Clk_24M;
		sys.demod_clk = Demod_Clk_72M;
		demod_status.tmp = Cry_mode;
	}

	if (is_ic_ver(IC_VER_TL1)) {
		sys.adc_clk = Adc_Clk_24M;
		sys.demod_clk = Demod_Clk_167M;
		demod_status.tmp = Cry_mode;
	}

	demod_status.ch_if = Si2176_5M_If * 1000;
	PR_DBG("[%s]adc_clk is %d,demod_clk is %d\n", __func__, sys.adc_clk,
	       sys.demod_clk);
	autoFlagsTrig = 0;
	/*demod_set_sys(&demod_status, &i2c, &sys);*/
	demod_set_sys(&demod_status, &sys);
	demod_mode_para = AML_DVBC;
	return 0;
}

#if 0
/*TL1*/
void Gxtv_Demod_Dvbc_v4_Init(void)
{
	//struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dvbc dvbc;
	int nco_rate;
	int tmp;

	//printf("frequency point is(KHz):");
	//scanf("%d", &tmp);
	//dvbc.ch_freq = tmp;
	//dvbc.ch_freq = 474000;
	dvbc.ch_freq = 474000;
	//dvbc.mode = tmp;
	dvbc.mode = 4;// 0=16QAM, 1=32QAM, 2 = 64QAM, 3 = 128QAM, 4 = 256QAM
	//dvbc.symb_rate = tmp;
	dvbc.symb_rate = 5360;//6875;5056(J.83b 64QAM),5360(J.83b 256QAM)
	//ioctl(fd, AML_DEMOD_DVBC_SET_CH, &dvbc);
	dvbc.dat0 = 24000;
	nco_rate = (24*256)/250+2;

	//app_apb_write_reg(0xf00*4,0x88);
	demod_write_reg(DEMOD_TOP_REG0, 0x88);
	//app_apb_write_reg(0xf08*4,0x201);
	demod_write_reg(DEMOD_TOP_REG8, 0x201);
	//app_apb_write_reg(0xf0c*4,0x11); //open write enable
	demod_write_reg(DEMOD_TOP_REGC, 0x11);
	//app_apb_write_reg(0xe20,
		//((app_apb_read_reg(0xe20) &~ 0xff) | (nco_rate & 0xff)));
	//app_apb_write_reg(0xe20,	(app_apb_read_reg(0xe20) | (1 << 8)));
	front_write_reg_v4(0x20,
		((front_read_reg_v4(0x20) & ~0xff) | (nco_rate & 0xff)));
	front_write_reg_v4(0x20,	(front_read_reg_v4(0x20) | (1 << 8)));

	//dvbc_reg_initial_tmp(&dvbc);
	dvbc_reg_initial_tmp_v4(&dvbc);

	//printf("set J.83B(1) OR DVBC(2)");
	tmp = 2;
	if (tmp == 1) {
		//app_apb_write_reg(reg_reset,0x10f33);
		qam_write_reg(0x7, 0x10f33);
		//set_j83b_filter_reg();
		set_j83b_filter_reg_v4();
	} else {
		//app_apb_write_reg(reg_reset,0xf33);
		qam_write_reg(0x7, 0xf33);
	}

	#if 0
	printf("select  0~3 mode:");
	scanf("%d", &tmp);
	if (tmp == 0) {
	//set_dvbc_reg_1();
	set_dvbc_reg_1_v4();
	//set_dvbc_reg_2();
	set_dvbc_reg_2_v4();
	} else if (tmp == 1) {
	//set_dvbc_reg_1();
	set_dvbc_reg_1_v4();
	//set_j83b_reg_2();
	set_j83b_reg_2_v4();
	} else if (tmp == 2) {
	//set_j83b_reg_1();
	set_j83b_reg_1_v4();
	//set_dvbc_reg_2();
	set_dvbc_reg_2_v4();
	} else if (tmp == 3) {
	//set_j83b_reg_1();
	set_j83b_reg_1_v4();
	//set_j83b_reg_2();
	set_j83b_reg_2_v4();
	}
	#endif

	#if 0
	app_apb_write_reg(0x48, 0x50e1000);
	app_apb_write_reg(0xc0, 0x41f2f69);
	#else
	qam_write_reg(0x12, 0x50e1000);
	qam_write_reg(0x30, 0x41f2f69);
	#endif
}

int uart_error_confirm(int a_0, int a_1)
{
	int flag_out_of_range;
	int flag_error;
	int flag_zero;
	int flag_sta_check;
	int st[2];
	int out;

	st[0] = a_0&0xf;
	st[1] = (a_0>>4)&0xf;
	if (st[1] == 0)
		flag_sta_check = ((st[0] == 1) || (st[0] == 2)) ? 0 : 1;
	if (st[1] == 1)
		flag_sta_check = ((st[0] == 2) || (st[0] == 3)) ? 0 : 1;
	if (st[1] == 2)
		flag_sta_check = (st[0] == 3) ? 0 : 1;
	if (st[1] == 3)
		flag_sta_check = (st[0] == 4) ? 0 : 1;
	if (st[1] == 4)
		flag_sta_check = (st[0] == 6) ? 0 : 1;
	if (st[1] == 6)
		flag_sta_check = ((st[0] == 4) || (st[0] == 5)) ? 0 : 1;

	flag_zero = (a_0 == 0) ? 1 : 0;
	flag_out_of_range = ((a_0 & 0xf) > 6) ? 1 : 0;
	flag_error = (((a_0 >> 4) & 0x0fffffff) == (a_1 & 0x0fffffff)) ? 0 : 1;
	out = ((flag_out_of_range == 1) || (flag_error == 1)
		|| (flag_zero == 1)) ? 1 : 0;
	return out;
}

void print_dvbc_result(void)
{
	//int timer1,
	int time;
	int i, tmp;
	//float ftmp;
	//struct aml_demod_sts  sts;
	int status[2], uart_error;
	int record = 6, cnt = 0, cur_res = 0;

	//timer1 = read_time_usecond();
	status[0] = status[1] = 0;
	time = 1;
	i = 0;
	while (i < time) {
		status[0] = qam_read_reg(0x31);
		//if((status[0]&0xf) == 5){
		//    timer2=read_time_usecond();
		//    printf("\n\n\n\n[sync timer]%d\n",timer2-timer1);
		//    break;
		//}
		uart_error = uart_error_confirm(status[0], status[1]);
		usleep_range(10000, 10001);
		status[1] = status[0];
		i = i + 1;

		//lg add state cnt
		cur_res = status[1] & 0xf;
		if (cur_res != 6) {
			record = cur_res;
		} else {
			if (record != cur_res)
				cnt++;
			record = cur_res;
		}

		// ioctl(fd, AML_DEMOD_DVBC_GET_CH, &sts);
		//tmp = sts.ch_sts;
		PR_INFO("sta %08x  ulock_n %d  ", qam_read_reg(0x6), cnt);
		//ftmp = sts.ch_snr;
		//ftmp /= 100.0;
		//ftmp=(app_apb_read_reg(0x174)&0xfff0)/16/32;
		//ftmp=(dvbc_read_reg(0x174)&0xfff0)/16/32;
		//PR_INFO("snr %5.2f dB ", ftmp);
		PR_INFO("eq status is %d ", qam_read_reg(0x5d)&0xf);
		PR_INFO("npn_det  %d impdet  %d imp_n  %d ",
			qam_read_reg(0x58)>>20&0x1, qam_read_reg(0x58)>>16&0x3,
			qam_read_reg(0x58)&0xffff);
		//  ftmp = sts.ch_ber;
		//  ftmp /= 1e6;
		//  printf("ber %.2e ", ftmp);
		//	if(ber_avg == 0)
		//		ber_avg = ftmp;
		//	ber_avg = ber_avg + (ftmp-ber_avg)/32;
		//	printf("beravg %.2e ", ber_avg);
		tmp = (qam_read_reg(0x33)&0xffff);
		//tmp = sts.ch_per;
		PR_INFO("per %d ", tmp);
		//ftmp=(dvbc_read_reg(0x34)&0xffff)/1e3;
		//      ftmp = sts.symb_rate;
		//PR_INFO("srate %.3f ", ftmp);
		//ftmp = (dvbc_read_reg(0x28)&0x7fffff)/1e3;
		//PR_INFO("freqoff %.3f kHz ", ftmp);
		tmp = (qam_read_reg(0x27));
		//PR_INFO("power %ddb,gain %.3d  ",
			//((tmp>>22) & 0x1ff)/16.0, tmp & 0x7ff);
		PR_INFO("lock sta %x\n", qam_read_reg(0x6));
	}
}
#endif

static void gxtv_demod_dvbt_release(struct dvb_frontend *fe)
{

}

static int gxtv_demod_dvbt_read_status
	(struct dvb_frontend *fe, enum fe_status *status)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
	/*struct aml_demod_i2c demod_i2c;*/
	struct aml_demod_sta demod_sta;
	int ilock;
	unsigned char s = 0;

	s = dvbt_get_status_ops()->get_status(&demod_sta/*, &demod_i2c*/);
	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		if (timer_not_enough(D_TIMER_DETECT)) {
			ilock = 0;
			*status = 0;
			PR_INFO("timer not enough\n");

		} else {
			ilock = 0;
			*status = FE_TIMEDOUT;
			timer_disable(D_TIMER_DETECT);
		}
	}
	if (last_lock != ilock) {
		PR_INFO("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int gxtv_demod_dvbt_read_ber(struct dvb_frontend *fe, u32 *ber)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
	/*struct aml_demod_i2c demod_i2c;*/
	struct aml_demod_sta demod_sta;

	*ber = dvbt_get_status_ops()->get_ber(&demod_sta) & 0xffff;
	return 0;
}

static int gxtv_demod_dvbt_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{
	/*struct aml_fe *afe = fe->demodulator_priv;*/
	/*struct aml_fe_dev *dev = afe->dtv_demod;*/

	/**strength = 256 - tuner_get_ch_power(fe);*/

	*strength = tuner_get_ch_power3();

	PR_DBGL("[RSJ]tuner strength is %d dbm\n", *strength);
	return 0;
}

static int gxtv_demod_dvbt_read_snr(struct dvb_frontend *fe, u16 *snr)
{
/*      struct aml_fe *afe = fe->demodulator_priv;*/
/*      struct aml_demod_sts demod_sts;*/
/*	struct aml_demod_i2c demod_i2c;*/
	struct aml_demod_sta demod_sta;

	*snr = dvbt_get_status_ops()->get_snr(&demod_sta/*, &demod_i2c*/);
	*snr /= 8;
	PR_DBGL("[RSJ]snr is %d dbm\n", *snr);
	return 0;
}

static int gxtv_demod_dvbt_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}
int convert_bandwidth(unsigned int input)
{
	/*int output;*/
	enum fe_bandwidth output;

	output = 3;
	switch (input) {
	case 10000000:
		output = BANDWIDTH_10_MHZ;
		break;
	case 8000000:
		output = BANDWIDTH_8_MHZ;
		break;
	case 7000000:
		output = BANDWIDTH_7_MHZ;
		break;
	case 6000000:
		output = BANDWIDTH_6_MHZ;
		break;
	case 5000000:
		output = BANDWIDTH_5_MHZ;
		break;
	case 1712000:
		output = BANDWIDTH_1_712_MHZ;
		break;
	case 0:
		output = BANDWIDTH_AUTO;
		break;
	}
	return output;


}

static int gxtv_demod_dvbt_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	/*struct aml_demod_sts demod_sts;*/
	struct aml_demod_dvbt param;




	/*////////////////////////////////////*/
	/* bw == 0 : 8M*/
	/*       1 : 7M*/
	/*       2 : 6M*/
	/*       3 : 5M*/
	/* agc_mode == 0: single AGC*/
	/*             1: dual AGC*/
	/*////////////////////////////////////*/
	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;

	//param.bw = c->bandwidth_hz;
	param.bw = convert_bandwidth(c->bandwidth_hz);
	PR_INFO("%s:bw=%d\n", __func__, c->bandwidth_hz);
	param.agc_mode = 1;
	/*ISDBT or DVBT : 0 is QAM, 1 is DVBT, 2 is ISDBT,*/
	/* 3 is DTMB, 4 is ATSC */
	param.dat0 = 1;
	last_lock = -1;

	tuner_set_params(fe);/*aml_fe_analog_set_frontend(fe);*/
	dvbt_set_ch(&demod_status, /*&demod_i2c,*/ &param);


	/*pr_dbg("AML amldemod => frequency=%d,symbol_rate=%d\r\n",*/
	/* p->frequency,p->u.qam.symbol_rate);*/
	return 0;
}

static int gxtv_demod_dvbt_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/

	return 0;
}

/*int Gxtv_Demod_Dvbt_Init(struct aml_fe_dev *dev)*/
int Gxtv_Demod_Dvbt_Init(void)
{
	struct aml_demod_sys sys;

	PR_DBG("AML Demod DVB-T init\r\n");

	memset(&sys, 0, sizeof(sys));

	memset(&demod_status, 0, sizeof(demod_status));

	/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC*/
	demod_status.dvb_mode = Gxtv_Dvbt_Isdbt;
	sys.adc_clk = Adc_Clk_24M;
	sys.demod_clk = Demod_Clk_60M;
	demod_status.ch_if = Si2176_5M_If * 1000;

	demod_set_sys(&demod_status, &sys);
	demod_mode_para = AML_DVBT;
	return 0;
}

static void gxtv_demod_atsc_release(struct dvb_frontend *fe)
{

}


static int gxtv_demod_atsc_get_frontend_algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_HW;
}
static int gxtv_demod_txlx_get_frontend_algo(struct dvb_frontend *fe)
{

	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	int ret = DVBFE_ALGO_HW;

	switch (nmode) {
	case AM_FE_QPSK_N:
		break;

	case AM_FE_QAM_N:
		/*dvbc*/
		ret = DVBFE_ALGO_HW;
		break;

	case AM_FE_OFDM_N:
	case AM_FE_ISDBT_N:
		/*dvbt*/
		ret = DVBFE_ALGO_HW;

		break;
	case AM_FE_ATSC_N:
		ret = DVBFE_ALGO_HW;
		break;
	case AM_FE_DTMB_N:
		ret = DVBFE_ALGO_HW;
		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}

	return ret;

}

unsigned  int ats_thread_flg;
static int gxtv_demod_atsc_read_status
	(struct dvb_frontend *fe, enum fe_status *status)
{

	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_sts demod_sts;
	/*struct aml_demod_i2c demod_i2c;*/
	struct aml_demod_sta demod_sta;
	int ilock;
	/*struct dvb_frontend *dvbfe;*/
	unsigned char s = 0;
	int strength = 0;

	/*debug only*/
	static enum fe_status dbg_lst_status;	/* last status */


	if (!demod_thread) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
		return 0;
	}
	if (!get_dtvpll_init_flag())
		return 0;

	if ((c->modulation <= QAM_AUTO) && (c->modulation != QPSK)
		&& (atsc_flag == QAM_AUTO)) {
		s = amdemod_dvbc_stat_islock();
		dvbc_status(&demod_sta,/* &demod_i2c, */&demod_sts);
	} else if ((c->modulation > QAM_AUTO)
		&& (atsc_flag == VSB_8)) {
		/*atsc_thread();*/
		s = amdemod_atsc_stat_islock();

		if (!is_ic_ver(IC_VER_TL1)) {
			if ((s == 0) && (last_lock == 1)
				&& (atsc_read_reg(0x0980) >= 0x76)) {
				s = 1;
				PR_ATSC("[rsj] unlock,but fsm >= 0x76\n");
			}
		}
	}
#if 0	/*debug only move to end*/
	dvbfe = get_si2177_tuner();
	if (dvbfe != NULL)
		if (dvbfe->ops.tuner_ops.get_strength) {
			strength =
			dvbfe->ops.tuner_ops.get_strength(dvbfe);
		}
	/*strength -= 100;*/
	pr_dbg("[rsj_test]freq[%d] strength[%d]\n", freq_p, strength);
#endif
	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		if (is_ic_ver(IC_VER_TL1)) {
			if (timer_not_enough(D_TIMER_DETECT)) {
				*status = 0;
				PR_DBG("s=0\n");
			} else {
				*status = FE_TIMEDOUT;
				timer_disable(D_TIMER_DETECT);
			}
		} else {
			*status = FE_TIMEDOUT;
		}
		#if 0
		if (ats_thread_flg)
			*status = FE_TIMEDOUT;
		else
			*status = 0;
		#endif

	}
#if 1	/*debug only*/
	if (last_lock != ilock) {
		PR_INFO("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}
#endif
	/*debug only*/
	if (aml_demod_debug & DBG_ATSC) {
		if ((dbg_lst_status != s) || (last_lock != ilock)) {
			/* check tuner */
			strength = tuner_get_ch_power2();

			PR_ATSC("s=%d(1 is lock),lock=%d\n", s, ilock);
			PR_ATSC("[rsj_test]freq[%d] strength[%d]\n",
				freq_p, strength);

			/*update */
			dbg_lst_status = s;
			last_lock = ilock;
		}
		/*aml_dbgatscl(".");*/
	}
	return 0;
}

static int gxtv_demod_atsc_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	if (!get_dtvpll_init_flag())
		return 0;
	if ((c->modulation > QAM_AUTO)
		&& (atsc_flag == VSB_8))
		*ber = atsc_read_reg(0x980)&0xffff;
	else if (((c->modulation == QAM_256)
		|| (c->modulation == QAM_64))
		&& (atsc_flag == QAM_AUTO))

		*ber = dvbc_get_status();
	return 0;
}

static int gxtv_demod_atsc_read_signal_strength
	(struct dvb_frontend *fe, u16 *strength)
{
/*	struct aml_fe *afe = fe->demodulator_priv;*/
	/*struct aml_fe_dev *dev = afe->dtv_demod; */


	*strength = tuner_get_ch_power3();
	#if 0
	if (*strength < 0)
		*strength = 0;
	else if (*strength > 100)
		*strength = 100;
	#endif
	if (*strength > 100)
		*strength = 100;

	return 0;
}

static int gxtv_demod_atsc_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_sts demod_sts;
	/*struct aml_demod_i2c demod_i2c;*/
	struct aml_demod_sta demod_sta;

	if ((c->modulation <= QAM_AUTO)
		&& (c->modulation != QPSK)
		&& (atsc_flag == QAM_AUTO)) {
		dvbc_status(&demod_sta, /*&demod_i2c, */&demod_sts);
		*snr = demod_sts.ch_snr / 100;
	} else if ((c->modulation > QAM_AUTO)
		&& (atsc_flag == VSB_8))
		*snr = atsc_read_snr();
	return 0;
}

static int gxtv_demod_atsc_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	if (!demod_thread)
		return 0;
	if ((c->modulation > QAM_AUTO)
		&& (atsc_flag == VSB_8))
		atsc_thread();
	*ucblocks = 0;
	return 0;
}

static int gxtv_demod_atsc_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_atsc param_atsc;
	struct aml_demod_dvbc param_j83b;
	int temp_freq = 0;
	union ATSC_DEMOD_REG_0X6A_BITS Val_0x6a;
	union ATSC_CNTR_REG_0X20_BITS Val_0x20;
	int nco_rate;

	memset(&param_atsc, 0, sizeof(param_atsc));
	memset(&param_j83b, 0, sizeof(param_j83b));
	if (!demod_thread)
		return 0;
	freq_p = c->frequency / 1000;
	PR_INFO("c->modulation is %d,freq_p is %d, atsc_flag is %d\n",
		c->modulation, freq_p, atsc_flag);
	last_lock = -1;
	atsc_mode = c->modulation;
	/* param.mode = amdemod_qam(p->u.vsb.modulation);*/
	tuner_set_params(fe);/*aml_fe_analog_set_frontend(fe);*/
	if ((c->modulation <= QAM_AUTO) && (c->modulation != QPSK)) {
		if (atsc_flag != QAM_AUTO)
			atsc_flag = QAM_AUTO;
		/* demod_set_demod_reg(0x502, TXLX_ADC_REG6);*/
		if (!is_ic_ver(IC_VER_TL1))
			dd_tvafe_hiu_reg_write(D_HHI_DEMOD_CLK_CNTL, 0x502);

		demod_set_mode_ts(Gxtv_Dvbc);
		param_j83b.ch_freq = c->frequency / 1000;
		param_j83b.mode = amdemod_qam(c->modulation);
		if (c->modulation == QAM_64)
			param_j83b.symb_rate = 5057;
		else if (c->modulation == QAM_256)
			param_j83b.symb_rate = 5361;
		else
			param_j83b.symb_rate = 5361;

		if (is_ic_ver(IC_VER_TL1)) {
			//for timeshift mosaic
			demod_status.clk_freq = Demod_Clk_167M;
			nco_rate = (demod_status.adc_freq * 256)
				/ demod_status.clk_freq + 2;
			front_write_reg_v4(0x20,
				((front_read_reg_v4(0x20) & ~0xff)
				| (nco_rate & 0xff)));
			front_write_reg_v4(0x2f, 0x5);//for timeshift mosaic
			dd_tvafe_hiu_reg_write(0x1d0, 0x502);//sys_clk=167M
		}

		dvbc_set_ch(&demod_status, /*&demod_i2c, */&param_j83b);

		if (is_ic_ver(IC_VER_TL1)) {
			qam_write_reg(0x7, 0x10f33);
			set_j83b_filter_reg_v4();
			qam_write_reg(0x12, 0x50e1000);
			qam_write_reg(0x30, 0x41f2f69);
			//for timeshift mosaic issue
			//qam_write_reg(0x84, 0x2190000);
		}

	} else if (c->modulation > QAM_AUTO) {
		if (is_ic_ver(IC_VER_TL1)) {
			//demod_set_sys_atsc_v4();
			Val_0x6a.bits = atsc_read_reg_v4(ATSC_DEMOD_REG_0X6A);
			Val_0x6a.b.peak_thd = 0x6;//Let CCFO Quality over 6
			atsc_write_reg_v4(ATSC_DEMOD_REG_0X6A, Val_0x6a.bits);
			atsc_write_reg_v4(ATSC_EQ_REG_0XA5, 0x8c);

			if (demod_status.adc_freq == Adc_Clk_24M) {
				atsc_write_reg_v4(ATSC_DEMOD_REG_0X54,
					0x1aaaaa);

				atsc_write_reg_v4(ATSC_DEMOD_REG_0X55,
					0x3ae28d);

				atsc_write_reg_v4(ATSC_DEMOD_REG_0X6E,
					0x16e3600);
			}

			atsc_write_reg_v4(0x12, 0x18);//for timeshift mosaic
			Val_0x20.bits = atsc_read_reg_v4(ATSC_CNTR_REG_0X20);
			Val_0x20.b.cpu_rst = 1;
			atsc_write_reg_v4(ATSC_CNTR_REG_0X20, Val_0x20.bits);
			Val_0x20.b.cpu_rst = 0;
			atsc_write_reg_v4(ATSC_CNTR_REG_0X20, Val_0x20.bits);
			usleep_range(5000, 5001);
		} else {
			if (atsc_flag != VSB_8)
				atsc_flag = VSB_8;
			/*demod_set_demod_reg(0x507, TXLX_ADC_REG6);*/
			dd_tvafe_hiu_reg_write(D_HHI_DEMOD_CLK_CNTL, 0x507);
			demod_set_mode_ts(Gxtv_Atsc);
			param_atsc.ch_freq = c->frequency / 1000;
			param_atsc.mode = c->modulation;
			atsc_set_ch(&demod_status, /*&demod_i2c,*/ &param_atsc);
		}
	}

	if (is_ic_ver(IC_VER_TL1))
		return 0;

	if ((auto_search_std == 1) && ((c->modulation <= QAM_AUTO)
	&& (c->modulation != QPSK))) {
		unsigned char s = 0;

		msleep(std_lock_timeout);
		s = amdemod_dvbc_stat_islock();
		if (s == 1) {
			PR_DBG("atsc std mode is %d locked\n", atsc_mode);

			return 0;
		}
		if ((c->frequency == 79000000) || (c->frequency == 85000000)) {
			temp_freq = (c->frequency + 2000000) / 1000;
			param_j83b.ch_freq = temp_freq;
			PR_DBG("irc fre:%d\n", param_j83b.ch_freq);
			c->frequency = param_j83b.ch_freq * 1000;


			tuner_set_params(fe);
			demod_set_mode_ts(Gxtv_Dvbc);
			param_j83b.mode = amdemod_qam(c->modulation);
			if (c->modulation == QAM_64)
				param_j83b.symb_rate = 5057;
			else if (c->modulation == QAM_256)
				param_j83b.symb_rate = 5361;
			else
				param_j83b.symb_rate = 5361;
			dvbc_set_ch(&demod_status, /*&demod_i2c,*/
			&param_j83b);

			msleep(std_lock_timeout);
			s = amdemod_dvbc_stat_islock();
			if (s == 1) {
				PR_DBG("irc mode is %d locked\n", atsc_mode);
			} else {
				temp_freq = (c->frequency - 1250000) / 1000;
				param_j83b.ch_freq = temp_freq;
				PR_DBG("hrc fre:%d\n", param_j83b.ch_freq);
				c->frequency = param_j83b.ch_freq * 1000;

				tuner_set_params(fe);
				demod_set_mode_ts(Gxtv_Dvbc);
				param_j83b.mode = amdemod_qam(c->modulation);
				if (c->modulation == QAM_64)
					param_j83b.symb_rate = 5057;
				else if (c->modulation == QAM_256)
					param_j83b.symb_rate = 5361;
				else
					param_j83b.symb_rate = 5361;
				dvbc_set_ch(&demod_status, /*&demod_i2c,*/
				&param_j83b);
			}
		} else {
			param_j83b.ch_freq = (c->frequency - 1250000) / 1000;
			PR_DBG("hrc fre:%d\n", param_j83b.ch_freq);
			c->frequency = param_j83b.ch_freq * 1000;

			demod_set_mode_ts(Gxtv_Dvbc);
			param_j83b.mode = amdemod_qam(c->modulation);
			if (c->modulation == QAM_64)
				param_j83b.symb_rate = 5057;
			else if (c->modulation == QAM_256)
				param_j83b.symb_rate = 5361;
			else
				param_j83b.symb_rate = 5361;
			dvbc_set_ch(&demod_status, /*&demod_i2c,*/
			&param_j83b);
		}
	}
	PR_DBG("atsc_mode is %d\n", atsc_mode);
	/*pr_dbg("AML amldemod => frequency=%d,symbol_rate=%d\r\n",*/
	/* p->frequency,p->u.qam.symbol_rate);*/
	return 0;
}

static int gxtv_demod_atsc_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	PR_DBG("c->frequency is %d\n", c->frequency);
	return 0;
}

void atsc_detect_first(struct dvb_frontend *fe, enum fe_status *status)
{
	unsigned int ucblocks;
	unsigned int atsc_status;
	enum fe_status s;
	int strenth;
	int cnt;
	int check_ok;

	/*tuner strength*/
	if (dvb_tuner_delay > 9)
		msleep(dvb_tuner_delay);

	strenth = tuner_get_ch_power(fe);
	if (strenth < THRD_TUNER_STRENTH_ATSC) {
		*status = FE_TIMEDOUT;
		PR_ATSC("tuner:no signal!\n");
		return;
	}
	#define CNT_FIRST_ATSC  (2)
	check_ok = 0;

	for (cnt = 0; cnt < CNT_FIRST_ATSC; cnt++) {
		if (!is_ic_ver(IC_VER_TL1))
			gxtv_demod_atsc_read_ucblocks(fe, &ucblocks);

		gxtv_demod_atsc_read_status(fe, &s);

		if (is_ic_ver(IC_VER_TL1)) {
			*status = s;
			break;
		}

		//	*status = s;

		if (s != 0x1f) {
			gxtv_demod_atsc_read_ber(fe, &atsc_status);
			if ((atsc_status < 0x60)) {
				*status = FE_TIMEDOUT;
				check_ok = 1;
			}
		} else {
			check_ok = 1;
			*status = s;
		}

		if (check_ok)
			break;

	}
	PR_ATSC("%s,detect=0x%x,cnt=%d\n", __func__,
			(unsigned int)*status, cnt);
}


static int dvb_j83b_count = 5;
module_param(dvb_j83b_count, int, 0644);
MODULE_PARM_DESC(dvb_atsc_count, "dvb_j83b_count");
/*come from j83b_speedup_func*/

static int atsc_j83b_detect_first(struct dvb_frontend *fe, enum fe_status *s)
{
	int j83b_status, i;
	/*struct dvb_frontend_private *fepriv = fe->frontend_priv;*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int strenth;
	enum fe_status cs;
	int cnt;
	int check_ok;

	/*tuner:*/
	if (dvb_tuner_delay > 9)
		msleep(dvb_tuner_delay);

	strenth = tuner_get_ch_power(fe);
	if (strenth < THRD_TUNER_STRENTH_J83) {
		*s = FE_TIMEDOUT;
		PR_ATSC("tuner:no signal!j83\n");
		return 0;
	}
	check_ok = 0;

	/*first check signal max time*/
	#define CNT_FIRST  (10)

	for (cnt = 0; cnt < CNT_FIRST; cnt++) {
		gxtv_demod_atsc_read_status(fe, &cs);

		if (is_ic_ver(IC_VER_TL1)) {
			*s = cs;
			return 0;
		}

		if (cs != 0x1f) {
			/*msleep(200);*/
			PR_DBG("[j.83b] 1\n");
			for (i = 0; i < dvb_j83b_count; i++) {
				msleep(25);
				gxtv_demod_atsc_read_ber(fe, &j83b_status);

				/*J.83 status >=0x38,has signal*/
				if (j83b_status >= 0x3)
					break;
			}
			PR_DBG("[rsj]j.83b_status is %x,modulation is %d\n",
					j83b_status,
					c->modulation);

			if (j83b_status < 0x3) {
				*s = FE_TIMEDOUT;
				check_ok = 1;
			}

		} else {
			/*have signal*/
			*s = cs;
			check_ok = 1;
		}

		if (check_ok)
			break;

		msleep(50);
	}


	if (!check_ok)
		*s = FE_TIMEDOUT;

	PR_ATSC("j83 first:cnt:%d,sgn:0x%x\n", cnt, *s);
	return 0;
}
static int atsc_j83b_polling(struct dvb_frontend *fe, enum fe_status *s)
{
	int j83b_status, i;
	/*struct dvb_frontend_private *fepriv = fe->frontend_priv;*/
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int strenth;

	PR_DBG("+7+");


	strenth = tuner_get_ch_power(fe);
	if (strenth < THRD_TUNER_STRENTH_J83) {
		*s = FE_TIMEDOUT;
		PR_DBGL("tuner:no signal!j83\n");
		return 0;
	}

	gxtv_demod_atsc_read_status(fe, s);

	if (*s != 0x1f) {
		/*msleep(200);*/
		PR_DBG("[j.83b] 1\n");
		for (i = 0; i < dvb_j83b_count; i++) {
			msleep(25);
			gxtv_demod_atsc_read_ber(fe, &j83b_status);

			/*J.83 status >=0x38,has signal*/
			if (j83b_status >= 0x3)
				break;
		}
		PR_DBG("[rsj]j.83b_status is %x,modulation is %d\n",
				j83b_status,
				c->modulation);

		if (j83b_status < 0x3)
			*s = FE_TIMEDOUT;
	}




	return 0;
}

void atsc_polling(struct dvb_frontend *fe, enum fe_status *status)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	if (c->modulation == QPSK) {
		PR_DBG("mode is qpsk, return;\n");
		/*return;*/
	} else if (c->modulation <= QAM_AUTO) {
		atsc_j83b_polling(fe, status);
	} else {
		atsc_thread();
		gxtv_demod_atsc_read_status(fe, status);
	}



}

static int gxtv_demod_atsc_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	/*
	 * It is safe to discard "params" here, as the DVB core will sync
	 * fe->dtv_property_cache with fepriv->parameters_in, where the
	 * DVBv3 params are stored. The only practical usage for it indicate
	 * that re-tuning is needed, e. g. (fepriv->state & FESTATE_RETUNE) is
	 * true.
	 */
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	/*int ret = 0;*/

	PR_ATSC("%s:\n", __func__);
	*delay = HZ/2;
	if (re_tune) {
		dtvdd_devp->en_detect = 1; /*fist set*/
		/*c->delivery_system = aml_demod_delivery_sys;*/
	/*PR_ATSC("delivery_system=%d\n", aml_demod_delivery_sys);*/
		gxtv_demod_atsc_set_frontend(fe);

		if (is_ic_ver(IC_VER_TL1))
			timer_begain(D_TIMER_DETECT);

		if (c->modulation ==  QPSK) {
			PR_ATSC("modulation is QPSK do nothing!");
		} else if (c->modulation <= QAM_AUTO) {
			PR_ATSC("j83\n");
			atsc_j83b_detect_first(fe, status);
		} else if (c->modulation > QAM_AUTO) {
			atsc_detect_first(fe, status);
		}

		return 0;
	}
#if 1
	if (!dtvdd_devp->en_detect) {
		PR_DBGL("tune:not enable\n");
		return 0;
	}
#endif
	if (is_ic_ver(IC_VER_TL1)) {
		if (c->modulation > QAM_AUTO)
			atsc_detect_first(fe, status);
		else if (c->modulation <= QAM_AUTO &&
			(c->modulation !=  QPSK))
			atsc_j83b_detect_first(fe, status);
	} else {
		atsc_polling(fe, status);
	}

	return 0;

}

static int gxtv_demod_dvbt_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	/*
	 * It is safe to discard "params" here, as the DVB core will sync
	 * fe->dtv_property_cache with fepriv->parameters_in, where the
	 * DVBv3 params are stored. The only practical usage for it indicate
	 * that re-tuning is needed, e. g. (fepriv->state & FESTATE_RETUNE) is
	 * true.
	 */

	*delay = HZ/2;

	/*PR_ATSC("%s:\n", __func__);*/
#if 1
	if (re_tune) {

		timer_begain(D_TIMER_DETECT);
		PR_INFO("%s:\n", __func__);
		dtvdd_devp->en_detect = 1; /*fist set*/

		gxtv_demod_dvbt_set_frontend(fe);
		gxtv_demod_dvbt_read_status(fe, status);
		return 0;
	}
#endif
	if (!dtvdd_devp->en_detect) {
		PR_DBGL("tune:not enable\n");
		return 0;
	}
	/*polling*/
	gxtv_demod_dvbt_read_status(fe, status);

	if (*status & FE_HAS_LOCK) {
		timer_disable(D_TIMER_SET);
	} else {
		if (!timer_is_en(D_TIMER_SET))
			timer_begain(D_TIMER_SET);
	}

	if (timer_is_enough(D_TIMER_SET)) {
		gxtv_demod_dvbt_set_frontend(fe);
		timer_disable(D_TIMER_SET);
	}



	return 0;

}

int Gxtv_Demod_Atsc_Init(void/*struct aml_fe_dev *dev*/)
{
	struct aml_demod_sys sys;
	/*struct aml_demod_i2c i2c;*/

	PR_DBG("%s\n", __func__);

	memset(&sys, 0, sizeof(sys));
	/*memset(&i2c, 0, sizeof(i2c));*/
	memset(&demod_status, 0, sizeof(demod_status));
	/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC*/
	demod_status.dvb_mode = Gxtv_Atsc;
	sys.adc_clk = Adc_Clk_24M;    /*Adc_Clk_26M;*/
	if (is_ic_ver(IC_VER_TL1))
		sys.demod_clk = Demod_Clk_250M;
	else
		sys.demod_clk = Demod_Clk_225M;
	demod_status.ch_if = 5000;
	demod_status.tmp = Adc_mode;
	/*demod_set_sys(&demod_status, &i2c, &sys);*/
	demod_set_sys(&demod_status, &sys);
	demod_mode_para = AML_ATSC;
	atsc_flag = VSB_8;
	return 0;
}

static void gxtv_demod_dtmb_release(struct dvb_frontend *fe)
{

}

static int gxtv_demod_dtmb_read_status
	(struct dvb_frontend *fe, enum fe_status *status)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	*status = pollm->last_s;
	return 0;
}

void dtmb_save_status(unsigned int s)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	if (s) {
		pollm->last_s =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;

	} else {
		pollm->last_s = FE_TIMEDOUT;
	}
}
void dtmb_poll_start(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	pollm->last_s = 0;
	pollm->flg_restart = 1;
	/*pr_dbg("dtmb_poll_start!\n");*/
	PR_DTMB("dtmb_poll_start2\n");

}


void dtmb_poll_stop(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	pollm->flg_stop = 1;

}
void dtmb_set_delay(unsigned int delay)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	pollm->delayms = delay;
	pollm->flg_updelay = 1;
}
unsigned int dtmb_is_update_delay(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	return pollm->flg_updelay;
}
unsigned int dtmb_get_delay_clear(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	pollm->flg_updelay = 0;

	return pollm->delayms;
}

void dtmb_poll_clear(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	memset(pollm, 0, sizeof(struct poll_machie_s));

}

#define DTMBM_NO_SIGNEL_CHECK	0x01
#define DTMBM_HV_SIGNEL_CHECK	0x02
#define DTMBM_BCH_OVER_CHEK	0x04

#define DTMBM_CHEK_NO		(DTMBM_NO_SIGNEL_CHECK)
#define DTMBM_CHEK_HV		(DTMBM_HV_SIGNEL_CHECK | DTMBM_BCH_OVER_CHEK)
#define DTMBM_WORK		(DTMBM_NO_SIGNEL_CHECK | DTMBM_HV_SIGNEL_CHECK\
					| DTMBM_BCH_OVER_CHEK)

#define DTMBM_POLL_CNT_NO_SIGNAL	(10)
#define DTMBM_POLL_CNT_WAIT_LOCK	(3) /*from 30 to 3 */
#define DTMBM_POLL_DELAY_NO_SIGNAL	(120)
#define DTMBM_POLL_DELAY_HAVE_SIGNAL	(100)
/*dtmb_poll_v3 is same as dtmb_check_status_txl*/
void dtmb_poll_v3(void)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;
	unsigned int bch_tmp;
	unsigned int s;

	if (!pollm->state) {
		/* idle */
		/* idle -> start check */
		if (!pollm->flg_restart) {
			PR_DBG("x");
			return;
		}
	} else {
		if (pollm->flg_stop) {
			PR_DBG("dtmb poll stop !\n");
			dtmb_poll_clear();
			dtmb_set_delay(3*HZ);
			return;
		}
	}

	/* restart: clear */
	if (pollm->flg_restart) {
		PR_DBG("dtmb poll restart!\n");
		dtmb_poll_clear();

	}
	PR_DBG("-");
	s = check_dtmb_fec_lock();

	/* bch exceed the threshold: wait lock*/
	if (pollm->state & DTMBM_BCH_OVER_CHEK) {
		if (pollm->crrcnt < DTMBM_POLL_CNT_WAIT_LOCK) {
			pollm->crrcnt++;

			if (s) {
				PR_DBG("after reset get lock again!cnt=%d\n",
					pollm->crrcnt);
				dtmb_constell_check();
				pollm->state = DTMBM_HV_SIGNEL_CHECK;
				pollm->crrcnt = 0;
				pollm->bch = dtmb_reg_r_bch();


				dtmb_save_status(s);
			}
		} else {
			PR_DBG("can't lock after reset!\n");
			pollm->state = DTMBM_NO_SIGNEL_CHECK;
			pollm->crrcnt = 0;
			/* to no signal*/
			dtmb_save_status(s);
		}
		return;
	}


	if (s) {
		/*have signal*/
		if (!pollm->state) {
			pollm->state = DTMBM_CHEK_NO;
			PR_DBG("from idle to have signal wait 1\n");
			return;
		}
		if (pollm->state & DTMBM_CHEK_NO) {
			/*no to have*/
			PR_DBG("poll machie: from no signal to have signal\n");
			pollm->bch = dtmb_reg_r_bch();
			pollm->state = DTMBM_HV_SIGNEL_CHECK;

			dtmb_set_delay(DTMBM_POLL_DELAY_HAVE_SIGNAL);
			return;
		}


		bch_tmp = dtmb_reg_r_bch();
		if (bch_tmp > (pollm->bch + 50)) {
			pollm->state = DTMBM_BCH_OVER_CHEK;

			PR_DBG("bch add ,need reset,wait not to reset\n");
			dtmb_reset();

			pollm->crrcnt = 0;
			dtmb_set_delay(DTMBM_POLL_DELAY_HAVE_SIGNAL);
		} else {
			pollm->bch = bch_tmp;
			pollm->state = DTMBM_HV_SIGNEL_CHECK;

			dtmb_save_status(s);
			/*have signale to have signal*/
			dtmb_set_delay(300);
		}
		return;
	}


	/*no signal */
	if (!pollm->state) {
		/* idle -> no signal */
		PR_DBG("poll machie: from idle to no signal\n");
		pollm->crrcnt = 0;

		pollm->state = DTMBM_NO_SIGNEL_CHECK;
	} else if (pollm->state & DTMBM_CHEK_HV) {
		/*have signal -> no signal*/
		PR_DBG("poll machie: from have signal to no signal\n");
		pollm->crrcnt = 0;
		pollm->state = DTMBM_NO_SIGNEL_CHECK;
		dtmb_save_status(s);
	}

	/*no siganel check process */
	if (pollm->crrcnt < DTMBM_POLL_CNT_NO_SIGNAL) {
		dtmb_no_signal_check_v3();
		pollm->crrcnt++;

		dtmb_set_delay(DTMBM_POLL_DELAY_NO_SIGNAL);
	} else {
		dtmb_no_signal_check_finishi_v3();
		pollm->crrcnt = 0;

		dtmb_save_status(s);
		/*no signal to no signal*/
		dtmb_set_delay(300);
	}

}

void dtmb_poll_start_tune(unsigned int state)
{
	struct poll_machie_s *pollm = &dtvdd_devp->poll_machie;

	dtmb_poll_clear();

	pollm->state = state;
	if (state & DTMBM_NO_SIGNEL_CHECK)
		dtmb_save_status(0);
	else
		dtmb_save_status(1);
	PR_DTMB("dtmb_poll_start tune to %d\n", state);

}

/*come from gxtv_demod_dtmb_read_status, have ms_delay*/
int dtmb_poll_v2(struct dvb_frontend *fe, enum fe_status *status)
{
	int ilock;
	unsigned char s = 0;

	s = dtmb_check_status_gxtv(fe);

	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;
		*status = FE_TIMEDOUT;
	}
	if (last_lock != ilock) {
		PR_INFO("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}


/*this is ori gxtv_demod_dtmb_read_status*/
static int gxtv_demod_dtmb_read_status_old
	(struct dvb_frontend *fe, enum fe_status *status)
{

	int ilock;
	unsigned char s = 0;

	if (is_dtmb_ver(IC_DTMB_V2)) {
		s = dtmb_check_status_gxtv(fe);
	} else if (is_dtmb_ver(IC_DTMB_V3)) {
		if (!is_ic_ver(IC_VER_TL1))
			s = dtmb_check_status_txl(fe);
	} else {

		PR_ERR("%s:not support %d!\n", __func__, get_dtmb_ver());
		return -1;
	}
	s = amdemod_dtmb_stat_islock();
/*      s=1;*/
	if (s == 1) {
		ilock = 1;
		*status =
			FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
	} else {
		ilock = 0;

		if (is_ic_ver(IC_VER_TL1)) {
			if (timer_not_enough(D_TIMER_DETECT)) {
				*status = 0;
				PR_DBG("s=0\n");
			} else {
				*status = FE_TIMEDOUT;
				timer_disable(D_TIMER_DETECT);
			}
		} else {
			*status = FE_TIMEDOUT;
		}
	}
	if (last_lock != ilock) {
		PR_INFO("%s.\n",
			 ilock ? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		last_lock = ilock;
	}

	return 0;
}

static int gxtv_demod_dtmb_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	*ber = 0;

	return 0;
}

static int gxtv_demod_dtmb_read_signal_strength
		(struct dvb_frontend *fe, u16 *strength)
{

	*strength = tuner_get_ch_power3();
	return 0;
}

static int gxtv_demod_dtmb_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	int tmp, snr_avg;

	tmp = snr_avg = 0;
	/* tmp = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR);*/
	tmp = dtmb_reg_r_che_snr();

	*snr = convert_snr(tmp);

	return 0;
}

static int gxtv_demod_dtmb_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}


static int gxtv_demod_dtmb_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct aml_demod_dtmb param;
	int times;

	times = 2;
	PR_DBG("gxtv_demod_dtmb_set_frontend,freq is %d\n", c->frequency);
	memset(&param, 0, sizeof(param));
	param.ch_freq = c->frequency / 1000;

	last_lock = -1;
/* demod_power_switch(PWR_OFF); */
	tuner_set_params(fe);	/*aml_fe_analog_set_frontend(fe);*/
	msleep(100);
/* demod_power_switch(PWR_ON); */

	#if 0
	if (is_ic_ver(IC_VER_TL1))
		demod_set_sys_dtmb_v4();
	else
	#endif
	dtmb_set_ch(&demod_status, /*&demod_i2c,*/ &param);

	return 0;
}


static int gxtv_demod_dtmb_get_frontend(struct dvb_frontend *fe)
{                               /*these content will be writed into eeprom .*/

	return 0;
}

/*int Gxtv_Demod_Dtmb_Init(struct aml_fe_dev *dev)*/
int Gxtv_Demod_Dtmb_Init(struct amldtvdemod_device_s *dev)
{
	struct aml_demod_sys sys;
	/*struct aml_demod_i2c i2c;*/
	PR_DBG("AML Demod DTMB init\r\n");

	memset(&sys, 0, sizeof(sys));
	/*memset(&i2c, 0, sizeof(i2c));*/
	memset(&demod_status, 0, sizeof(demod_status));
	/* 0 -DVBC, 1-DVBT, ISDBT, 2-ATSC*/
	demod_status.dvb_mode = Gxtv_Dtmb;

	if (is_dtmb_ver(IC_DTMB_V2)) {
		sys.adc_clk = Adc_Clk_25M;	/*Adc_Clk_26M;*/
		sys.demod_clk = Demod_Clk_200M;
	} else if (is_dtmb_ver(IC_DTMB_V3)) {
		if (is_ic_ver(IC_VER_TXL)) {
			sys.adc_clk = Adc_Clk_25M;
			sys.demod_clk = Demod_Clk_225M;
		} else if (is_ic_ver(IC_VER_TL1)) {
			sys.adc_clk = Adc_Clk_24M;
			sys.demod_clk = Demod_Clk_250M;
		} else {
			sys.adc_clk = Adc_Clk_24M;
			sys.demod_clk = Demod_Clk_225M;
		}
	} else {
		PR_ERR("%s:not support %d\n", __func__, get_dtmb_ver());
		return -1;
	}

	demod_status.ch_if = Si2176_5M_If;
	demod_status.tmp = Adc_mode;
	demod_status.spectrum = dev->spectrum;
	/*demod_set_sys(&demod_status, &i2c, &sys);*/
	demod_set_sys(&demod_status, &sys);
	demod_mode_para = AML_DTMB;

	return 0;
}
#ifdef DVB_CORE_ORI


static int gxtv_demod_dvbc_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	/*struct dtv_frontend_properties *c = &fe->dtv_property_cache;*/
	int ret = 0;
	/*unsigned int up_delay;*/
	/*unsigned int firstdetet;*/

	*delay = HZ / 4;

#if 1
	if (re_tune) {
		/*first*/
		dtvdd_devp->en_detect = 1;

		gxtv_demod_dvbc_set_frontend(fe);
		/*timer_set_max(D_TIMER_DETECT, 4000);*/
		timer_begain(D_TIMER_DETECT);
		gxtv_demod_dvbc_read_status_timer(fe, status);

		PR_DBG("tune finish!\n");

		return ret;
	}
#endif
	if (!dtvdd_devp->en_detect) {
		PR_DBGL("tune:not enable\n");
		return ret;
	}

	gxtv_demod_dvbc_read_status_timer(fe, status);

	#if 0
	if (is_ic_ver(IC_VER_TL1))
		return ret;
	#endif

	if (*status & FE_HAS_LOCK) {
		timer_disable(D_TIMER_SET);
	} else {
		if (!timer_is_en(D_TIMER_SET))
			timer_begain(D_TIMER_SET);
	}

	if (timer_is_enough(D_TIMER_SET)) {
		gxtv_demod_dvbc_set_frontend(fe);
		timer_disable(D_TIMER_SET);
	}

	return ret;

}

static int gxtv_demod_dtmb_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	/*struct dtv_frontend_properties *c = &fe->dtv_property_cache;*/
	int ret = 0;
//	unsigned int up_delay;
	unsigned int firstdetet;



	if (re_tune) {
		/*first*/
		dtvdd_devp->en_detect = 1;

		*delay = HZ / 4;
		gxtv_demod_dtmb_set_frontend(fe);

		if (is_ic_ver(IC_VER_TL1)) {
			timer_begain(D_TIMER_DETECT);
			firstdetet = 0;
		} else {
			firstdetet = dtmb_detect_first();
		}

		if (firstdetet == 1) {
			*status = FE_TIMEDOUT;
			/*polling mode*/
			dtmb_poll_start_tune(DTMBM_NO_SIGNEL_CHECK);

		} else if (firstdetet == 2) {  /*need check*/
			*status = FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC;
			dtmb_poll_start_tune(DTMBM_HV_SIGNEL_CHECK);

		} else if (firstdetet == 0) {
			PR_DBG("use read_status\n");
			gxtv_demod_dtmb_read_status_old(fe, status);
			if (*status == (0x1f))
				dtmb_poll_start_tune(DTMBM_HV_SIGNEL_CHECK);
			else
				dtmb_poll_start_tune(DTMBM_NO_SIGNEL_CHECK);


		}
		PR_DBG("tune finish!\n");

		return ret;
	}

	if (!dtvdd_devp->en_detect) {
		PR_DBG("tune:not enable\n");
		return ret;
	}

#if 1	/**/
	*delay = HZ / 4;
	gxtv_demod_dtmb_read_status_old(fe, status);

	if (is_ic_ver(IC_VER_TL1)) {
		if (*status == (FE_HAS_LOCK | FE_HAS_SIGNAL | FE_HAS_CARRIER |
			FE_HAS_VITERBI | FE_HAS_SYNC))
			dtmb_poll_start_tune(DTMBM_HV_SIGNEL_CHECK);
		else
			dtmb_poll_start_tune(DTMBM_NO_SIGNEL_CHECK);
	}

#else	/*try polling*/

	/*pr_dbg("+");*/
	if (is_dtmb_ver(IC_DTMB_V3)) {
		dtmb_poll_v3();
		gxtv_demod_dtmb_read_status(fe, status);
		if (dtmb_is_update_delay()) {
			up_delay = (dtmb_get_delay_clear()*HZ)/1000;
			if (up_delay > 0)
				*delay = up_delay;
			else
				PR_DBG("warning:delay is 0\n");
		}
	} else if (is_dtmb_ver(IC_DTMB_V2)) {
		*delay = HZ / 3;
		dtmb_poll_v2(fe, status);
	}

#endif
	return ret;

}

#endif



#ifdef CONFIG_CMA
/*void dtmb_cma_alloc(struct aml_fe_dev *devp)*/
bool dtmb_cma_alloc(struct amldtvdemod_device_s *devp)
{
	bool ret;

	unsigned int mem_size = devp->cma_mem_size;
	/*	dma_alloc_from_contiguous*/
	devp->venc_pages =
		dma_alloc_from_contiguous(&(devp->this_pdev->dev),
			mem_size >> PAGE_SHIFT, 0);
	PR_DBG("[cma]mem_size is %d,%d\n",
		mem_size, mem_size >> PAGE_SHIFT);
	if (devp->venc_pages) {
		devp->mem_start = page_to_phys(devp->venc_pages);
		devp->mem_size  = mem_size;
		devp->flg_cma_allc = true;
		PR_DBG("demod mem_start = 0x%x, mem_size = 0x%x\n",
			devp->mem_start, devp->mem_size);
		PR_DBG("demod cma alloc ok!\n");
		ret = true;
	} else {
		PR_DBG("demod cma mem undefined2.\n");
		ret = false;
	}
	return ret;
}

/*void dtmb_cma_release(struct aml_fe_dev *devp)*/
void dtmb_cma_release(struct amldtvdemod_device_s *devp)
{
	/*	dma_release_from_contiguous*/
	dma_release_from_contiguous(&(devp->this_pdev->dev),
			devp->venc_pages,
			devp->cma_mem_size>>PAGE_SHIFT);
		PR_DBG("demod cma release ok!\n");
	devp->mem_start = 0;
	devp->mem_size = 0;
}
#endif

static bool enter_mode(int mode)
{
	/*struct aml_fe_dev *dev = fe->dtv_demod;*/
	struct amldtvdemod_device_s *devn = dtvdd_devp;
	int memstart_dtmb;
	bool ret = true;

	PR_INFO("%s:%d\n", __func__, mode);

	dtvdemod_set_agc_pinmux(1);

	/*-------------------*/
	/* must enable the adc ref signal for demod, */
	/*vdac_enable(1, 0x2);*/
	dtvdemod_vdac_enable(1);/*on*/
	dtvdd_devp->en_detect = 0;/**/
	dtvdd_devp->n_mode = mode;
	dtmb_poll_stop();/*polling mode*/

	autoFlagsTrig = 1;
	if (cci_thread)
		if (dvbc_get_cci_task() == 1)
			dvbc_create_cci_task();
	/*mem_buf = (long *)phys_to_virt(memstart);*/
	if (mode == AM_FE_DTMB_N) {
		Gxtv_Demod_Dtmb_Init(devn);
		if (is_ic_ver(IC_VER_TL1))
			timer_set_max(D_TIMER_DETECT, 500);
	if (devn->cma_flag == 1) {
		PR_DBG("CMA MODE, cma flag is %d,mem size is %d",
			devn->cma_flag, devn->cma_mem_size);
		if (dtmb_cma_alloc(devn)) {
			memstart_dtmb = devn->mem_start;
		} else {
			ret = false;
			return ret;

		}

	} else {
		memstart_dtmb = devn->mem_start;/*??*/
	}
		devn->act_dtmb = true;
		dtmb_set_mem_st(memstart_dtmb);

		//?, already set in Gxtv_Demod_Dtmb_Init()
		if (!is_ic_ver(IC_VER_TL1))
			demod_write_reg(DEMOD_TOP_REGC, 0x8);
	} else if (mode == AM_FE_QAM_N) {
		Gxtv_Demod_Dvbc_Init(/*dev,*/ Adc_mode);

		/*The maximum time of signal detection is 3s*/
		timer_set_max(D_TIMER_DETECT, 3000);
		/*reset is 4s*/
		timer_set_max(D_TIMER_SET, 4000);
	} else if (mode == AM_FE_ATSC_N) {
		Gxtv_Demod_Atsc_Init();

		if (is_ic_ver(IC_VER_TL1))
			timer_set_max(D_TIMER_DETECT, 4000);
	} else if (mode == AM_FE_OFDM_N || mode == AM_FE_ISDBT_N) {
		Gxtv_Demod_Dvbt_Init();
		/*The maximum time of signal detection is 2s */
		timer_set_max(D_TIMER_DETECT, 2000);
		/*reset is 4s*/
		timer_set_max(D_TIMER_SET, 4000);
		if (devn->cma_flag == 1) {
			PR_DBG("CMA MODE, cma flag is %d,mem size is %d",
				devn->cma_flag,
				devn->cma_mem_size);
			if (dtmb_cma_alloc(devn)) {
				memstart_dtmb = devn->mem_start;
			} else {
				ret = false;
				return ret;

			}
		} else {
			memstart_dtmb = devn->mem_start;/*??*/
		}
		PR_DBG("[im]memstart is %x\n", memstart_dtmb);
		dvbt_write_reg((0x10 << 2), memstart_dtmb);
	}

	return ret;

}

static int leave_mode(int mode)
{
/*	struct aml_fe_dev *dev = fe->dtv_demod;*/
	struct amldtvdemod_device_s *devn = dtvdd_devp;

	PR_INFO("%s:\n", __func__);
	dtvdd_devp->en_detect = 0;
	dtvdd_devp->last_delsys = SYS_UNDEFINED;

	dtvpll_init_flag(0);
	/*dvbc_timer_exit();*/
	if (cci_thread)
		dvbc_kill_cci_task();
	#if 0
	if (mode == AM_FE_DTMB_N) {
		dtmb_poll_stop();	/*polling mode*/
		/* close arbit */

		demod_write_reg(DEMOD_TOP_REGC, 0x0);
		if (devn->cma_flag == 1)
			dtmb_cma_release(devn);
	}
	#else
	if (dtvdd_devp->act_dtmb) {
		dtmb_poll_stop();	/*polling mode*/
		/* close arbit */
		demod_write_reg(DEMOD_TOP_REGC, 0x0);
		dtvdd_devp->act_dtmb = false;
	}
	if ((devn->cma_flag == 1) && dtvdd_devp->flg_cma_allc) {
		dtmb_cma_release(devn);
		dtvdd_devp->flg_cma_allc = false;
	}
	#endif

	adc_set_pll_cntl(0, 0x04, NULL);
	demod_mode_para = UNKNOWN;
	/* should disable the adc ref signal for demod */
	/*vdac_enable(0, 0x2);*/
	dtvdemod_vdac_enable(0);/*off*/
	dtvdemod_set_agc_pinmux(0);
	msleep(200);


	return 0;

}
/* when can't get ic_config by dts, use this*/
const struct meson_ddemod_data  data_gxtvbb = {
	.name = "ddmode_gxtvbb",
	.icver = {
		.atsc = IC_MD_NONE,
		.dvbt = IC_MD_NONE,
		.dtmb = IC_DTMB_V2,
		.dvbc = IC_DVBC_V2,
		.reserved = 0,
		.offset = IC_OFFS_V2,
		.ic = IC_VER_GTVBB,
	},

};

const struct meson_ddemod_data  data_txl = {
	.name = "ddmode_txl",
	.icver = {
		.atsc = IC_MD_NONE,
		.dvbt = IC_MD_NONE,
		.dtmb = IC_DTMB_V3,
		.dvbc = IC_DVBC_V2,
		.reserved = 0,
		.offset = IC_OFFS_V2,
		.ic = IC_VER_TXL,
	},

};

const struct meson_ddemod_data  data_txlx = {
	.name = "ddmode_txlx",
	.icver = {
		.atsc = IC_ATSC_V2,
		.dvbt = IC_DVBT_V2,
		.dtmb = IC_MD_NONE,
		.dvbc = IC_DVBC_V3,
		.reserved = 0,
		.offset = IC_OFFS_V3,
		.ic = IC_VER_TXLX,
	},

};

const struct meson_ddemod_data  data_gxlx = {
	.name = "ddmode_gxlx",
	.icver = {
		.atsc = IC_MD_NONE,
		.dvbt = IC_MD_NONE,
		.dtmb = IC_MD_NONE,
		.dvbc = IC_DVBC_V3,
		.reserved = 0,
		.offset = IC_OFFS_V3,
		.ic = IC_VER_GXLX,
	},

};

const struct meson_ddemod_data  data_txhd = {
	.name = "ddmode_txhd",
	.icver = {
		.atsc = IC_MD_NONE,
		.dvbt = IC_MD_NONE,
		.dtmb = IC_DTMB_V3,
		.dvbc = IC_MD_NONE,
		.reserved = 0,
		.offset = IC_OFFS_V3,
		.ic = IC_VER_TXHD,
	},

};

const struct meson_ddemod_data  data_tl1 = {
	.name = "ddmode_tl1",
	.icver = {
		.atsc = IC_ATSC_V2,
		.dvbt = IC_MD_NONE,
		.dtmb = IC_DTMB_V3,
		.dvbc = IC_DVBC_V3,
		.reserved = 0,
		.offset = IC_OFFS_V4,
		.ic = IC_VER_TL1,
	},

};

static const struct of_device_id meson_ddemod_match[] = {
	{
		.compatible = "amlogic, ddemod-gxtvbb",
		.data		= &data_gxtvbb,
	}, {
		.compatible = "amlogic, ddemod-txl",
		.data		= &data_txl,
	}, {
		.compatible = "amlogic, ddemod-txlx",
		.data		= &data_txlx,
	}, {
		.compatible = "amlogic, ddemod-gxlx",
		.data		= &data_gxlx,
	}, {
		.compatible = "amlogic, ddemod-txhd",
		.data		= &data_txhd,
	}, {
		.compatible = "amlogic, ddemod-tl1",
		.data		= &data_tl1,
	},
	{},
};



/*
 * dds_init_reg_map - physical addr map
 *
 * map physical address of I/O memory resources
 * into the core virtual address space
 */
static int dds_init_reg_map(struct platform_device *pdev)
{

	struct ss_reg_phy *preg = &dtvdd_devp->reg_p[0];
	struct ss_reg_vt *pv = &dtvdd_devp->reg_v[0];
	int i;
	struct resource *res = 0;
	int size = 0;
	int ret = 0;

	for (i = 0; i < ES_MAP_ADDR_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			PR_ERR("%s: res %d is faile\n", __func__, i);
			ret = -ENOMEM;
			break;
		}
		size = resource_size(res);
		preg[i].size = size;
		preg[i].phy_addr = res->start;

		pv[i].v = devm_ioremap_nocache(&pdev->dev,
						res->start, size);
	}
	return ret;
}

int dtvdemod_set_iccfg_by_dts(struct platform_device *pdev)
{

	/*struct ic_cfg_s *ic_cfg = &dtvdd_devp->iccfg;*/

	/*int size_io_reg;*/
	u32 value;
	int ret;
	/*struct resource *res = &dtvdemod_mem;*/


	PR_DBG("%s:\n", __func__);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		PR_INFO("no reserved mem.\n");


	/*agc pinmux: option*/
	ret = of_property_read_string(pdev->dev.of_node, "pinctrl-names",
			&dtvdd_devp->pin_name);

	if (ret)
		PR_INFO("pinmux:not define in dts\n");
	else
		PR_INFO("pinmux name:%s\n", dtvdd_devp->pin_name);

/*move from aml_fe*/
	/*snprintf(buf, sizeof(buf), "%s%d_spectrum", name, id);*/
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, "spectrum", &value);
	if (!ret) {
		dtvdd_devp->spectrum = value;
		PR_INFO("spectrum: %d\n", value);
	} else {
		dtvdd_devp->spectrum = 2;
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spectrum");
	if (res) {
		int spectrum = res->start;

		dtvdd_devp->spectrum = spectrum;
	} else {
		dtvdd_devp->spectrum = 0;
	}
#endif
	/*snprintf(buf, sizeof(buf), "%s%d_cma_flag", name, id);*/
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, "cma_flag", &value);
	if (!ret) {
		dtvdd_devp->cma_flag = value;
		PR_INFO("cma_flag: %d\n", value);
	} else {
		dtvdd_devp->cma_flag = 0;
	}
#else				/*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cma_flag");
	if (res) {
		int cma_flag = res->start;

		dtvdd_devp->cma_flag = cma_flag;
	} else {
		dtvdd_devp->cma_flag = 0;
	}
#endif
	/*snprintf(buf, sizeof(buf), "%s%d_atsc_version", name, id);*/
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, "atsc_version", &value);
	if (!ret) {
		dtvdd_devp->atsc_version = value;
		PR_INFO("atsc_version: %d\n", value);
	} else {
		dtvdd_devp->atsc_version = 0;
	}
#else /*CONFIG_OF */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					"atsc_version");
	if (res) {
		int atsc_version = res->start;

		dtvdd_devp->atsc_version = atsc_version;
	} else {
		dtvdd_devp->atsc_version = 0;
	}
#endif

	if (dtvdd_devp->cma_flag == 1) {
		/*snprintf(buf, sizeof(buf), "%s%d_cma_mem_size", name, id);*/
#ifdef CONFIG_CMA
#ifdef CONFIG_OF
		ret = of_property_read_u32(pdev->dev.of_node,
					"cma_mem_size", &value);
		if (!ret) {
			dtvdd_devp->cma_mem_size = value;
			PR_INFO("cma_mem_size: %d\n", value);
		} else {
			dtvdd_devp->cma_mem_size = 0;
		}
#else				/*CONFIG_OF */
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"cma_mem_size");
		if (res) {
			int cma_mem_size = res->start;

			dtvdd_devp->cma_mem_size = cma_mem_size;
		} else {
			dtvdd_devp->cma_mem_size = 0;
		}
#endif
		dtvdd_devp->cma_mem_size =
				dma_get_cma_size_int_byte(&pdev->dev);
			dtvdd_devp->this_pdev = pdev;
			dtvdd_devp->cma_mem_alloc = 0;
			PR_INFO("[cma]demod cma_mem_size = %d MB\n",
					(u32)dtvdd_devp->cma_mem_size/SZ_1M);
#endif
		} else {
#ifdef CONFIG_OF
		dtvdd_devp->mem_start = memstart;
#endif
	}

/*end-------------*/
	return 0;

}
#if (defined CONFIG_AMLOGIC_DTV_DEMOD)	/*move to aml_dtv_demod*/
static int rmem_demod_device_init(struct reserved_mem *rmem, struct device *dev)
{
	unsigned int demod_mem_start;
	unsigned int demod_mem_size;

	demod_mem_start = rmem->base;
	demod_mem_size = rmem->size;
	memstart = demod_mem_start;
	pr_info("demod reveser memory 0x%x, size %dMB.\n",
		demod_mem_start, (demod_mem_size >> 20));
	return 1;
}

static void rmem_demod_device_release(struct reserved_mem *rmem,
				      struct device *dev)
{
}

static const struct reserved_mem_ops rmem_demod_ops = {
	.device_init = rmem_demod_device_init,
	.device_release = rmem_demod_device_release,
};

static int __init rmem_demod_setup(struct reserved_mem *rmem)
{
	/*
	 * struct cma *cma;
	 * int err;
	 * pr_info("%s setup.\n",__func__);
	 * err = cma_init_reserved_mem(rmem->base, rmem->size, 0, &cma);
	 * if (err) {
	 *      pr_err("Reserved memory: unable to setup CMA region\n");
	 *      return err;
	 * }
	 */
	rmem->ops = &rmem_demod_ops;
	/* rmem->priv = cma; */

	pr_info
	    ("DTV demod reserved memory: %pa, size %ld MiB\n",
	     &rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}

RESERVEDMEM_OF_DECLARE(demod, "amlogic, demod-mem", rmem_demod_setup);
#endif

int dtvdemod_init_regaddr_byversion(struct platform_device *pdev)
{
	struct ic_ver *pic = &dtvdd_devp->icver;	/*in*/
	struct ddemod_reg_off *ireg = &dtvdd_devp->ireg;	/*set*/

	unsigned int off_ver = pic->offset;
	int ret = 0;

	/*clear*/
	ireg->off_demod_top = 0x00;
	ireg->off_dvbc = 0x00;
	ireg->off_dtmb = 0x00;
	ireg->off_dvbt = 0x00;
	ireg->off_atsc = 0x00;
	ireg->off_front = 0x00;
	ireg->off_isdbt = 0x00;

	if (is_offset_ver(IC_OFFS_V2)) {
		ireg->off_demod_top = 0xc00;
		ireg->off_dvbc = 0x400;
		ireg->off_dtmb = 0x00;
	} else if (is_offset_ver(IC_OFFS_V3)) {
		ireg->off_demod_top = 0xf00;
		ireg->off_dvbc = 0xc00;
		ireg->off_dtmb = 0x000;
		ireg->off_dvbt = 0x400;
		ireg->off_isdbt = ireg->off_dvbt;
		ireg->off_atsc = 0x800;
	} else if (is_offset_ver(IC_OFFS_V4)) {
		ireg->off_demod_top = 0x3c00;
		ireg->off_dvbc = 0x1000;
		ireg->off_dtmb = 0x0000;
		ireg->off_dvbt = 0x0400;
		ireg->off_isdbt = 0x0800;
		ireg->off_atsc = 0x0c00;
		ireg->off_front = 0x3800;
	} else {
		PR_ERR("ic_offset_version[%d] is not support!\n", off_ver);
		ireg->off_demod_top = 0xf00;
		ireg->off_dvbc = 0xc00;
		ireg->off_dtmb = 0x000;
		ireg->off_dvbt = 0x400;
		ireg->off_atsc = 0x800;

		ret = -1;
	}


	return ret;
}





/* It's a correspondence with enum es_map_addr*/

void dbg_ic_cfg(void)
{
	struct ic_ver *pic = &dtvdd_devp->icver;
	struct ss_reg_phy *preg = &dtvdd_devp->reg_p[0];
	int i;

	PR_INFO("ic_version=0x%x\n", pic->ic);
	PR_INFO("\tver_dvbc:\t%d\n", pic->dvbc);
	PR_INFO("\tver_dtmb:\t%d\n", pic->dtmb);
	PR_INFO("\tver_dvbt:\t%d\n", pic->dvbt);
	PR_INFO("\tver_atsc:\t%d\n", pic->atsc);
	PR_INFO("\tver_offset:\t%d\n", pic->offset);


	for (i = 0; i < ES_MAP_ADDR_NUM; i++)
		PR_INFO("reg:%s:st=0x%x,size=0x%x\n",
			name_reg[i], preg[i].phy_addr, preg[i].size);


}

void dbg_reg_addr(void)
{
	struct ddemod_reg_off *ireg = &dtvdd_devp->ireg;
	struct ss_reg_vt *regv = &dtvdd_devp->reg_v[0];
	int i;

	PR_INFO("%s\n", __func__);

	PR_INFO("reg address offset:\n");
	PR_INFO("\tdemod top:\t0x%x\n", ireg->off_demod_top);
	PR_INFO("\tdvbc:\t0x%x\n", ireg->off_dvbc);
	PR_INFO("\tdtmb:\t0x%x\n", ireg->off_dtmb);
	PR_INFO("\tdvbt:\t0x%x\n", ireg->off_dvbt);
	PR_INFO("\tisdbt:\t0x%x\n", ireg->off_isdbt);
	PR_INFO("\tatsc:\t0x%x\n", ireg->off_atsc);
	PR_INFO("\tfront:\t0x%x\n", ireg->off_front);

	PR_INFO("virtual addr:\n");
	for (i = 0; i < ES_MAP_ADDR_NUM; i++)
		PR_INFO("\t%s:\t0x%p\n", name_reg[i], regv[i].v);


}
static void dtvdemod_set_agc_pinmux(int on)
{
	if (dtvdd_devp->pin_name == NULL) {
		PR_INFO("no pinmux control\n");
		return;
	}

	if (on) {
		dtvdd_devp->pin = devm_pinctrl_get_select(dtvdd_devp->dev,
							dtvdd_devp->pin_name);
		if (IS_ERR(dtvdd_devp->pin)) {
			dtvdd_devp->pin = NULL;
			PR_ERR("get agc pins fail\n");
		}
	} else {
		/*off*/
		if (!IS_ERR_OR_NULL(dtvdd_devp->pin)) {
			devm_pinctrl_put(dtvdd_devp->pin);
			dtvdd_devp->pin = NULL;
		}
	}

}

static void dtvdemod_clktree_probe(struct device *dev)
{
	dtvdd_devp->clk_gate_state = 0;

	dtvdd_devp->vdac_clk_gate = devm_clk_get(dev, "vdac_clk_gate");
	if (IS_ERR(dtvdd_devp->vdac_clk_gate))
		PR_ERR("error: %s: clk vdac_clk_gate\n", __func__);
}

static void dtvdemod_clktree_remove(struct device *dev)
{
	if (!IS_ERR(dtvdd_devp->vdac_clk_gate))
		devm_clk_put(dev, dtvdd_devp->vdac_clk_gate);
}
static void vdac_clk_gate_ctrl(int status)
{
	if (status) {
		if (dtvdd_devp->clk_gate_state) {
			PR_INFO("clk_gate is already on\n");
			return;
		}

		if (IS_ERR(dtvdd_devp->vdac_clk_gate))
			PR_ERR("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_prepare_enable(dtvdd_devp->vdac_clk_gate);

		dtvdd_devp->clk_gate_state = 1;
	} else {
		if (dtvdd_devp->clk_gate_state == 0) {
			PR_INFO("clk_gate is already off\n");
			return;
		}

		if (IS_ERR(dtvdd_devp->vdac_clk_gate))
			PR_ERR("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_disable_unprepare(dtvdd_devp->vdac_clk_gate);

		dtvdd_devp->clk_gate_state = 0;
	}
}
/*
 * use dtvdemod_vdac_enable replace vdac_enable
 */
static void dtvdemod_vdac_enable(bool on)
{
	if (on) {
		vdac_clk_gate_ctrl(1);
		vdac_enable(1, 0x02);
	} else {
		vdac_clk_gate_ctrl(0);
		vdac_enable(0, 0x02);
	}
}

/* platform driver*/
static int aml_dtvdemod_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct of_device_id *match;

	PR_INFO("%s\n", __func__);
	/*memory*/
	dtvdd_devp = kzalloc(sizeof(struct amldtvdemod_device_s),
			GFP_KERNEL);

	if (!dtvdd_devp)
		goto fail_alloc_region;

	dtvdd_devp->state = DTVDEMOD_ST_NOT_INI;


	/*class attr */
	dtvdd_devp->clsp = class_create(THIS_MODULE, DEMOD_DEVICE_NAME);
	if (!dtvdd_devp->clsp)
		goto fail_create_class;

	ret = class_create_file(dtvdd_devp->clsp, &class_attr_auto_sym);
	if (ret)
		goto fail_class_create_file;

	ret = class_create_file(dtvdd_devp->clsp, &class_attr_dtmb_para);
	if (ret)
		goto fail_class_create_file;

	ret = class_create_file(dtvdd_devp->clsp, &class_attr_dvbc_reg);
	if (ret)
		goto fail_class_create_file;

	ret = class_create_file(dtvdd_devp->clsp, &class_attr_atsc_para);
	if (ret)
		goto fail_class_create_file;

	ret = class_create_file(dtvdd_devp->clsp, &class_attr_demod_rate);
	if (ret)
		goto fail_class_create_file;

	ret = class_create_file(dtvdd_devp->clsp, &class_attr_info);
	if (ret)
		goto fail_class_create_file;

	/**/
	match = of_match_device(meson_ddemod_match, &pdev->dev);
	if (match == NULL) {
		PR_ERR("%s,no matched table\n", __func__);
		goto fail_ic_config;
	}
	dtvdd_devp->data = (struct meson_ddemod_data *)match->data;
	dtvdd_devp->icver = dtvdd_devp->data->icver;

	/*reg*/
	ret = dds_init_reg_map(pdev);
	if (ret)
		goto fail_ic_config;

	/*mem info from dts*/
	ret = dtvdemod_set_iccfg_by_dts(pdev);
	if (ret)
		goto fail_ic_config;
		/*dtvdemod_set_iccfg_by_cputype();*/

	ret = dtvdemod_init_regaddr_byversion(pdev);
	if (ret)
		goto fail_ic_config;

	/*debug:*/
	dbg_ic_cfg();
	dbg_reg_addr();

	/**/
	dtvpll_lock_init();
	/* init */
	/*dtvdemod_base_add_init();*/
	mutex_init(&dtvdd_devp->lock);

	dtvdd_devp->dev = &pdev->dev;

	dtvdemod_clktree_probe(&pdev->dev);


	dtvdd_devp->state = DTVDEMOD_ST_IDLE;
	dtvdemod_version(dtvdd_devp);
	dtvdd_devp->flg_cma_allc = false;
	dtvdd_devp->act_dtmb = false;
	//ary temp:
	aml_demod_init();

	PR_INFO("[amldtvdemod.] : probe ok.\n");
	return 0;
fail_ic_config:
	PR_ERR("ic config error.\n");
fail_class_create_file:
	PR_ERR("dtvdemod class file create error.\n");
	class_destroy(dtvdd_devp->clsp);
fail_create_class:
	PR_ERR("dtvdemod class create error.\n");
	kfree(dtvdd_devp);
fail_alloc_region:
	PR_ERR("dtvdemod alloc error.\n");
	PR_ERR("dtvdemod_init fail.\n");


	return ret;
}

static int __exit aml_dtvdemod_remove(struct platform_device *pdev)
{
	if (dtvdd_devp == NULL)
		return -1;

	dtvdemod_clktree_remove(&pdev->dev);

	mutex_destroy(&dtvdd_devp->lock);

	class_remove_file(dtvdd_devp->clsp, &class_attr_auto_sym);
	class_remove_file(dtvdd_devp->clsp, &class_attr_dtmb_para);
	class_remove_file(dtvdd_devp->clsp, &class_attr_dvbc_reg);
	class_remove_file(dtvdd_devp->clsp, &class_attr_atsc_para);
	class_remove_file(dtvdd_devp->clsp, &class_attr_demod_rate);
	class_remove_file(dtvdd_devp->clsp, &class_attr_info);

	class_destroy(dtvdd_devp->clsp);

	kfree(dtvdd_devp);
	PR_INFO("%s:remove.\n", __func__);

	aml_demod_exit();//ary temp
	return 0;
}

static void aml_dtvdemod_shutdown(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	if (dtvdd_devp->state != DTVDEMOD_ST_IDLE) {
		leave_mode(0);
		dtvdd_devp->state = DTVDEMOD_ST_IDLE;
	}

}

static struct platform_driver aml_dtvdemod_driver = {
	.driver = {
		.name = "aml_dtv_demod",
		.owner = THIS_MODULE,
		/*aml_dtvdemod_dt_match*/
		.of_match_table = meson_ddemod_match,
	},
	.shutdown   = aml_dtvdemod_shutdown,
	.probe = aml_dtvdemod_probe,
	.remove = __exit_p(aml_dtvdemod_remove),
};


static int __init aml_dtvdemod_init(void)
{
	if (platform_driver_register(&aml_dtvdemod_driver)) {
		pr_err("failed to register amldtvdemod driver module\n");
		return -ENODEV;
	}
	PR_INFO("[amldtvdemod..]%s.\n", __func__);
	return 0;
}

static void __exit aml_dtvdemod_exit(void)
{
	platform_driver_unregister(&aml_dtvdemod_driver);
	PR_INFO("[amldtvdemod..]%s: driver removed ok.\n", __func__);
}

#if 0
static int delsys_confirm(struct dvb_frontend *fe)
{
	enum fe_delivery_system ldelsys = dtvdd_devp->last_delsys;
	enum fe_delivery_system cdelsys;

	int ncaps, support;
	enum aml_fe_n_mode_t mode = AM_FE_UNKNOWN_N;

	cdelsys = fe->dtv_property_cache.delivery_system;

	/*same*/
	if (ldelsys == cdelsys) {
		//PR_DBG("delsys is same, do nothing\n");
		return 0;
	}

	/*support ?*/
	ncaps = 0;
	support = 0;
	while (ncaps < MAX_DELSYS && fe->ops.delsys[ncaps]) {
		if (fe->ops.delsys[ncaps] == cdelsys) {

			support = 1;
			break;
		}
		ncaps++;
	}

	if (!support) {
		PR_INFO("delsys:%d is not support!\n", cdelsys);
		return 0;
	}

	PR_DBG("%s:l=%d,c=%d\n", __func__, ldelsys, cdelsys);

	switch (cdelsys) {

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		/*dvbc*/
		if (ldelsys == SYS_DVBC_ANNEX_A
			|| ldelsys == SYS_DVBC_ANNEX_C) {
			break;
		}
		mode = AM_FE_QAM_N;
		break;
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		/*atsc*/
		if (ldelsys  == SYS_ATSC
			|| ldelsys == SYS_ATSCMH
			|| ldelsys == SYS_DVBC_ANNEX_B) {
			break;

		}
		mode = AM_FE_ATSC_N;
		break;
	case SYS_DVBT:
	case SYS_DVBT2:
		/*dvbt, OFDM*/
		if (ldelsys  == SYS_DVBT
			|| ldelsys == SYS_DVBT2) {
			break;

		}
		mode = AM_FE_OFDM_N;
		break;
	case SYS_ISDBT:
		if (ldelsys != SYS_ISDBT)
			mode = AM_FE_ISDBT_N;
		break;
	case SYS_DTMB:
		/*dtmb*/
		mode = AM_FE_DTMB_N;
		break;
	case SYS_DVBS:
	case SYS_DVBS2:
		/*QPSK*/
		if (ldelsys  == SYS_DVBS
			|| ldelsys == SYS_DVBS2) {

			break;

		}
		mode = AM_FE_QPSK_N;
		break;

	case SYS_DSS:
	case SYS_DVBH:

	case SYS_ISDBS:
	case SYS_ISDBC:
	case SYS_CMMB:
	case SYS_DAB:
	case SYS_TURBO:
	case SYS_UNDEFINED:
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	case SYS_ANALOG:
#endif
		mode = AM_FE_UNKNOWN_N;
		PR_INFO("delsys not support!%d=\n", cdelsys);
		return 0;
	}

	if (mode != AM_FE_UNKNOWN_N)
		enter_mode(mode);

	if (!get_dtvpll_init_flag()) {
		PR_INFO("pll is not set!\n");
		leave_mode(mode);

		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
		dtvdd_devp->last_delsys = SYS_UNDEFINED;
		dtvdd_devp->n_mode = AM_FE_UNKNOWN_N;
		return 0;
	}

	dtvdd_devp->last_delsys = cdelsys;

	return 0;
}
#endif
static int delsys_set(struct dvb_frontend *fe, unsigned int delsys)
{
	enum fe_delivery_system ldelsys = dtvdd_devp->last_delsys;
	enum fe_delivery_system cdelsys;

	int ncaps, support;
	enum aml_fe_n_mode_t mode = AM_FE_UNKNOWN_N;
	enum aml_fe_n_mode_t lmode = dtvdd_devp->n_mode;

	cdelsys = delsys; /*diff*/

	/*same*/
	if (ldelsys == cdelsys) {
		//PR_DBG("delsys is same, do nothing\n");
		return 0;
	}

	/*support ?*/
	ncaps = 0;
	support = 0;
	while (ncaps < MAX_DELSYS && fe->ops.delsys[ncaps]) {
		if (fe->ops.delsys[ncaps] == cdelsys) {

			support = 1;
			break;
		}
		ncaps++;
	}

	if (!support) {
		if (get_dtvpll_init_flag()) {
			/**/
			PR_INFO("delsys:%d is not support!\n", cdelsys);
			leave_mode(lmode);
			if (fe->ops.tuner_ops.release)
				fe->ops.tuner_ops.release(fe);
			dtvdd_devp->last_delsys = SYS_UNDEFINED;
			dtvdd_devp->n_mode = AM_FE_UNKNOWN_N;
		}
		return 0;
	}

	PR_DBG("%s:l=%d,c=%d\n", __func__, ldelsys, cdelsys);
	dbg_delsys(cdelsys);

	switch (cdelsys) {

	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		/*dvbc*/
		if (ldelsys == SYS_DVBC_ANNEX_A
			|| ldelsys == SYS_DVBC_ANNEX_C) {
			break;
		}
		mode = AM_FE_QAM_N;
		break;
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		/*atsc*/
		if (ldelsys  == SYS_ATSC
			|| ldelsys == SYS_ATSCMH
			|| ldelsys == SYS_DVBC_ANNEX_B) {
			break;

		}
		mode = AM_FE_ATSC_N;
		break;
	case SYS_DVBT:
	case SYS_DVBT2:
		/*dvbt, OFDM*/
		if (ldelsys  == SYS_DVBT
			|| ldelsys == SYS_DVBT2) {
			break;

		}
		mode = AM_FE_OFDM_N;
		break;
	case SYS_ISDBT:
		if (ldelsys != SYS_ISDBT)
			mode = AM_FE_ISDBT_N;
		break;
	case SYS_DTMB:
		/*dtmb*/
		mode = AM_FE_DTMB_N;
		break;
	case SYS_DVBS:
	case SYS_DVBS2:
		/*QPSK*/
		if (ldelsys  == SYS_DVBS
			|| ldelsys == SYS_DVBS2) {

			break;

		}
		mode = AM_FE_QPSK_N;
		break;

	case SYS_DSS:
	case SYS_DVBH:

	case SYS_ISDBS:
	case SYS_ISDBC:
	case SYS_CMMB:
	case SYS_DAB:
	case SYS_TURBO:
	case SYS_UNDEFINED:
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	case SYS_ANALOG:
#endif
		mode = AM_FE_UNKNOWN_N;
		if (get_dtvpll_init_flag()) {
			PR_INFO("delsys not support!%d=\n", cdelsys);
			leave_mode(mode);
			if (fe->ops.tuner_ops.release)
				fe->ops.tuner_ops.release(fe);
			dtvdd_devp->last_delsys = SYS_UNDEFINED;
			dtvdd_devp->n_mode = AM_FE_UNKNOWN_N;
		}
		return 0;
	}

	if (mode != AM_FE_UNKNOWN_N) {
		if (!enter_mode(mode)) {
			PR_INFO("enter_mode failed,leave!\n");
			leave_mode(mode);
			if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
			dtvdd_devp->last_delsys = SYS_UNDEFINED;
			dtvdd_devp->n_mode = AM_FE_UNKNOWN_N;
			return 0;
		}
	}

	if (!get_dtvpll_init_flag()) {
		PR_INFO("pll is not set!\n");
		leave_mode(mode);
		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
		dtvdd_devp->last_delsys = SYS_UNDEFINED;
		dtvdd_devp->n_mode = AM_FE_UNKNOWN_N;
		return 0;
	}

	dtvdd_devp->last_delsys = cdelsys;
#if 1	/*ary add for test*/
	PR_INFO("info type:%d", fe->ops.info.type);
	if (mode == AM_FE_ATSC_N)
		fe->ops.info.type = FE_ATSC;
	else if (mode == AM_FE_OFDM_N)
		fe->ops.info.type = FE_OFDM;
	else if (mode == AM_FE_DTMB_N)
		fe->ops.info.type = FE_DTMB;
	else if (mode == AM_FE_QAM_N)
		fe->ops.info.type = FE_QAM;
	else if (mode == AM_FE_ISDBT_N)
		fe->ops.info.type = FE_ISDBT;

	fe->ops.tuner_ops.set_config(fe, NULL);

#endif

	return 0;
}

static int is_not_active(struct dvb_frontend *fe)
{
	enum fe_delivery_system cdelsys;
	enum fe_delivery_system ldelsys = dtvdd_devp->last_delsys;

	if (!get_dtvpll_init_flag())
		return 1;

	cdelsys = fe->dtv_property_cache.delivery_system;
	if (ldelsys != cdelsys)
		return 2;


	return 0;/*active*/
}
/*ko attach==============================*/
static int aml_dtvdm_init(struct dvb_frontend *fe)
{

	return 0;
}
static int aml_dtvdm_sleep(struct dvb_frontend *fe)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	if (get_dtvpll_init_flag()) {
		PR_INFO("%s\n", __func__);
		leave_mode(nmode);
		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
		dtvdd_devp->last_delsys = SYS_UNDEFINED;
		dtvdd_devp->n_mode = AM_FE_UNKNOWN_N;
	}
	return 0;
}
static int aml_dtvdm_set_parameters(struct dvb_frontend *fe)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;
	int ret = 0;

	PR_INFO("%s", __func__);

	/*delsys_confirm(fe);*/
	if (is_not_active(fe)) {
		PR_DBG("set parm:not active\n");
		return 0;
	}
	switch (nmode) {
	case AM_FE_QPSK_N:

		break;
	case AM_FE_QAM_N:
		PR_INFO("FE_QAM\n");
		timer_begain(D_TIMER_DETECT);
		dtvdd_devp->en_detect = 1; /*fist set*/
		ret = gxtv_demod_dvbc_set_frontend(fe);
		break;
	case AM_FE_OFDM_N:
		PR_INFO("FE_OFDM\n");
		timer_begain(D_TIMER_DETECT);
		dtvdd_devp->en_detect = 1; /*fist set*/

		ret = gxtv_demod_dvbt_set_frontend(fe);

		break;
	case AM_FE_ISDBT_N:	/*same as dvbt*/
		PR_INFO("FE_ISDBT\n");

		timer_begain(D_TIMER_DETECT);
		dtvdd_devp->en_detect = 1; /*fist set*/
		ret = gxtv_demod_dvbt_set_frontend(fe);

		break;
	case AM_FE_ATSC_N:
		PR_INFO("FE_ATSC\n");
		ret = gxtv_demod_atsc_set_frontend(fe);
		break;
	case AM_FE_DTMB_N:
		PR_INFO("FE_DTMB\n");
		ret = gxtv_demod_dtmb_set_frontend(fe);
		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}
	return ret;
}

static int aml_dtvdm_get_frontend(struct dvb_frontend *fe,
				 struct dtv_frontend_properties *p)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("get parm:not active\n");
		return 0;
	}
	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		ret = gxtv_demod_dvbc_get_frontend(fe);
		break;
	case AM_FE_OFDM_N:
		ret = gxtv_demod_dvbt_get_frontend(fe);
		break;
	case AM_FE_ISDBT_N:	/*same as dvbt*/
		ret = gxtv_demod_dvbt_get_frontend(fe);
		break;
	case AM_FE_ATSC_N:
		ret = gxtv_demod_atsc_get_frontend(fe);
		break;
	case AM_FE_DTMB_N:
		ret = gxtv_demod_dtmb_get_frontend(fe);
		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}
	return ret;
}
static int aml_dtvdm_get_tune_settings(struct dvb_frontend *fe,
				      struct dvb_frontend_tune_settings
					*fe_tune_settings)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("get parm:not active\n");
		return 0;
	}
	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		fe_tune_settings->min_delay_ms = 300;
		fe_tune_settings->step_size = 0; /* no zigzag */
		fe_tune_settings->max_drift = 0;

		break;
	case AM_FE_OFDM_N:
		/*dvbt*/
		fe_tune_settings->min_delay_ms = 500;
		fe_tune_settings->step_size = 0;
		fe_tune_settings->max_drift = 0;
		break;
	case AM_FE_ISDBT_N:	/*same as dvbt*/
		/*isdbt*/
		fe_tune_settings->min_delay_ms = 300;
		fe_tune_settings->step_size = 0;
		fe_tune_settings->max_drift = 0;

		break;
	case AM_FE_ATSC_N:

		break;
	case AM_FE_DTMB_N:

		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}
	return ret;
}
static int aml_dtvdm_read_status(struct dvb_frontend *fe,
					enum fe_status *status)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	int ret = 0;

	if (nmode == AM_FE_UNKNOWN_N)
		return 0;

	if (is_not_active(fe)) {
		PR_DBGL("read status:not active\n");
		return 0;
	}

	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		ret = gxtv_demod_dvbc_read_status_timer(fe, status);
		break;
	case AM_FE_OFDM_N:
		ret = gxtv_demod_dvbt_read_status(fe, status);
		break;
	case AM_FE_ISDBT_N:
		ret = gxtv_demod_dvbt_read_status(fe, status);
		break;
	case AM_FE_ATSC_N:
		ret = gxtv_demod_atsc_read_status(fe, status);
		break;
	case AM_FE_DTMB_N:
		ret = gxtv_demod_dtmb_read_status(fe, status);
		break;
	/*case AM_FE_UNKNOWN_N:*/
	default:

		break;
	}
	return ret;

}
static int aml_dtvdm_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("read ber:not active\n");
		return 0;
	}

	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		ret = gxtv_demod_dvbc_read_ber(fe, ber);
		break;
	case AM_FE_OFDM_N:
		ret = gxtv_demod_dvbt_read_ber(fe, ber);
		break;
	case AM_FE_ISDBT_N:	/*same as dvbt*/
		ret = gxtv_demod_dvbt_read_ber(fe, ber);
		break;
	case AM_FE_ATSC_N:
		ret = gxtv_demod_atsc_read_ber(fe, ber);
		break;
	case AM_FE_DTMB_N:
		ret = gxtv_demod_dtmb_read_ber(fe, ber);
		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}
	return ret;

}

static int aml_dtvdm_read_signal_strength(struct dvb_frontend *fe,
					 u16 *strength)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("read strength:not active\n");
		return 0;
	}

	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		ret = gxtv_demod_dvbc_read_signal_strength(fe, strength);
		break;
	case AM_FE_OFDM_N:
		ret = gxtv_demod_dvbt_read_signal_strength(fe, strength);
		break;
	case AM_FE_ISDBT_N:	/*same as dvbt*/
		ret = gxtv_demod_dvbt_read_signal_strength(fe, strength);
		break;
	case AM_FE_ATSC_N:
		ret = gxtv_demod_atsc_read_signal_strength(fe, strength);
		break;
	case AM_FE_DTMB_N:
		ret = gxtv_demod_dtmb_read_signal_strength(fe, strength);
		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}
	return ret;

}
static int aml_dtvdm_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("read snr :not active\n");
		return 0;
	}

	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		ret = gxtv_demod_dvbc_read_snr(fe, snr);
		break;
	case AM_FE_OFDM_N:
		ret = gxtv_demod_dvbt_read_snr(fe, snr);
		break;
	case AM_FE_ISDBT_N:
		ret = gxtv_demod_dvbt_read_snr(fe, snr);
		break;
	case AM_FE_ATSC_N:
		ret = gxtv_demod_atsc_read_snr(fe, snr);
		break;
	case AM_FE_DTMB_N:
		ret = gxtv_demod_dtmb_read_snr(fe, snr);
		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}
	return ret;

}
static int aml_dtvdm_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	int ret = 0;

	if (is_not_active(fe)) {
		PR_DBGL("read ucblocks :not active\n");
		return 0;
	}

	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		ret = gxtv_demod_dvbc_read_ucblocks(fe, ucblocks);
		break;
	case AM_FE_OFDM_N:
		ret = gxtv_demod_dvbt_read_ucblocks(fe, ucblocks);
		break;
	case AM_FE_ISDBT_N:
		ret = gxtv_demod_dvbt_read_ucblocks(fe, ucblocks);
		break;
	case AM_FE_ATSC_N:
		ret = gxtv_demod_atsc_read_ucblocks(fe, ucblocks);
		break;
	case AM_FE_DTMB_N:
		ret = gxtv_demod_dtmb_read_ucblocks(fe, ucblocks);
		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}
	return ret;

}
static void aml_dtvdm_release(struct dvb_frontend *fe)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;

	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		gxtv_demod_dvbc_release(fe);
		break;
	case AM_FE_OFDM_N:
	case AM_FE_ISDBT_N:
		gxtv_demod_dvbt_release(fe);
		break;
	case AM_FE_ATSC_N:
		gxtv_demod_atsc_release(fe);
		break;
	case AM_FE_DTMB_N:
		gxtv_demod_dtmb_release(fe);
		break;
	case AM_FE_UNKNOWN_N:
	default:

		break;
	}

	if (get_dtvpll_init_flag()) {
		PR_INFO("%s\n", __func__);
		leave_mode(nmode);
		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
		dtvdd_devp->last_delsys = SYS_UNDEFINED;
		dtvdd_devp->n_mode = AM_FE_UNKNOWN_N;
	}
}


static int aml_dtvdm_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status)
{
	enum aml_fe_n_mode_t nmode = dtvdd_devp->n_mode;
	int ret = 0;
	static int flg;	/*debug only*/

	if (re_tune)
		;//delsys_confirm(fe);

	if (nmode == AM_FE_UNKNOWN_N) {
		*delay = HZ * 5;
		*status = 0;
		return 0;
	}

	if (is_not_active(fe)) {
		*status = 0;
		PR_DBGL("tune :not active\n");
		return 0;
	}

	if ((flg > 0) && (flg < 5))
		PR_INFO("%s\n", __func__);


	switch (nmode) {
	case AM_FE_QPSK_N:

		break;

	case AM_FE_QAM_N:
		gxtv_demod_dvbc_tune(fe, re_tune, mode_flags,
						delay, status);

		break;
	case AM_FE_OFDM_N:
	case AM_FE_ISDBT_N:
		ret = gxtv_demod_dvbt_tune(fe, re_tune, mode_flags,
						delay, status);
		break;
	case AM_FE_ATSC_N:
		ret = gxtv_demod_atsc_tune(fe, re_tune, mode_flags,
						delay, status);
		flg++;
		break;
	case AM_FE_DTMB_N:
		ret = gxtv_demod_dtmb_tune(fe, re_tune, mode_flags,
						delay, status);

		break;
	/*case AM_FE_UNKNOWN_N:*/
	default:
		flg = 0;
		break;
	}

	return ret;

}
static int aml_dtvdm_set_property(struct dvb_frontend *dev,
			struct dtv_property *tvp)
{
	int r = 0;
	u32 delsys;

	switch (tvp->cmd) {
	case DTV_DELIVERY_SYSTEM:
		delsys = tvp->u.data;
		delsys_set(dev, delsys);
		break;

	default:
		break;
	}


	return r;

}
static int aml_dtvdm_get_property(struct dvb_frontend *dev,
			struct dtv_property *tvp)
{
	return 0;
}

static struct dvb_frontend_ops aml_dtvdm_gxtvbb_ops = {
	.delsys = { SYS_DVBC_ANNEX_A, SYS_DTMB},
	.info = {
		/*in aml_fe, it is 'amlogic dvb frontend' */
		.name = "amlogic dtv demod txlx",
		.frequency_min      = 51000000,
		.frequency_max      = 900000000,
		.frequency_stepsize = 0,
		.frequency_tolerance = 0,		/**/
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS
	},
	.init                 = aml_dtvdm_init,
	.sleep                = aml_dtvdm_sleep,
	.set_frontend         = aml_dtvdm_set_parameters,
	.get_frontend         = aml_dtvdm_get_frontend,
	.get_tune_settings    = aml_dtvdm_get_tune_settings,
	.read_status          = aml_dtvdm_read_status,
	.read_ber             = aml_dtvdm_read_ber,
	.read_signal_strength = aml_dtvdm_read_signal_strength,
	.read_snr             = aml_dtvdm_read_snr,
	.read_ucblocks        = aml_dtvdm_read_ucblocks,
	.release              = aml_dtvdm_release,
	.set_property         = aml_dtvdm_set_property,
	.get_property	      = aml_dtvdm_get_property,

/*-------------*/
	.tune			= aml_dtvdm_tune,
	.get_frontend_algo	= gxtv_demod_atsc_get_frontend_algo,

};

static struct dvb_frontend_ops aml_dtvdm_txl_ops = {
	.delsys = { SYS_DVBC_ANNEX_A, SYS_DTMB, SYS_ANALOG},
	.info = {
		/*in aml_fe, it is 'amlogic dvb frontend' */
		.name = "amlogic dtv demod txl",
		.frequency_min      = 51000000,
		.frequency_max      = 900000000,
		.frequency_stepsize = 0,
		.frequency_tolerance = 0,		/**/
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS
	},
	.init                 = aml_dtvdm_init,
	.sleep                = aml_dtvdm_sleep,
	.set_frontend         = aml_dtvdm_set_parameters,
	.get_frontend         = aml_dtvdm_get_frontend,
	.get_tune_settings    = aml_dtvdm_get_tune_settings,
	.read_status          = aml_dtvdm_read_status,
	.read_ber             = aml_dtvdm_read_ber,
	.read_signal_strength = aml_dtvdm_read_signal_strength,
	.read_snr             = aml_dtvdm_read_snr,
	.read_ucblocks        = aml_dtvdm_read_ucblocks,
	.release              = aml_dtvdm_release,
	.set_property         = aml_dtvdm_set_property,
	.get_property	      = aml_dtvdm_get_property,

/*-------------*/
	.tune			= aml_dtvdm_tune,
	.get_frontend_algo	= gxtv_demod_atsc_get_frontend_algo,

};

static struct dvb_frontend_ops aml_dtvdm_txlx_ops = {
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	.delsys = { SYS_ATSC, SYS_DVBC_ANNEX_B,  SYS_DVBC_ANNEX_A, SYS_DVBT,
		SYS_ANALOG, SYS_ISDBT},
#else
	.delsys = { SYS_ATSC, SYS_DVBC_ANNEX_B,  SYS_DVBC_ANNEX_A, SYS_DVBT},
#endif
	.info = {
		/*in aml_fe, it is 'amlogic dvb frontend' */
		.name = "amlogic dtv demod txlx",
		.frequency_min      = 51000000,
		.frequency_max      = 900000000,
		.frequency_stepsize = 0,
		.frequency_tolerance = 0,		/**/
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS
	},
	.init                 = aml_dtvdm_init,
	.sleep                = aml_dtvdm_sleep,
	.set_frontend         = aml_dtvdm_set_parameters,
	.get_frontend         = aml_dtvdm_get_frontend,
	.get_tune_settings    = aml_dtvdm_get_tune_settings,
	.read_status          = aml_dtvdm_read_status,
	.read_ber             = aml_dtvdm_read_ber,
	.read_signal_strength = aml_dtvdm_read_signal_strength,
	.read_snr             = aml_dtvdm_read_snr,
	.read_ucblocks        = aml_dtvdm_read_ucblocks,
	.release              = aml_dtvdm_release,
	.set_property         = aml_dtvdm_set_property,
	.get_property	      = aml_dtvdm_get_property,

/*-------------*/
	.tune			= aml_dtvdm_tune,
	.get_frontend_algo	= gxtv_demod_txlx_get_frontend_algo,

};

static struct dvb_frontend_ops aml_dtvdm_gxlx_ops = {
	.delsys = { SYS_DVBC_ANNEX_A },
	.info = {
		/*in aml_fe, it is 'amlogic dvb frontend' */
		.name = "amlogic dtv demod txlx",
		.frequency_min      = 51000000,
		.frequency_max      = 900000000,
		.frequency_stepsize = 0,
		.frequency_tolerance = 0,		/**/
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS
	},
	.init                 = aml_dtvdm_init,
	.sleep                = aml_dtvdm_sleep,
	.set_frontend         = aml_dtvdm_set_parameters,
	.get_frontend         = aml_dtvdm_get_frontend,
	.get_tune_settings    = aml_dtvdm_get_tune_settings,
	.read_status          = aml_dtvdm_read_status,
	.read_ber             = aml_dtvdm_read_ber,
	.read_signal_strength = aml_dtvdm_read_signal_strength,
	.read_snr             = aml_dtvdm_read_snr,
	.read_ucblocks        = aml_dtvdm_read_ucblocks,
	.release              = aml_dtvdm_release,
	.set_property         = aml_dtvdm_set_property,
	.get_property	      = aml_dtvdm_get_property,

/*-------------*/
	.tune			= aml_dtvdm_tune,
	.get_frontend_algo	= gxtv_demod_atsc_get_frontend_algo,

};

static struct dvb_frontend_ops aml_dtvdm_txhd_ops = {
	.delsys = { SYS_DTMB },
	.info = {
		/*in aml_fe, it is 'amlogic dvb frontend' */
		.name = "amlogic dtv demod txlx",
		.frequency_min      = 51000000,
		.frequency_max      = 900000000,
		.frequency_stepsize = 0,
		.frequency_tolerance = 0,		/**/
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS
	},
	.init                 = aml_dtvdm_init,
	.sleep                = aml_dtvdm_sleep,
	.set_frontend         = aml_dtvdm_set_parameters,
	.get_frontend         = aml_dtvdm_get_frontend,
	.get_tune_settings    = aml_dtvdm_get_tune_settings,
	.read_status          = aml_dtvdm_read_status,
	.read_ber             = aml_dtvdm_read_ber,
	.read_signal_strength = aml_dtvdm_read_signal_strength,
	.read_snr             = aml_dtvdm_read_snr,
	.read_ucblocks        = aml_dtvdm_read_ucblocks,
	.release              = aml_dtvdm_release,
	.set_property         = aml_dtvdm_set_property,
	.get_property	      = aml_dtvdm_get_property,

/*-------------*/
	.tune			= aml_dtvdm_tune,
	.get_frontend_algo	= gxtv_demod_atsc_get_frontend_algo,

};

static struct dvb_frontend_ops aml_dtvdm_tl1_ops = {
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	.delsys = {SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_B, SYS_ATSC, SYS_DTMB,
		SYS_ANALOG},
#else
	.delsys = { SYS_ATSC, SYS_DVBC_ANNEX_B,  SYS_DVBC_ANNEX_A, SYS_DVBT},
#endif
	.info = {
		/*in aml_fe, it is 'amlogic dvb frontend' */
		.name = "amlogic dtv demod tl1",
		.frequency_min      = 51000000,
		.frequency_max      = 900000000,
		.frequency_stepsize = 0,
		.frequency_tolerance = 0,		/**/
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS
	},
	.init                 = aml_dtvdm_init,
	.sleep                = aml_dtvdm_sleep,
	.set_frontend         = aml_dtvdm_set_parameters,
	.get_frontend         = aml_dtvdm_get_frontend,
	.get_tune_settings    = aml_dtvdm_get_tune_settings,
	.read_status          = aml_dtvdm_read_status,
	.read_ber             = aml_dtvdm_read_ber,
	.read_signal_strength = aml_dtvdm_read_signal_strength,
	.read_snr             = aml_dtvdm_read_snr,
	.read_ucblocks        = aml_dtvdm_read_ucblocks,
	.release              = aml_dtvdm_release,
	.set_property         = aml_dtvdm_set_property,
	.get_property	      = aml_dtvdm_get_property,

/*-------------*/
	.tune			= aml_dtvdm_tune,
	.get_frontend_algo	= gxtv_demod_txlx_get_frontend_algo,

};

struct dvb_frontend *aml_dtvdm_attach(const struct amlfe_exp_config *config)
{
	int ic_version = get_ic_ver();


	/*mem setting is in prob*/

	struct dvb_frontend *fe = &dtvdd_devp->frontend;

	/* mem of dvb_frontend is define in aml_fe*/

	switch (ic_version) {

	case IC_VER_GTVBB:
		memcpy(&fe->ops, &aml_dtvdm_gxtvbb_ops,
				       sizeof(struct dvb_frontend_ops));
		break;
	case IC_VER_TXL:
		memcpy(&fe->ops, &aml_dtvdm_txl_ops,
		       sizeof(struct dvb_frontend_ops));
		break;
	case IC_VER_TXLX:
		memcpy(&fe->ops, &aml_dtvdm_txlx_ops,
		       sizeof(struct dvb_frontend_ops));
		break;
	case IC_VER_GXLX:
		memcpy(&fe->ops, &aml_dtvdm_gxlx_ops,
		       sizeof(struct dvb_frontend_ops));
		break;
	case IC_VER_TXHD:
		memcpy(&fe->ops, &aml_dtvdm_txhd_ops,
				       sizeof(struct dvb_frontend_ops));
		break;
	case IC_VER_TL1:
		memcpy(&fe->ops, &aml_dtvdm_tl1_ops,
		       sizeof(struct dvb_frontend_ops));
		break;
	default:
		PR_ERR("attach fail! ic=%d\n", ic_version);
		/*return NULL;*/
		fe = NULL;
		break;
	}
		/* mem of dvb_frontend is define in aml_fe*/

	dtvdd_devp->last_delsys = SYS_UNDEFINED;


	return fe;
}
EXPORT_SYMBOL(aml_dtvdm_attach);

/*-------------------------*/
static struct aml_exp_func aml_exp_ops = {
	.leave_mode = leave_mode,
};

struct aml_exp_func *aml_dtvdm_exp_attach(struct aml_exp_func *exp)
{
	if (exp) {
		memcpy(exp, &aml_exp_ops, sizeof(struct aml_exp_func));
	} else {
		PR_ERR("%s:fail!\n", __func__);
		return NULL;

	}
	return exp;
}
EXPORT_SYMBOL(aml_dtvdm_exp_attach);

/*-------------------------*/
struct dvb_frontend *aml_get_fe(void)
{

	return &dtvdd_devp->frontend;

}

void aml_exp_attach(struct aml_exp_func *afe)
{

}
EXPORT_SYMBOL(aml_exp_attach);

/*=======================================*/

fs_initcall(aml_dtvdemod_init);
module_exit(aml_dtvdemod_exit);

MODULE_DESCRIPTION("gxtv_demod DVB-T/DVB-C/DTMB Demodulator driver");
MODULE_AUTHOR("RSJ");
MODULE_LICENSE("GPL");
