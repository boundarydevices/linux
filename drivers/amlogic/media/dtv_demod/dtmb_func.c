/*
 * drivers/amlogic/media/dtv_demod/dtmb_func.c
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

#include "addr_dtmb_front_bit.h"
/*#include "reg_dtmb.h"*/

#include "demod_func.h"

#if 0
#ifdef pr_dbg
undef pr_dbg
#endif

#define pr_dbg		aml_dbgdtmb
#endif

static int dtmb_spectrum = 2;
MODULE_PARM_DESC(demod_enable_performance, "\n\t\t demod_enable_performance information");
static int demod_enable_performance = 1;
module_param(demod_enable_performance, int, 0644);

MODULE_PARM_DESC(demod_sync_count, "\n\t\t timeout debug information");
static int demod_sync_count = 60;
module_param(demod_sync_count, int, 0644);

MODULE_PARM_DESC(demod_sync_delay_time, "\n\t\t timeout debug information");
static int demod_sync_delay_time = 8;
module_param(demod_sync_delay_time, int, 0644);


MODULE_PARM_DESC(demod_timeout, "\n\t\t timeout debug information");
static int demod_timeout = 120;
module_param(demod_timeout, int, 0644);


void dtmb_set_fe_config_modify(unsigned int modify)
{
	union DTMB_SYNC_FE_CONFIG_BITS fe_cofig;

	fe_cofig.d32 = dtmb_read_reg(DTMB_SYNC_FE_CONFIG);
	fe_cofig.b.fe_modify = modify;

	dtmb_write_reg(DTMB_SYNC_FE_CONFIG, fe_cofig.d32);
	PR_DTMB("set modiy=0x%x,0x%x\n", modify, fe_cofig.d32);
}
/* formula: fs(MHz)
 *  2*7.56*2^23/fs/256
 *	24MHz: 0x50a3
 *	25MHz: 0x4d6a
 */
void dtmb_clk_set(unsigned int adc_clk)
{
	unsigned int fe_modify = 0x4d6a;

	if (adc_clk)
		fe_modify = 3963617280UL / (adc_clk << 3);

	dtmb_set_fe_config_modify(fe_modify);

}
#if 0
void dtmb_clk_set(unsigned int adc_clk)
{
	unsigned int fe_modify = 0x4d6a;

	switch (adc_clk) {
	case Adc_Clk_24M:
		fe_modify = 0x50a3;
		break;
	case Adc_Clk_25M:
		fe_modify = 0x4d6a;
		break;
	default:
		pr_error("error:%s:not support,adc_clk=%d\n",
				__func__, adc_clk);
		fe_modify = 0x4d6a;
		break;
	}
	dtmb_set_fe_config_modify(fe_modify);
}
#endif

void dtmb_all_reset(void)
{
	int temp_data = 0;

	if (is_ic_ver(IC_VER_TXL)) {
		/*fix bug 139044: DTMB lost sync*/
		/*dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x1f);*/
		dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x22);
		/*modified bu xiaotong*/
		dtmb_write_reg(DTMB_CHE_TPS_CONFIG, 0xc00000);
		dtmb_write_reg(DTMB_CHE_EQ_CONFIG, 0x1a027719);
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG1, 0x101a7);
		/*21bit set ddr access urgent*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x331a31);
		/*detect 64qam 420 595 problems*/
		dtmb_write_reg(DTMB_FRONT_19_CONFIG, 0x300);
		dtmb_write_reg(DTMB_FRONT_4d_CONFIG, 0x12ffbe0);
		/*fix fsm b bug*/
		dtmb_write_reg(DTMB_FRONT_DEBUG_CFG, 0x5680000);
		/*fix agc problem,skip warm_up status*/
		dtmb_write_reg(DTMB_FRONT_46_CONFIG, 0x1a000f0f);
		dtmb_write_reg(DTMB_FRONT_ST_FREQ, 0xf2400000);
		dtmb_clk_set(Adc_Clk_25M);
	} else if (is_ic_ver(IC_VER_TXHD)) {
		/* dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x1f); */
		dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x1e);

		/*demod can't sync when freq offset >1.5KHz for air signal*/
		dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x6aaaaa);
		dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x13196596);

		/*modified bu xiaotong*/
		dtmb_write_reg(DTMB_CHE_TPS_CONFIG, 0xc00000);
		dtmb_write_reg(DTMB_CHE_EQ_CONFIG, 0x1a027719);
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG1, 0x101a7);
		/*21bit set ddr access urgent*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x331a31);
		/*detect 64qam 420 595 problems*/
		dtmb_write_reg(DTMB_FRONT_19_CONFIG, 0x300);
		dtmb_write_reg(DTMB_FRONT_4d_CONFIG, 0x12ffbe0);
		/*fix fsm b bug*/
		dtmb_write_reg(DTMB_FRONT_DEBUG_CFG, 0x5680000);
		/*fix agc problem,skip warm_up status*/
		dtmb_write_reg(DTMB_FRONT_46_CONFIG, 0x1a000f0f);
		dtmb_write_reg(DTMB_FRONT_ST_FREQ, 0xf2400000);
		dtmb_clk_set(Adc_Clk_24M);
	} else if (is_ic_ver(IC_VER_TL1)) {
		if (demod_get_adc_clk() == Adc_Clk_24M) {
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x6aaaaa);
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x13196596);
			dtmb_write_reg(0x5b << 2, 0x50a30a25);
		} else if (demod_get_adc_clk() == Adc_Clk_25M) {
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x62c1a5);
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x131a747d);
			dtmb_write_reg(0x5b << 2, 0x4d6a0a25);
		}

		//for timeshift issue(chuangcheng test)
		dtmb_write_reg(0x4e << 2, 0x256cf604);
	} else {
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG1, 0x10127);
		dtmb_write_reg(DTMB_CHE_IBDFE_CONFIG6, 0x943228cc);
		dtmb_write_reg(DTMB_CHE_IBDFE_CONFIG7, 0xc09aa8cd);
		dtmb_write_reg(DTMB_CHE_FD_TD_COEFF, 0x0);
		dtmb_write_reg(DTMB_CHE_EQ_CONFIG, 0x9dc59);
		/*0x2 is auto,0x406 is  invert spectrum*/
		if (dtmb_spectrum == 0)
			dtmb_write_reg(DTMB_TOP_CTRL_TPS, 0x406);
		else if (dtmb_spectrum == 1)
			dtmb_write_reg(DTMB_TOP_CTRL_TPS, 0x402);
		else
			dtmb_write_reg(DTMB_TOP_CTRL_TPS, 0x2);

		PR_DTMB("dtmb_spectrum is %d\n", dtmb_spectrum);
		dtmb_write_reg(DTMB_TOP_CTRL_FEC, 0x41444400);
		dtmb_write_reg(DTMB_TOP_CTRL_INTLV_TIME, 0x180300);
		dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x662ca0);
		dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x29);
		dtmb_write_reg(DTMB_FRONT_DC_HOLD, 0xa1066);
		/*cci para*/
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG3, 0x80201f6);
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG2, 0x3f20080);
		dtmb_write_reg(DTMB_CHE_TPS_CONFIG, 0xc00000);
		dtmb_write_reg(DTMB_TOP_CTRL_AGC, 0x3);
		dtmb_write_reg(DTMB_TOP_CTRL_TS_SFO_CFO, 0x20403006);
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG2, 0x7200a16);
		dtmb_write_reg(DTMB_FRONT_DEBUG_CFG, 0x1e00000);
		dtmb_write_reg(DTMB_TOP_CTRL_ENABLE, 0x7fffff);
		/*close ts3 timing loop*/
		dtmb_write_reg(DTMB_TOP_CTRL_DAGC_CCI, 0x305);
		/*dektec card issue,close f case snr drop*/
		dtmb_write_reg(DTMB_CHE_MC_SC_TIMING_POWTHR, 0xc06100a);
		if (demod_enable_performance) {
			dtmb_write_reg(DTMB_CHE_IBDFE_CONFIG1, 0x4040002);
			temp_data = dtmb_read_reg(DTMB_CHE_FD_TD_COEFF);
		    temp_data = (temp_data & ~0x3fff)|(0x241f & 0x3fff);
		    temp_data = temp_data | (1<<21);
			/*Set freeze_mode and reset coeff*/
		    dtmb_write_reg(DTMB_CHE_FD_TD_COEFF, temp_data);
		    temp_data = temp_data & ~(1<<21);
			/*Set freeze_mode and reset coeff*/
		    dtmb_write_reg(DTMB_CHE_FD_TD_COEFF, temp_data);
		}
	}
}

void dtmb_initial(struct aml_demod_sta *demod_sta)
{
/* dtmb_write_reg(0x049, memstart);		//only for init */
	/*dtmb_spectrum = 1; no use */
	dtmb_spectrum = demod_sta->spectrum;
	dtmb_register_reset();
	dtmb_all_reset();
}

int check_dtmb_fec_lock(void)
{
	int fec_lock, snr, status;
	/*fec_lock = (dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) >> 14) & 0x1;*/
	fec_lock = dtmb_reg_r_fec_lock();
	/*snr = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) & 0x3fff;*/
	snr = dtmb_reg_r_che_snr();
	if (fec_lock && (snr > 4))
		status = 1;
	else
		status = 0;
	return status;
}

int check_dtmb_mobile_det(void)
{
	int mobile_det = 0;

	mobile_det = (dtmb_read_reg(DTMB_TOP_CTRL_SYS_OFDM_CNT) >> 8) & 0x7ffff;
	return mobile_det;

}


int dtmb_information(void)
{
	int tps, snr, fec_lock, fec_bch_add, fec_ldpc_unc_acc, fec_ldpc_it_avg,
	    /*tmp,*/ che_snr;


	unsigned int buf[3]; /**/

	struct dvb_frontend *fe = aml_get_fe();

	tps = dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT);

	che_snr = dtmb_reg_r_che_snr();
	snr = che_snr;
	snr = convert_snr(snr);
	/*	if (che_snr >= 8192) */
	/*		che_snr = che_snr - 16384;*/
	/*	snr = che_snr / 32;*/
	/* snr = 10*log10(snr)-6; */
	/* fec_lock = (dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) >> 14) & 0x1; */
	fec_lock = dtmb_reg_r_fec_lock();
	fec_bch_add = dtmb_reg_r_bch();
	fec_ldpc_unc_acc = dtmb_read_reg(DTMB_TOP_FEC_LDPC_UNC_ACC);
	fec_ldpc_it_avg = dtmb_read_reg(DTMB_TOP_FEC_LDPC_IT_AVG);
	PR_DTMB("[FSM] : %x %x %x %x\n",
	       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0),
	       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE1),
	       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE2),
	       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE3));
	/* PR_DTMB*/
	/*    ("[AGC]: agc_power %d,agc_if_gain %d,agc_rf_gain %d,",*/
	/*     (-(((dtmb_read_reg(DTMB_TOP_FRONT_AGC) >> 22) & 0x3ff) / 16)),*/
	/*     ((dtmb_read_reg(DTMB_TOP_FRONT_AGC)) & 0x3ff),*/
	/*    ((dtmb_read_reg(DTMB_TOP_FRONT_AGC) >> 11) & 0x7ff));*/

	dtmb_read_agc(DTMB_D9_ALL, &buf[0]);
	PR_DTMB
	    ("[AGC]: agc_power %d,agc_if_gain %d,agc_rf_gain %d,",
	     (-((buf[2]) / 16)),
	     buf[0],
	     buf[1]);

	PR_DTMB
	      ("dagc_power %3d,dagc_gain %3d mobi_det_power %d\n",
	      ((dtmb_read_reg(DTMB_TOP_FRONT_DAGC) >> 0) & 0xff),
	     ((dtmb_read_reg(DTMB_TOP_FRONT_DAGC) >> 8) & 0xfff),
	     (dtmb_read_reg(DTMB_TOP_CTRL_SYS_OFDM_CNT) >> 8) & 0x7ffff);
	PR_DTMB
	    ("[TPS] SC or MC %2d,f_r %2d qam_nr %2d ",
	     (dtmb_read_reg(DTMB_TOP_CHE_OBS_STATE1) >> 1) & 0x1,
	     (tps >> 22) & 0x1, (tps >> 21) & 0x1);
	PR_DTMB
		("intlv %2d,cr %2d constl %2d\n",
		(tps >> 20) & 0x1,
	     (tps >> 18) & 0x3, (tps >> 16) & 0x3);

	PR_DTMB
	    ("[dtmb] snr is %d,fec_lock is %d,fec_bch_add is %d,",
	     snr, fec_lock, fec_bch_add);
	PR_DTMB
	    ("fec_ldpc_unc_acc is %d ,fec_ldpc_it_avg is %d\n",
	     fec_ldpc_unc_acc,
	     fec_ldpc_it_avg / 256);
	PR_DTMB
	    ("------------------------------------------------------------\n");

	tuner_get_ch_power(fe);

	return 0;
}

int dtmb_check_cci(void)
{
	int cci_det = 0;

	cci_det =
	((dtmb_read_reg(DTMB_TOP_SYNC_CCI_NF2_POSITION) >> 22)
		& 0x3);

	if (cci_det > 0) {
		PR_DTMB("find cci\n");
		dtmb_write_reg(DTMB_CHE_CCIDET_CONFIG, 0x20210290);
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG3, 0x20081f6);
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG2, 0x3f08020);
	}
	return cci_det;
}

int dtmb_bch_check(void)
{
	int fec_bch_add, i;
	char *info1 = "fec lock,but bch add ,need reset,wait not to reset";
	char *info2 = "fec lock,but bch add ,need reset,now is lock";

	fec_bch_add = dtmb_reg_r_bch();

	/*PR_DTMB("[debug]fec lock,fec_bch_add is %d\n", fec_bch_add);*/
	msleep(100);
	if ((dtmb_reg_r_bch()-fec_bch_add) >= 50) {
		PR_DTMB("%s\n", info1);
		dtmb_reset();
		for (i = 0; i < 30; i++) {
			msleep(100);
			if (check_dtmb_fec_lock() == 1) {
				PR_DTMB("%s\n", info2);
				return 0;
			}
		}
	}
	return 0;
}

int dtmb_constell_check(void)
{
	int constell;

	constell = dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT)>>16 & 0x3;
	if (constell == 0)/*4qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x333221);
	else if (constell == 1)/*16qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x332821);
	else if (constell == 2)/*32qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x331e21);
	else if (constell == 3)/*64qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x331a31);

	return 0;
}


int dtmb_check_fsm(void)
{
	int tmp, fsm_status, i, has_signal;

	tmp = dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0);
	fsm_status =  tmp&0xffffffff;
	has_signal = 0;
	PR_DTMB("fsm_status is %x\n", fsm_status);
	for (i = 0 ; i < 8 ; i++) {
		if (((fsm_status >> (i*4)) & 0xf) > 3) {
			/*has signal*/
		/*	PR_DTMB("has signal\n");*/
			has_signal = 1;
		}
	}
	return has_signal;

}

int patch_ts3(int delay1_us, int delay2_us)
{
	if (((dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0)&0xf) == 0x7)&1) {
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x300f);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x310f);
		msleep(delay1_us);
		dtmb_write_reg(DTMB_TOP_CTRL_ENABLE, 0xffdfff);
		dtmb_write_reg(DTMB_TOP_CTRL_ENABLE, 0xffffff);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x3110);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x3010);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x3000);
		return 1;
	} else
		return 0;
}


int read_cfo_all(void)
{
	int icfo_all, fcfo_all;

	icfo_all = dtmb_read_reg(DTMB_TOP_CTRL_ICFO_ALL) & 0xfffff;
	fcfo_all = dtmb_read_reg(DTMB_TOP_CTRL_FCFO_ALL) & 0x3fff;
	if (icfo_all > (1 << 19))
		icfo_all = icfo_all - (1 << 20);
	if (fcfo_all > (1 << 13))
		fcfo_all = fcfo_all - (1 << 14);

	return (int)(icfo_all*4+fcfo_all);

}


int dtmb_v3_soft_sync(int cfo_init)
{

/*	int cfo_all;*/
/*	int cfo_setting;*/

	if (cfo_init == 0) {
		cfo_init = patch_ts3(11, 0);
		#if 0
		if (cfo_init == 1) {
			cfo_all = read_cfo_all();
			cfo_setting = dtmb_read_reg(DTMB_FRONT_DDC_BYPASS);
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS,
			cfo_setting+cfo_all);
			dtmb_write_reg(DTMB_TOP_CTRL_LOOP, 0x3);
			dtmb_reset();
		}
		#endif
	}
	return cfo_init;

}

int dtmb_check_status_gxtv(struct dvb_frontend *fe)
{
	int local_state;
	int time_cnt;/* cci_det, src_config;*/
	int cfo_init, count;

	dtmb_information();
	time_cnt = 0;
	local_state = 0;
	cfo_init = 0;
	if (check_dtmb_fec_lock() != 1) {
		dtmb_register_reset();
		dtmb_all_reset();
		count = 15;
		while ((count) &&
		((dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0)&0xf) < 0x6)) {
			msleep(20);
			count--;
		}

		count = demod_sync_count;
		while ((count) && (cfo_init == 0)) {

			cfo_init = dtmb_v3_soft_sync(cfo_init);

			msleep(demod_sync_delay_time);
			count--;
		}
		if ((cfo_init == 0) &&
			((dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0)&0xf) <= 7)) {
			PR_DTMB("over 400ms,status is %x, need reset\n",
				(dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0)&0xf));
			return 0;
		}
		while ((time_cnt < 10) && (check_dtmb_fec_lock() != 1)) {
			msleep(demod_timeout);
			time_cnt++;
			local_state = AMLOGIC_DTMB_STEP3;
			dtmb_information();
			dtmb_check_cci();
			if (time_cnt > 8)
				PR_DTMB
					("* local_state = %d\n", local_state);
		}
		if (time_cnt >= 10 && (check_dtmb_fec_lock() != 1)) {
			local_state = AMLOGIC_DTMB_STEP4;
			time_cnt = 0;
			PR_DTMB
				("*all reset,timeout is %d\n", demod_timeout);
		}
	} else {
		dtmb_check_cci();
		dtmb_bch_check();
	#if 0
		cci_det = dtmb_check_cci();
		if ((check_dtmb_mobile_det() <= demod_mobile_power)
				&& (cci_det == 0)) {
			/* open */
			src_config = (dtmb_read_reg(DTMB_FRONT_SRC_CONFIG1));
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1,
				src_config & (~(0x1 << 28)));
		} else {
			/* close */
			src_config = (dtmb_read_reg(DTMB_FRONT_SRC_CONFIG1));
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1,
				src_config | (0x1 << 28));
		}
	#endif
	}
	if (check_dtmb_fec_lock() == 1)
		dtmb_write_reg(DTMB_TOP_CTRL_LOOP, 0xf);
	return 0;
}


int dtmb_check_status_txl(struct dvb_frontend *fe)
{
	int time_cnt;

	time_cnt = 0;
	dtmb_information();
	if (check_dtmb_fec_lock() != 1) {
		while ((time_cnt < 10) && (check_dtmb_fec_lock() != 1)) {
			msleep(demod_timeout);
			time_cnt++;
			dtmb_information();
			if (((dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT)
				>> 21) & 0x1) == 0x1) {
				PR_DTMB("4qam-nr,need set spectrum\n");
				if (dtmb_spectrum == 1) {
					dtmb_write_reg
					(DTMB_TOP_CTRL_TPS, 0x1010406);
				} else if (dtmb_spectrum == 0) {
					dtmb_write_reg
					(DTMB_TOP_CTRL_TPS, 0x1010402);
				} else {
					dtmb_write_reg
					(DTMB_TOP_CTRL_TPS, 0x1010002);
				}
			}
			if (time_cnt > 8)
				PR_DTMB("* time_cnt = %d\n", time_cnt);
		}
		if (time_cnt >= 10 && (check_dtmb_fec_lock() != 1)) {
			time_cnt = 0;
			dtmb_register_reset();
			dtmb_all_reset();
			if	(dtmb_spectrum == 0)
				dtmb_spectrum = 1;
			else
				dtmb_spectrum = 0;
			PR_DTMB("*all reset,timeout is %d\n", demod_timeout);
		}
	} else {
		dtmb_bch_check();
		dtmb_constell_check();
	}
	return 0;
}

#ifdef DVB_CORE_ORI



void dtmb_no_signal_check_v3(void)
{

	if (((dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT)
		>> 21) & 0x1) == 0x1) {
		PR_DTMB("4qam-nr,need set spectrum\n");
		if (dtmb_spectrum == 1) {
			dtmb_write_reg
			(DTMB_TOP_CTRL_TPS, 0x1010406);
		} else if (dtmb_spectrum == 0) {
			dtmb_write_reg
			(DTMB_TOP_CTRL_TPS, 0x1010402);
		} else {
			dtmb_write_reg
			(DTMB_TOP_CTRL_TPS, 0x1010002);
		}
	}

}
void dtmb_no_signal_check_finishi_v3(void)
{
	dtmb_register_reset();
	dtmb_all_reset();
	if	(dtmb_spectrum == 0)
		dtmb_spectrum = 1;
	else
		dtmb_spectrum = 0;

}



#endif
void dtmb_reset(void)
{
	union DTMB_TOP_CTRL_SW_RST_BITS sw_rst;

	sw_rst.b.ctrl_sw_rst = 1;
	sw_rst.b.ctrl_sw_rst_noreg = 1;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
	sw_rst.b.ctrl_sw_rst = 0;
	sw_rst.b.ctrl_sw_rst_noreg = 0;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
}

void dtmb_register_reset(void)
{
	union DTMB_TOP_CTRL_SW_RST_BITS sw_rst;

	sw_rst.b.ctrl_sw_rst = 1;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
	sw_rst.b.ctrl_sw_rst = 0;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
}

int dtmb_set_ch(struct aml_demod_sta *demod_sta,
		/*struct aml_demod_i2c *demod_i2c,*/
		struct aml_demod_dtmb *demod_dtmb)
{
	int ret = 0;
	u8 demod_mode;
	u8 bw, sr, ifreq, agc_mode;
	u32 ch_freq;

	bw = demod_dtmb->bw;
	sr = demod_dtmb->sr;
	ifreq = demod_dtmb->ifreq;
	agc_mode = demod_dtmb->agc_mode;
	ch_freq = demod_dtmb->ch_freq;
	demod_mode = demod_dtmb->dat0;
	demod_sta->ch_mode = demod_dtmb->mode;	/* TODO */
	demod_sta->agc_mode = agc_mode;
	demod_sta->ch_freq = ch_freq;
	demod_sta->dvb_mode = demod_mode;
	demod_sta->ch_bw = (8 - bw) * 1000;
	dtmb_initial(demod_sta);
	PR_DTMB("DTMB mode\n");
	return ret;
}

void dtmb_set_mem_st(int mem_start)
{
	PR_DTMB("[im]memstart is %x\n", mem_start);
	dtmb_write_reg(DTMB_FRONT_MEM_ADDR, mem_start);
	PR_DTMB("[dtmb]mem_buf is 0x%x\n", dtmb_read_reg(DTMB_FRONT_MEM_ADDR));
}

int dtmb_read_agc(enum REG_DTMB_D9 type, unsigned int *buf)
{
	union DTMB_TOP_FRONT_AGC_BITS rval;

	rval.d32 = dtmb_read_reg(DTMB_TOP_FRONT_AGC);

	/*PR_DTMB("%s:type=%d,val=0x%x\n", __func__, type, rval.d32);*/
	/*PR_DTMB("if=0x%x,rf=0x%x,pw=0x%x", rval.b.front_agc_if_gain,*/
	/*		rval.b.front_agc_rf_gain,*/
	/*		rval.b.front_agc_power);*/
	switch (type) {
	case DTMB_D9_IF_GAIN:
		*buf = rval.b.front_agc_if_gain;
		break;
	case DTMB_D9_RF_GAIN:
		*buf = rval.b.front_agc_rf_gain;
		break;
	case DTMB_D9_POWER:
		*buf = rval.b.front_agc_power;
		break;
	case DTMB_D9_ALL:
		buf[0] = rval.b.front_agc_if_gain;
		buf[1] = rval.b.front_agc_rf_gain;
		buf[2] = rval.b.front_agc_power;
		break;
	}

	return 0;

}

unsigned int dtmb_reg_r_fec_lock(void)
{

	union DTMB_TOP_FEC_LOCK_SNR_BITS rval;
	unsigned int fec_lock;

	rval.d32 = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR);

	if (is_dtmb_ver(IC_DTMB_V2))
		fec_lock = rval.b_v2.fec_lock;
	else
		fec_lock = rval.b.fec_lock;

	return fec_lock;
}
unsigned int dtmb_reg_r_che_snr(void)
{
	union DTMB_TOP_FEC_LOCK_SNR_BITS rval;
	unsigned int che_snr;

	rval.d32 = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR);

	if (is_dtmb_ver(IC_DTMB_V2))
		che_snr = rval.b_v2.che_snr;
	else
		che_snr = rval.b.che_snr;

	return che_snr;

}

unsigned int dtmb_reg_r_bch(void)
{
	return dtmb_read_reg(DTMB_TOP_FEC_BCH_ACC);
}

/*move from dvb-core dvb_frontend.c dvb_frontend_swzigzag*/
/*1: timeout;2:have signal*/
unsigned int dtmb_detect_first(void)
{
	int has_signal, i;
	unsigned int dtmb_status;

	unsigned int timeout = 0;

	PR_DTMB("%s\n", __func__);
	/*printk("k:%s\n",__func__);*/

	has_signal = 0;
	msleep(200);

	/*fsm status is 4,maybe analog signal*/
	dtmb_status = dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0);
	PR_DTMB("fsm_status is %x\n", dtmb_status);

	for (i = 0 ; i < 8 ; i++) {
		if (((dtmb_status >> (i*4)) & 0xf) > 4) {
			/*has signal*/
		/*	dprintk("has signal\n");*/
			has_signal = 0x1;
		}
	}

	if (has_signal == 0x1) {
		/*fsm status is 6,digital signal*/
		/*fsm (1->4) 30ms,(4->5) 20ms,*/
		/*(5->6) 10ms,(6->7) 75ms,*/
		/*(7->8) 8ms,(8->9) 55ms, (9->a) 350ms*/
		msleep(500);
		dtmb_status = dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0);
		PR_DTMB("fsm_status2 is %x\n", dtmb_status);
		for (i = 0 ; i < 8 ; i++) {
			if (((dtmb_status >> (i*4))
				& 0xf) > 6) {
				/*has signal*/
			/*	dprintk("has signal\n");*/
				has_signal = 0x3;
			}
		}
	}

	PR_DTMB("[DTV]has_signal is %d\n", has_signal);
	if ((has_signal == 0) || (has_signal == 0x1)) {
		//timeout = 1;	/*FE_TIMEDOUT;*/
		timeout = 0;	/*FE_TIMEDOUT;*/
		PR_DTMB("\t timeout\n");

	} else {
		/*timeout = 2; *//*have signal*/
		PR_DTMB("\thave signal\n");
	}

	return timeout;
}
