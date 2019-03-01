/*
 * drivers/amlogic/media/dtv_demod/tuner_func.c
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

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/dvb/aml_demod.h>
#include "demod_func.h"
/*#include "aml_fe.h"*/


/*add to replase aml_fe_analog_set_frontend*/
void tuner_set_params(struct dvb_frontend *fe)
{
	int ret = -1;

	PR_INFO("%s:\n", __func__);

	if (fe->ops.tuner_ops.set_params)
		ret = fe->ops.tuner_ops.set_params(fe);
	else
		PR_ERR("error: no tuner");


}

int tuner_get_ch_power(struct dvb_frontend *fe)
{
	int strength = 0;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	s16 strengtha = 0;
#endif

	if (fe != NULL) {
#if 0
		if (fe->ops.tuner_ops.get_strength)
			strength = fe->ops.tuner_ops.get_strength(fe);
		else
			PR_INFO("no tuner get_strength\n");
#endif
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
		if (fe->ops.tuner_ops.get_strength) {
			fe->ops.tuner_ops.get_strength(fe, &strengtha);
			strength = (int)strengtha;
		} else {
			PR_INFO("no tuner get_strength\n");
		}
#endif
	}


	return strength;
}

int tuner_get_ch_power2(void)
{

	int strength = 0;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	s16 strengtha = 0;
#endif
	struct dvb_frontend *fe;

	fe = aml_get_fe();
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	if (fe != NULL) {
		if (fe->ops.tuner_ops.get_strength) {
			fe->ops.tuner_ops.get_strength(fe, &strengtha);
			//strength = strengtha - 256;
			strength = (int)strengtha;
		} else {
			PR_INFO("no tuner get_strength\n");
		}
	}
#endif

	return strength;
}

u16 tuner_get_ch_power3(void)
{

	u16 strength = 0;
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	s16 strengtha = 0;
#endif
	struct dvb_frontend *fe;

	fe = aml_get_fe();
#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	if (fe != NULL) {

		if (fe->ops.tuner_ops.get_strength) {
			fe->ops.tuner_ops.get_strength(fe, &strengtha);
			/*from negative to positive*/
			if (strengtha < -100)
				strength = 0;
			else
				strength = strengtha + 100;

		} else {
			PR_INFO("no tuner get_strength\n");
		}
	}
#endif

	return strength;
}


struct dvb_tuner_info *tuner_get_info(int type, int mode)
{
	/*type :  0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316 */
	/*mode: 0-DVBC 1-DVBT */
	static struct dvb_tuner_info tinfo_null = { };

	static struct dvb_tuner_info tinfo_MXL5003S[2] = {
		[1] = { /*DVBT*/ .name		= "Maxliner",
				 .frequency_min = 44000000,
				 .frequency_max = 885000000, }
	};
	static struct dvb_tuner_info tinfo_FJ2207[2] = {
		[0] = { /*DVBC*/ .name		= "FJ2207",
				 .frequency_min = 54000000,
				 .frequency_max = 870000000, },
		[1] = { /*DVBT*/ .name		= "FJ2207",
				 .frequency_min = 174000000,
				 .frequency_max = 864000000, },
	};
	static struct dvb_tuner_info tinfo_DCT7070[2] = {
		[0] = { /*DVBC*/ .name		= "DCT7070",
				 .frequency_min = 51000000,
				 .frequency_max = 860000000, }
	};
	static struct dvb_tuner_info tinfo_TD1316[2] = {
		[1] = { /*DVBT*/ .name		= "TD1316",
				 .frequency_min = 51000000,
				 .frequency_max = 858000000, }
	};
	static struct dvb_tuner_info tinfo_SI2176[2] = {
		[0] = { /*DVBC*/
			/*#error please add SI2176 code*/
			.name		= "SI2176",
			.frequency_min	= 51000000,
			.frequency_max	= 860000000,
		}
	};

	struct dvb_tuner_info *tinfo[] = {
		&tinfo_null,
		tinfo_DCT7070,
		tinfo_MXL5003S,
		tinfo_FJ2207,
		tinfo_TD1316,
		tinfo_SI2176
	};

	if ((type < 0) || (type > 4) || (mode < 0) || (mode > 1))
		return tinfo[0];

	return &tinfo[type][mode];
}

struct agc_power_tab *tuner_get_agc_power_table(int type)
{
	/*type :  0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316 */
	static int calcE_FJ2207[31] = {
		87, 118, 138, 154, 172, 197, 245,
		273, 292, 312, 327, 354, 406, 430,
		448, 464, 481, 505, 558, 583, 599,
		616, 632, 653, 698, 725, 745, 762,
		779, 801, 831 };
	static int calcE_Maxliner[79] = {
		543, 552, 562, 575, 586, 596, 608,
		618, 627, 635, 645, 653, 662, 668,
		678, 689, 696, 705, 715, 725, 733,
		742, 752, 763, 769, 778, 789, 800,
		807, 816, 826, 836, 844, 854, 864,
		874, 884, 894, 904, 913, 923, 932,
		942, 951, 961, 970, 980, 990, 1000,
		1012, 1022, 1031, 1040, 1049, 1059,
		1069, 1079, 1088, 1098, 1107, 1115,
		1123, 1132, 1140, 1148, 1157, 1165,
		1173, 1179, 1186, 1192, 1198, 1203,
		1208, 1208, 1214, 1217, 1218, 1220 };

	static struct agc_power_tab power_tab[] = {
		[0] = { "null", 0, 0, NULL },
		[1] = {
			.name	= "DCT7070",
			.level	= 0,
			.ncalcE = 0,
			.calcE	= NULL,
		},
		[2] = {
			.name	= "Maxlear",
			.level	= -22,
			.ncalcE = sizeof(calcE_Maxliner) / sizeof(int),
			.calcE	= calcE_Maxliner,
		},
		[3] = {
			.name	= "FJ2207",
			.level	= -62,
			.ncalcE = sizeof(calcE_FJ2207) / sizeof(int),
			.calcE	= calcE_FJ2207,
		},
		[4] = {
			.name	= "TD1316",
			.level	= 0,
			.ncalcE = 0,
			.calcE	= NULL,
		},
	};

	if (type >= 2 && type <= 3)
		return &power_tab[type];
	else
		return &power_tab[3];
};

int agc_power_to_dbm(int agc_gain, int ad_power, int offset, int tuner)
{
	struct agc_power_tab *ptab = tuner_get_agc_power_table(tuner);
	int est_rf_power;
	int j;

	for (j = 0; j < ptab->ncalcE; j++)
		if (agc_gain <= ptab->calcE[j])
			break;

	est_rf_power = ptab->level - j - (ad_power >> 4) + 12 + offset;

	return est_rf_power;
}

int dtmb_get_power_strength(int agc_gain)
{
	int strength;
	int j;
	static int calcE_R840[13] = {
		1010, 969, 890, 840, 800,
		760, 720, 680, 670, 660,
		510, 440, 368};
	for (j = 0; j < sizeof(calcE_R840)/sizeof(int); j++)
		if (agc_gain >= calcE_R840[j])
			break;
	if (agc_gain >= 440)
		strength = -90+j*3;
	else
		strength = -56;
	return strength;
}

/*tuner has 3 stage gain control, only last is controlled by demod agc*/
static int dvbc_R842[20] = {
	/*-90,-89,-88,  -87, -86  , -85 , -84 , -83  , -82 , -81dbm*/
	1200, 1180, 1150, 1130, 1100, 1065, 1040, 1030, 1000, 970
};

int dvbc_get_power_strength(int agc_gain, int tuner_strength)
{
	int strength;
	int i;

	for (i = 0; i < sizeof(dvbc_R842)/sizeof(int); i++)
		if (agc_gain >= dvbc_R842[i])
			break;

	if (agc_gain >= 970)
		strength = -90+i*1;
	else
		strength = tuner_strength + 22;

	return strength;
}

static int j83b_R842[10] = {
	/*-90,-89,-88,  -87, -86  , -85 , -84 , -83  , -82 , -81dbm*/
	1140, 1110, 1080, 1060, 1030, 1000, 980, 1000, 970, 1000,
	/*-80,-79,-78,  -77, -76  , -75 , -74 , -73  , -72 , -71dbm*/
	/*970 ,    980, 960,  970, 950,   960,   970, 980,   960,   970*/
};

int  j83b_get_power_strength(int agc_gain, int tuner_strength)
{
	int strength;
	int i;

	for (i = 0; i < sizeof(j83b_R842)/sizeof(int); i++)
		if (agc_gain >= j83b_R842[i])
			break;

	if (agc_gain >= 970)
		strength = -90+i*1;
	else
		strength = tuner_strength + 18;

	return strength;
}

static int atsc_R842[6] = {
	/*-90,-89,-88,  -87, -86  , -85 , -84 , -83  , -82 , -81dbm*/
	2160, 2110, 2060, 2010, 1960, 1910/*, 1870, 1910, 1860, 1900*/
};

int atsc_get_power_strength(int agc_gain, int tuner_strength)
{
	int strength;
	int i;

	for (i = 0; i < sizeof(atsc_R842)/sizeof(int); i++)
		if (agc_gain >= atsc_R842[i])
			break;

	if (agc_gain >= 1910)
		strength = -90+i*1;
	else
		strength = tuner_strength;

	return strength;
}

