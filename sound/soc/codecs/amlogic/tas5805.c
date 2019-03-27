/*
 * Driver for the TAS5805M Audio Amplifier
 *
 * Author: Andy Liu <andy-liu@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <linux/amlogic/aml_gpio_consumer.h>

#include "tas5805.h"

#define TAS5805M_DRV_NAME    "tas5805"

#define TAS5805M_RATES	     (SNDRV_PCM_RATE_8000 | \
		       SNDRV_PCM_RATE_11025 | \
		       SNDRV_PCM_RATE_16000 | \
		       SNDRV_PCM_RATE_22050 | \
		       SNDRV_PCM_RATE_32000 | \
		       SNDRV_PCM_RATE_44100 | \
		       SNDRV_PCM_RATE_48000)
#define TAS5805M_FORMATS     (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE)

#define TAS5805M_REG_00      (0x00)
#define TAS5805M_REG_03      (0x03)
#define TAS5805M_REG_24      (0x24)
#define TAS5805M_REG_25      (0x25)
#define TAS5805M_REG_26      (0x26)
#define TAS5805M_REG_27      (0x27)
#define TAS5805M_REG_28      (0x28)
#define TAS5805M_REG_29      (0x29)
#define TAS5805M_REG_2A      (0x2a)
#define TAS5805M_REG_2B      (0x2b)
#define TAS5805M_REG_35      (0x35)
#define TAS5805M_DIG_VAL_CTL (0x4c)
#define TAS5805M_REG_7F      (0x7f)

#define TAS5805M_PAGE_00     (0x00)
#define TAS5805M_PAGE_2A     (0x2a)

#define TAS5805M_BOOK_00     (0x00)
#define TAS5805M_BOOK_8C     (0x8c)

#define TAS5805M_VOLUME_MAX  (578)
#define TAS5805M_VOLUME_MIN  (0)

const uint32_t tas5805m_volume[] = {
	0x0000001B,		//0, -110dB
	0x0000001E,		//1, -109dB
	0x00000021,		//2, -108dB
	0x00000025,		//3, -107dB
	0x0000002A,		//4, -106dB
	0x0000002F,		//5, -105dB
	0x00000035,		//6, -104dB
	0x0000003B,		//7, -103dB
	0x00000043,		//8, -102dB
	0x0000004B,		//9, -101dB
	0x00000054,		//10, -100dB
	0x0000005E,		//11, -99dB
	0x0000006A,		//12, -98dB
	0x00000076,		//13, -97dB
	0x00000085,		//14, -96dB
	0x00000095,		//15, -95dB
	0x000000A7,		//16, -94dB
	0x000000BC,		//17, -93dB
	0x000000D3,		//18, -92dB
	0x000000EC,		//19, -91dB
	0x00000109,		//20, -90dB
	0x0000012A,		//21, -89dB
	0x0000014E,		//22, -88dB
	0x00000177,		//23, -87dB
	0x000001A4,		//24, -86dB
	0x000001D8,		//25, -85dB
	0x00000211,		//26, -84dB
	0x00000252,		//27, -83dB
	0x0000029A,		//28, -82dB
	0x000002EC,		//29, -81dB
	0x00000347,		//30, -80dB
	0x000003AD,		//31, -79dB
	0x00000420,		//32, -78dB
	0x000004A1,		//33, -77dB
	0x00000532,		//34, -76dB
	0x000005D4,		//35, -75dB
	0x0000068A,		//36, -74dB
	0x00000756,		//37, -73dB
	0x0000083B,		//38, -72dB
	0x0000093C,		//39, -71dB
	0x00000A5D,		//40, -70dB
	0x00000BA0,		//41, -69dB
	0x00000D0C,		//42, -68dB
	0x00000EA3,		//43, -67dB
	0x0000106C,		//44, -66dB
	0x0000126D,		//45, -65dB
	0x000014AD,		//46, -64dB
	0x00001733,		//47, -63dB
	0x00001A07,		//48, -62dB
	0x00001D34,		//49, -61dB
	0x000020C5,		//50, -60dB
	0x000024C4,		//51, -59dB
	0x00002941,		//52, -58dB
	0x00002E49,		//53, -57dB
	0x000033EF,		//54, -56dB
	0x00003A45,		//55, -55dB
	0x00004161,		//56, -54dB
	0x0000495C,		//57, -53dB
	0x0000524F,		//58, -52dB
	0x00005C5A,		//59, -51dB
	0x0000679F,		//60, -50dB
	0x00007444,		//61, -49dB
	0x00008274,		//62, -48dB
	0x0000925F,		//63, -47dB
	0x0000A43B,		//64, -46dB
	0x0000B845,		//65, -45dB
	0x0000CEC1,		//66, -44dB
	0x0000E7FB,		//67, -43dB
	0x00010449,		//68, -42dB
	0x0001240C,		//69, -41dB
	0x000147AE,		//70, -40dB
	0x00016FAA,		//71, -39dB
	0x00019C86,		//72, -38dB
	0x0001CEDC,		//73, -37dB
	0x00020756,		//74, -36dB
	0x000246B5,		//75, -35dB
	0x00028DCF,		//76, -34dB
	0x0002DD96,		//77, -33dB
	0x00033718,		//78, -32dB
	0x00039B87,		//79, -31dB
	0x00040C37,		//80    -30
	0x00041B3C,		//81    -29.875
	0x00042A79,		//82    -29.75
	0x000439EE,		//83    -29.625
	0x0004499D,		//84    -29.5
	0x00045986,		//85    -29.375
	0x000469AA,		//86    -29.25
	0x00047A0A,		//87    -29.125
	0x00048AA7,		//88    -29
	0x00049B81,		//89    -28.875
	0x0004AC9A,		//90    -28.75
	0x0004BDF2,		//91    -28.625
	0x0004CF8B,		//92    -28.5
	0x0004E165,		//93    -28.375
	0x0004F381,		//94    -28.25
	0x000505E0,		//95    -28.125
	0x00051884,		//96    -28
	0x00052B6D,		//97    -27.875
	0x00053E9C,		//98    -27.75
	0x00055212,		//99    -27.625
	0x000565D0,		//100   -27.5
	0x000579D8,		//101   -27.375
	0x00058E2A,		//102   -27.25
	0x0005A2C7,		//103   -27.125
	0x0005B7B1,		//104   -27
	0x0005CCE8,		//105   -26.875
	0x0005E26E,		//106   -26.75
	0x0005F844,		//107   -26.625
	0x00060E6C,		//108   -26.5
	0x000624E5,		//109   -26.375
	0x00063BB1,		//110   -26.25
	0x000652D3,		//111   -26.125
	0x00066A4A,		//112   -26
	0x00068218,		//113   -25.875
	0x00069A3E,		//114   -25.75
	0x0006B2BF,		//115   -25.625
	0x0006CB9A,		//116   -25.5
	0x0006E4D1,		//117   -25.375
	0x0006FE66,		//118   -25.25
	0x0007185A,		//119   -25.125
	0x000732AE,		//120   -25
	0x00074D63,		//121   -24.875
	0x0007687C,		//122   -24.75
	0x000783FA,		//123   -24.625
	0x00079FDD,		//124   -24.5
	0x0007BC28,		//125   -24.375
	0x0007D8DC,		//126   -24.25
	0x0007F5FA,		//127   -24.125
	0x00081385,		//128   -24
	0x0008317D,		//129   -23.875
	0x00084FE4,		//130   -23.75
	0x00086EBC,		//131   -23.625
	0x00088E07,		//132   -23.5
	0x0008ADC6,		//133   -23.375
	0x0008CDFA,		//134   -23.25
	0x0008EEA6,		//135   -23.125
	0x00090FCB,		//136   -23
	0x0009316C,		//137   -22.875
	0x00095389,		//138   -22.75
	0x00097624,		//139   -22.625
	0x00099940,		//140   -22.5
	0x0009BCDF,		//141   -22.375
	0x0009E101,		//142   -22.25
	0x000A05AA,		//143   -22.125
	0x000A2ADA,		//144   -22
	0x000A5095,		//145   -21.875
	0x000A76DC,		//146   -21.75
	0x000A9DB0,		//147   -21.625
	0x000AC515,		//148   -21.5
	0x000AED0C,		//149   -21.375
	0x000B1597,		//150   -21.25
	0x000B3EB9,		//151   -21.125
	0x000B6873,		//152   -21
	0x000B92C8,		//153   -20.875
	0x000BBDBA,		//154   -20.75
	0x000BE94C,		//155   -20.625
	0x000C157F,		//156   -20.5
	0x000C4256,		//157   -20.375
	0x000C6FD4,		//158   -20.25
	0x000C9DFB,		//159   -20.125
	0x000CCCCC,		//160   -20
	0x000CFC4C,		//161   -19.875
	0x000D2C7B,		//162   -19.75
	0x000D5D5E,		//163   -19.625
	0x000D8EF6,		//164   -19.5
	0x000DC146,		//165   -19.375
	0x000DF451,		//166   -19.25
	0x000E2819,		//167   -19.125
	0x000E5CA1,		//168   -19
	0x000E91EC,		//169   -18.875
	0x000EC7FD,		//170   -18.75
	0x000EFED6,		//171   -18.625
	0x000F367B,		//172   -18.5
	0x000F6EEF,		//173   -18.375
	0x000FA834,		//174   -18.25
	0x000FE24E,		//175   -18.125
	0x00101D3F,		//176   -18
	0x0010590B,		//177   -17.875
	0x001095B4,		//178   -17.75
	0x0010D33F,		//179   -17.625
	0x001111AE,		//180   -17.5
	0x00115105,		//181   -17.375
	0x00119147,		//182   -17.25
	0x0011D278,		//183   -17.125
	0x0012149A,		//184   -17
	0x001257B2,		//185   -16.875
	0x00129BC2,		//186   -16.75
	0x0012E0CF,		//187   -16.625
	0x001326DD,		//188   -16.5
	0x00136DEE,		//189   -16.375
	0x0013B607,		//190   -16.25
	0x0013FF2C,		//191   -16.125
	0x00144960,		//192   -16
	0x001494A8,		//193   -15.875
	0x0014E107,		//194   -15.75
	0x00152E81,		//195   -15.625
	0x00157D1A,		//196   -15.5
	0x0015CCD8,		//197   -15.375
	0x00161DBD,		//198   -15.25
	0x00166FCE,		//199   -15.125
	0x0016C310,		//200   -15
	0x00171787,		//201   -14.875
	0x00176D38,		//202   -14.75
	0x0017C426,		//203   -14.625
	0x00181C57,		//204   -14.5
	0x001875CF,		//205   -14.375
	0x0018D093,		//206   -14.25
	0x00192CA8,		//207   -14.125
	0x00198A13,		//208   -14
	0x0019E8D8,		//209   -13.875
	0x001A48FD,		//210   -13.75
	0x001AAA87,		//211   -13.625
	0x001B0D7B,		//212   -13.5
	0x001B71DD,		//213   -13.375
	0x001BD7B5,		//214   -13.25
	0x001C3F06,		//215   -13.125
	0x001CA7D7,		//216   -13
	0x001D122D,		//217   -12.875
	0x001D7E0D,		//218   -12.75
	0x001DEB7D,		//219   -12.625
	0x001E5A84,		//220   -12.5
	0x001ECB27,		//221   -12.375
	0x001F3D6B,		//222   -12.25
	0x001FB158,		//223   -12.125
	0x002026F3,		//224   -12
	0x00209E42,		//225   -11.875
	0x0021174C,		//226   -11.75
	0x00219217,		//227   -11.625
	0x00220EA9,		//228   -11.5
	0x00228D0A,		//229   -11.375
	0x00230D40,		//230   -11.25
	0x00238F52,		//231   -11.125
	0x00241346,		//232   -11
	0x00249924,		//233   -10.875
	0x002520F3,		//234   -10.75
	0x0025AABA,		//235   -10.625
	0x00263680,		//236   -10.5
	0x0026C44D,		//237   -10.375
	0x00275427,		//238   -10.25
	0x0027E618,		//239   -10.125
	0x00287A26,		//240   -10
	0x0029105A,		//241   -9.875
	0x0029A8BB,		//242   -9.75
	0x002A4351,		//243   -9.625
	0x002AE025,		//244   -9.5
	0x002B7F3F,		//245   -9.375
	0x002C20A8,		//246   -9.25
	0x002CC467,		//247   -9.125
	0x002D6A86,		//248   -9
	0x002E130D,		//249   -8.875
	0x002EBE06,		//250   -8.75
	0x002F6B79,		//251   -8.625
	0x00301B70,		//252   -8.5
	0x0030CDF4,		//253   -8.375
	0x0031830E,		//254   -8.25
	0x00323AC8,		//255   -8.125
	0x0032F52C,		//256   -8
	0x0033B244,		//257   -7.875
	0x0034721A,		//258   -7.75
	0x003534B7,		//259   -7.625
	0x0035FA26,		//260   -7.5
	0x0036C272,		//261   -7.375
	0x00378DA5,		//262   -7.25
	0x00385BCB,		//263   -7.125
	0x00392CED,		//264   -7
	0x003A0117,		//265   -6.875
	0x003AD855,		//266   -6.75
	0x003BB2B1,		//267   -6.625
	0x003C9038,		//268   -6.5
	0x003D70F5,		//269   -6.375
	0x003E54F3,		//270   -6.25
	0x003F3C40,		//271   -6.125
	0x004026E7,		//272   -6
	0x004114F4,		//273   -5.875
	0x00420675,		//274   -5.75
	0x0042FB77,		//275   -5.625
	0x0043F405,		//276   -5.5
	0x0044F02E,		//277   -5.375
	0x0045EFFE,		//278   -5.25
	0x0046F384,		//279   -5.125
	0x0047FACC,		//280   -5
	0x004905E6,		//281   -4.875
	0x004A14DF,		//282   -4.75
	0x004B27C5,		//283   -4.625
	0x004C3EA8,		//284   -4.5
	0x004D5995,		//285   -4.375
	0x004E789C,		//286   -4.25
	0x004F9BCD,		//287   -4.125
	0x0050C335,		//288   -4
	0x0051EEE6,		//289   -3.875
	0x00531EEF,		//290   -3.75
	0x00545361,		//291   -3.625
	0x00558C4B,		//292   -3.5
	0x0056C9BE,		//293   -3.375
	0x00580BCB,		//294   -3.25
	0x00595283,		//295   -3.125
	0x005A9DF7,		//296   -3
	0x005BEE3A,		//297   -2.875
	0x005D435C,		//298   -2.75
	0x005E9D70,		//299   -2.625
	0x005FFC88,		//300   -2.5
	0x006160B7,		//301   -2.375
	0x0062CA10,		//302   -2.25
	0x006438A6,		//303   -2.125
	0x0065AC8C,		//304   -2
	0x006725D6,		//305   -1.875
	0x0068A498,		//306   -1.75
	0x006A28E6,		//307   -1.625
	0x006BB2D6,		//308   -1.5
	0x006D427B,		//309   -1.375
	0x006ED7EB,		//310   -1.25
	0x0070733B,		//311   -1.125
	0x00721482,		//312   -1
	0x0073BBD6,		//313   -0.875
	0x0075694C,		//314   -0.75
	0x00771CFC,		//315   -0.625
	0x0078D6FC,		//316   -0.5
	0x007A9765,		//317   -0.375
	0x007C5E4E,		//318   -0.25
	0x007E2BCE,		//319   -0.125
	0x00800000,		//320   0
	0x0081DAFA,		//321   0.125
	0x0083BCD7,		//322   0.25
	0x0085A5B1,		//323   0.375
	0x008795A0,		//324   0.5
	0x00898CBF,		//325   0.625
	0x008B8B2A,		//326   0.75
	0x008D90FA,		//327   0.875
	0x008F9E4C,		//328   1
	0x0091B33C,		//329   1.125
	0x0093CFE5,		//330   1.25
	0x0095F464,		//331   1.375
	0x009820D7,		//332   1.5
	0x009A555A,		//333   1.625
	0x009C920D,		//334   1.75
	0x009ED70C,		//335   1.875
	0x00A12477,		//336   2
	0x00A37A6E,		//337   2.125
	0x00A5D90F,		//338   2.25
	0x00A8407C,		//339   2.375
	0x00AAB0D4,		//340   2.5
	0x00AD2A39,		//341   2.625
	0x00AFACCC,		//342   2.75
	0x00B238B0,		//343   2.875
	0x00B4CE07,		//344   3
	0x00B76CF4,		//345   3.125
	0x00BA159B,		//346   3.25
	0x00BCC81F,		//347   3.375
	0x00BF84A6,		//348   3.5
	0x00C24B54,		//349   3.625
	0x00C51C4F,		//350   3.75
	0x00C7F7BE,		//351   3.875
	0x00CADDC7,		//352   4
	0x00CDCE92,		//353   4.125
	0x00D0CA46,		//354   4.25
	0x00D3D10B,		//355   4.375
	0x00D6E30C,		//356   4.5
	0x00DA0072,		//357   4.625
	0x00DD2966,		//358   4.75
	0x00E05E15,		//359   4.875
	0x00E39EA8,		//360   5
	0x00E6EB4E,		//361   5.125
	0x00EA4431,		//362   5.25
	0x00EDA980,		//363   5.375
	0x00F11B69,		//364   5.5
	0x00F49A1B,		//365   5.625
	0x00F825C5,		//366   5.75
	0x00FBBE96,		//367   5.875
	0x00FF64C1,		//368   6
	0x01031876,		//369   6.125
	0x0106D9E8,		//370   6.25
	0x010AA94A,		//371   6.375
	0x010E86CF,		//372   6.5
	0x011272AB,		//373   6.625
	0x01166D15,		//374   6.75
	0x011A7643,		//375   6.875
	0x011E8E6A,		//376   7
	0x0122B5C2,		//377   7.125
	0x0126EC84,		//378   7.25
	0x012B32EA,		//379   7.375
	0x012F892C,		//380   7.5
	0x0133EF86,		//381   7.625
	0x01386634,		//382   7.75
	0x013CED72,		//383   7.875
	0x0141857E,		//384   8
	0x01462E96,		//385   8.125
	0x014AE8F9,		//386   8.25
	0x014FB4E8,		//387   8.375
	0x015492A3,		//388   8.5
	0x0159826D,		//389   8.625
	0x015E8488,		//390   8.75
	0x01639939,		//391   8.875
	0x0168C0C5,		//392   9
	0x016DFB71,		//393   9.125
	0x01734985,		//394   9.25
	0x0178AB48,		//395   9.375
	0x017E2104,		//396   9.5
	0x0183AB02,		//397   9.625
	0x0189498F,		//398   9.75
	0x018EFCF5,		//399   9.875
	0x0194C583,		//400   10
	0x019AA387,		//401   10.125
	0x01A09751,		//402   10.25
	0x01A6A131,		//403   10.375
	0x01ACC179,		//404   10.5
	0x01B2F87D,		//405   10.625
	0x01B94691,		//406   10.75
	0x01BFAC0A,		//407   10.875
	0x01C62940,		//408   11
	0x01CCBE8A,		//409   11.125
	0x01D36C42,		//410   11.25
	0x01DA32C2,		//411   11.375
	0x01E11266,		//412   11.5
	0x01E80B8C,		//413   11.625
	0x01EF1E92,		//414   11.75
	0x01F64BD9,		//415   11.875
	0x01FD93C1,		//416   12
	0x0204F6AE,		//417   12.125
	0x020C7504,		//418   12.25
	0x02140F28,		//419   12.375
	0x021BC582,		//420   12.5
	0x0223987A,		//421   12.625
	0x022B887B,		//422   12.75
	0x023395F0,		//423   12.875
	0x023BC147,		//424   13
	0x02440AEE,		//425   13.125
	0x024C7356,		//426   13.25
	0x0254FAF2,		//427   13.375
	0x025DA234,		//428   13.5
	0x02666992,		//429   13.625
	0x026F5184,		//430   13.75
	0x02785A83,		//431   13.875
	0x02818508,		//432   14
	0x028AD191,		//433   14.125
	0x0294409B,		//434   14.25
	0x029DD2A7,		//435   14.375
	0x02A78836,		//436   14.5
	0x02B161CD,		//437   14.625
	0x02BB5FF1,		//438   14.75
	0x02C5832A,		//439   14.875
	0x02CFCC01,		//440   15
	0x02DA3B02,		//441   15.125
	0x02E4D0BA,		//442   15.25
	0x02EF8DB9,		//443   15.375
	0x02FA7292,		//444   15.5
	0x03057FD7,		//445   15.625
	0x0310B61F,		//446   15.75
	0x031C1602,		//447   15.875
	0x0327A01A,		//448   16
	0x03335504,		//449   16.125
	0x033F355F,		//450   16.25
	0x034B41CC,		//451   16.375
	0x03577AEF,		//452   16.5
	0x0363E16D,		//453   16.625
	0x037075EF,		//454   16.75
	0x037D3920,		//455   16.875
	0x038A2BAC,		//456   17
	0x03974E44,		//457   17.125
	0x03A4A19A,		//458   17.25
	0x03B22662,		//459   17.375
	0x03BFDD55,		//460   17.5
	0x03CDC72C,		//461   17.625
	0x03DBE4A4,		//462   17.75
	0x03EA367D,		//463   17.875
	0x03F8BD79,		//464   18
	0x04077A5E,		//465   18.125
	0x04166DF2,		//466   18.25
	0x04259902,		//467   18.375
	0x0434FC5C,		//468   18.5
	0x044498CF,		//469   18.625
	0x04546F30,		//470   18.75
	0x04648056,		//471   18.875
	0x0474CD1B,		//472   19
	0x0485565C,		//473   19.125
	0x04961CFA,		//474   19.25
	0x04A721D8,		//475   19.375
	0x04B865DE,		//476   19.5
	0x04C9E9F5,		//477   19.625
	0x04DBAF0C,		//478   19.75
	0x04EDB613,		//479   19.875
	0x05000000,		//480   20
	0x05128DCA,		//481   20.125
	0x0525606D,		//482   20.25
	0x053878EA,		//483   20.375
	0x054BD842,		//484   20.5
	0x055F7F7E,		//485   20.625
	0x05736FA7,		//486   20.75
	0x0587A9CD,		//487   20.875
	0x059C2F01,		//488   21
	0x05B1005B,		//489   21.125
	0x05C61EF5,		//490   21.25
	0x05DB8BEE,		//491   21.375
	0x05F14868,		//492   21.5
	0x0607558B,		//493   21.625
	0x061DB482,		//494   21.75
	0x0634667C,		//495   21.875
	0x064B6CAD,		//496   22
	0x0662C84F,		//497   22.125
	0x067A7A9D,		//498   22.25
	0x069284DB,		//499   22.375
	0x06AAE84D,		//500   22.5
	0x06C3A63F,		//501   22.625
	0x06DCC001,		//502   22.75
	0x06F636E8,		//503   22.875
	0x07100C4D,		//504   23
	0x072A418F,		//505   23.125
	0x0744D811,		//506   23.25
	0x075FD13C,		//507   23.375
	0x077B2E7F,		//508   23.5
	0x0796F14D,		//509   23.625
	0x07B31B1F,		//510   23.75
	0x07CFAD73,		//511   23.875
	0x07ECA9CD,		//512   24
	0x080A11B5,		//513   24.125
	0x0827E6BD,		//514   24.25
	0x08462A77,		//515   24.375
	0x0864DE80,		//516   24.5
	0x08840477,		//517   24.625
	0x08A39E04,		//518   24.75
	0x08C3ACD3,		//519   24.875
	0x08E43298,		//520   25
	0x0905310C,		//521   25.125
	0x0926A9EF,		//522   25.25
	0x09489F07,		//523   25.375
	0x096B1222,		//524   25.5
	0x098E0512,		//525   25.625
	0x09B179B2,		//526   25.75
	0x09D571E3,		//527   25.875
	0x09F9EF8E,		//528   26
	0x0A1EF4A1,		//529   26.125
	0x0A448314,		//530   26.25
	0x0A6A9CE4,		//531   26.375
	0x0A914416,		//532   26.5
	0x0AB87AB7,		//533   26.625
	0x0AE042DB,		//534   26.75
	0x0B089E9E,		//535   26.875
	0x0B319024,		//536   27
	0x0B5B1998,		//537   27.125
	0x0B853D2F,		//538   27.25
	0x0BAFFD24,		//539   27.375
	0x0BDB5BBC,		//540   27.5
	0x0C075B43,		//541   27.625
	0x0C33FE0E,		//542   27.75
	0x0C61467B,		//543   27.875
	0x0C8F36F2,		//544   28
	0x0CBDD1E0,		//545   28.125
	0x0CED19C0,		//546   28.25
	0x0D1D1113,		//547   28.375
	0x0D4DBA63,		//548   28.5
	0x0D7F1845,		//549   28.625
	0x0DB12D58,		//550   28.75
	0x0DE3FC43,		//551   28.875
	0x0E1787B8,		//552   29
	0x0E4BD272,		//553   29.125
	0x0E80DF37,		//554   29.25
	0x0EB6B0D7,		//555   29.375
	0x0EED4A2D,		//556   29.5
	0x0F24AE1D,		//557   29.625
	0x0F5CDF98,		//558   29.75
	0x0F95E199,		//559   29.875
	0x0FCFB724,		//560   30
	0x11BD9C84,		//561   31dB
	0x13E7C594,		//562   32dB
	0x16558CCB,		//563   33dB
	0x190F3254,		//564   34dB
	0x1C1DF80E,		//565   35dB
	0x1F8C4107,		//566   36dB
	0x2365B4BF,		//567   37dB
	0x27B766C2,		//568   38dB
	0x2C900313,		//569   39dB
	0x32000000,		//570   40dB
	0x3819D612,		//571   41dB
	0x3EF23ECA,		//572   42dB
	0x46A07B07,		//573   43dB
	0x4F3EA203,		//574   44dB
	0x58E9F9F9,		//575   45dB
	0x63C35B8E,		//576   46dB
	0x6FEFA16D,		//577   47dB
	0x7D982575,		//578   48dB
};
#define TAS5805_EQPARAM_LENGTH 610
#define TAS5805_EQ_LENGTH 245
#define FILTER_PARAM_BYTE 244
static  int m_eq_tab[TAS5805_EQPARAM_LENGTH][2] = {0};
#define TAS5805_DRC_PARAM_LENGTH 29
#define TAS5805_DRC_PARAM_COUNT  58
static  int m_drc_tab[TAS5805_DRC_PARAM_LENGTH][2] = {0};

struct tas5805m_priv {
	struct regmap *regmap;
	struct tas5805m_platform_data *pdata;
	int vol;
	int mute;
	struct snd_soc_codec *codec;
};

const struct regmap_config tas5805m_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static int tas5805m_vol_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access =
	    (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count = 1;

	uinfo->value.integer.min = TAS5805M_VOLUME_MIN;
	uinfo->value.integer.max = TAS5805M_VOLUME_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int tas5805m_mute_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access =
	    (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count = 1;

	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

static int tas5805m_vol_locked_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tas5805m->vol;

	return 0;
}

static inline int get_volume_index(int vol)
{
	int index;

	index = vol;

	if (index < TAS5805M_VOLUME_MIN)
		index = TAS5805M_VOLUME_MIN;

	if (index > TAS5805M_VOLUME_MAX)
		index = TAS5805M_VOLUME_MAX;

	return index;
}

static void tas5805m_set_volume(struct snd_soc_codec *codec, int vol)
{
	unsigned int index;
	uint32_t volume_hex;
	uint8_t byte4;
	uint8_t byte3;
	uint8_t byte2;
	uint8_t byte1;

	index = get_volume_index(vol);
	volume_hex = tas5805m_volume[index];

	byte4 = ((volume_hex >> 24) & 0xFF);
	byte3 = ((volume_hex >> 16) & 0xFF);
	byte2 = ((volume_hex >> 8) & 0xFF);
	byte1 = ((volume_hex >> 0) & 0xFF);

	//w 58 00 00
	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	//w 58 7f 8c
	snd_soc_write(codec, TAS5805M_REG_7F, TAS5805M_BOOK_8C);
	//w 58 00 2a
	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_2A);
	//w 58 24 xx xx xx xx
	snd_soc_write(codec, TAS5805M_REG_24, byte4);
	snd_soc_write(codec, TAS5805M_REG_25, byte3);
	snd_soc_write(codec, TAS5805M_REG_26, byte2);
	snd_soc_write(codec, TAS5805M_REG_27, byte1);
	//w 58 28 xx xx xx xx
	snd_soc_write(codec, TAS5805M_REG_28, byte4);
	snd_soc_write(codec, TAS5805M_REG_29, byte3);
	snd_soc_write(codec, TAS5805M_REG_2A, byte2);
	snd_soc_write(codec, TAS5805M_REG_2B, byte1);
}

static int tas5805m_vol_locked_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);


	tas5805m->vol = ucontrol->value.integer.value[0];
	tas5805m_set_volume(codec, tas5805m->vol);

	return 0;
}

static int tas5805m_mute(struct snd_soc_codec *codec, int mute)
{
	u8 reg03_value = 0;
	u8 reg35_value = 0;

	if (mute) {
		//mute both left & right channels
		reg03_value = 0x0b;
		reg35_value = 0x00;
	} else {
		//unmute
		reg03_value = 0x03;
		reg35_value = 0x11;
	}

	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_write(codec, TAS5805M_REG_7F, TAS5805M_BOOK_00);
	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_write(codec, TAS5805M_REG_03, reg03_value);
	snd_soc_write(codec, TAS5805M_REG_35, reg35_value);

	return 0;
}

static int tas5805m_mute_locked_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	tas5805m->mute = ucontrol->value.integer.value[0];
	tas5805m_mute(codec, tas5805m->mute);

	return 0;
}

static int tas5805m_mute_locked_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);
	ucontrol->value.integer.value[0] = tas5805m->mute;
	return 0;
}

static int tas5805_set_EQ_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5805_get_EQ_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5805_set_DRC_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5805_get_DRC_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5805_set_DRC_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	void *data;
	char tmp_string[TAS5805_DRC_PARAM_COUNT];
	char *p_string = &tmp_string[0];
	u8 *val;
	unsigned int i = 0;

	data = kmemdup(ucontrol->value.bytes.data,
		TAS5805_DRC_PARAM_COUNT, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (u8 *)data;
	memcpy(p_string, val, TAS5805_DRC_PARAM_COUNT);

	for (i = 0; i < TAS5805_DRC_PARAM_COUNT/2; i++) {
		m_drc_tab[i][0] = tmp_string[2*i];
		m_drc_tab[i][1] = tmp_string[2*i+1];
	}

	for (i = 0; i < TAS5805_DRC_PARAM_LENGTH; i++)
		snd_soc_write(codec, m_drc_tab[i][0], m_drc_tab[i][1]);

	kfree(data);
	return 0;
}

static int tas5805_get_DRC_param(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static int tas5805_set_EQ_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	void *data;
	char tmp_string[TAS5805_EQ_LENGTH];
	char *p_string = &tmp_string[0];
	u8 *val;
	int band_id;
	unsigned int i = 0, j = 0;

	data = kmemdup(ucontrol->value.bytes.data,
		TAS5805_EQ_LENGTH, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (u8 *) data;
	memcpy(p_string, val, TAS5805_EQ_LENGTH);
	band_id = tmp_string[0];
	for (j = 0, i = band_id * FILTER_PARAM_BYTE / 2;
			j < FILTER_PARAM_BYTE / 2; i++, j++) {
		m_eq_tab[i][0] = tmp_string[2*j+1];
		m_eq_tab[i][1] = tmp_string[2*j+2];
	}
	if (band_id == 4) {
		for (i = 0; i < TAS5805_EQPARAM_LENGTH; i++)
			snd_soc_write(codec, m_eq_tab[i][0], m_eq_tab[i][1]);
	}
	kfree(data);
	return 0;
}

static int tas5805_get_EQ_param(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static const struct snd_kcontrol_new tas5805m_vol_control[] = {
	{
	 .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	 .name = "Master Volume",
	 .info = tas5805m_vol_info,
	 .get = tas5805m_vol_locked_get,
	 .put = tas5805m_vol_locked_put,
	 },
	{
	 .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	 .name = "Maser Volume Mute",
	 .info = tas5805m_mute_info,
	 .get = tas5805m_mute_locked_get,
	 .put = tas5805m_mute_locked_put,
	},
	SOC_SINGLE_BOOL_EXT("Set EQ Enable", 0,
			   tas5805_get_EQ_enum, tas5805_set_EQ_enum),
	SOC_SINGLE_BOOL_EXT("Set DRC Enable", 0,
			   tas5805_get_DRC_enum, tas5805_set_DRC_enum),
	SND_SOC_BYTES_EXT("EQ table", TAS5805_EQ_LENGTH,
			   tas5805_get_EQ_param, tas5805_set_EQ_param),
	SND_SOC_BYTES_EXT("DRC table", TAS5805_DRC_PARAM_COUNT,
			   tas5805_get_DRC_param, tas5805_set_DRC_param),
};

static int tas5805m_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	pr_debug("level = %d\n", level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		/* Full power on */
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		/* The chip runs through the power down sequence for us. */
		break;
	}
	codec->component.dapm.bias_level = level;

	return 0;
}

static int tas5805m_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *codec_dai)
{
	struct tas5805m_priv *tas5805m = snd_soc_dai_get_drvdata(codec_dai);
	struct snd_soc_codec *codec = tas5805m->codec;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			pr_debug("%s(), start\n", __func__);
			snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
			snd_soc_write(codec, TAS5805M_REG_7F, TAS5805M_BOOK_00);
			snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
			snd_soc_write(codec, TAS5805M_DIG_VAL_CTL, 0x30);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			pr_debug("%s(), stop\n", __func__);
			snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
			snd_soc_write(codec, TAS5805M_REG_7F, TAS5805M_BOOK_00);
			snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
			snd_soc_write(codec, TAS5805M_DIG_VAL_CTL, 0xff);
			break;
		}
	}
	return 0;
}
static int reset_tas5805m_GPIO(struct device *dev)
{
	struct tas5805m_priv *tas5805m =  dev_get_drvdata(dev);
	struct tas5805m_platform_data *pdata = tas5805m->pdata;
	int ret = 0;

	if (pdata->reset_pin < 0)
		return 0;

	ret = devm_gpio_request_one(dev, pdata->reset_pin,
					    GPIOF_OUT_INIT_LOW,
					    "tas5805m-reset-pin");
	if (ret < 0)
		return -1;

	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
	usleep_range(1 * 1000, 2 * 1000);
	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
	usleep_range(5 * 1000, 6 * 1000);

	return 0;
}

static int tas5805m_snd_suspend(struct snd_soc_codec *codec)
{
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);
	struct tas5805m_platform_data *pdata = tas5805m->pdata;
	dev_info(codec->dev, "tas5805m_suspend!\n");
	tas5805m_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if (pdata->reset_pin)
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
	udelay(10);
	return 0;
}


static int tas5805m_reg_init(struct snd_soc_codec *codec)
{
	int i, j = 0;

	for (j = 0; j < ARRAY_SIZE(tas5805m_reset); j++) {
		snd_soc_write(codec, tas5805m_reset[j][0],
			tas5805m_reset[j][1]);
	};
	usleep_range(10 * 1000, 11 * 1000);
	for (i = 0; i < ARRAY_SIZE(tas5805m_init_sequence); i++) {
		snd_soc_write(codec, tas5805m_init_sequence[i][0],
			tas5805m_init_sequence[i][1]);
	};
	return 0;

}

static int tas5805m_snd_resume(struct snd_soc_codec *codec)
{
	int ret;
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);
	struct tas5805m_platform_data *pdata = tas5805m->pdata;
	dev_info(codec->dev, "tas5805m_snd_resume!\n");

	if (pdata->reset_pin)
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);

	usleep_range(3 * 1000, 4 * 1000);

	ret = tas5805m_reg_init(codec);
//	    regmap_register_patch(tas5805m->regmap, tas5805m_init_sequence,
//				  ARRAY_SIZE(tas5805m_init_sequence));
	if (ret != 0) {
		dev_err(codec->dev, "Failed to initialize TAS5805M: %d\n", ret);
		goto err;
	}

	tas5805m_set_volume(codec, tas5805m->vol);
	tas5805m_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
err:
	return ret;
}




static int tas5805m_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

//	ret =
//	    regmap_register_patch(tas5805m->regmap, tas5805m_init_sequence,
//				  ARRAY_SIZE(tas5805m_init_sequence));
	usleep_range(20 * 1000, 21 * 1000);
	ret = tas5805m_reg_init(codec);
	if (ret != 0)
		goto err;

	if (tas5805m)
		tas5805m_set_volume(codec, tas5805m->vol);

	snd_soc_add_codec_controls(codec, tas5805m_vol_control,
			ARRAY_SIZE(tas5805m_vol_control));
	tas5805m->codec = codec;
	return 0;

err:
	return ret;

}

static int tas5805m_remove(struct snd_soc_codec *codec)
{
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);
	struct tas5805m_platform_data *pdata = tas5805m->pdata;

	if (pdata->reset_pin)
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);

	udelay(10);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_tas5805m = {
	.probe = tas5805m_probe,
	.remove = tas5805m_remove,
	.suspend = tas5805m_snd_suspend,
	.resume = tas5805m_snd_resume,
	.set_bias_level = tas5805m_set_bias_level,
};

static const struct snd_soc_dai_ops tas5805m_dai_ops = {
	//.digital_mute = tas5805m_mute,
	.trigger = tas5805m_trigger,
};

static struct snd_soc_dai_driver tas5805m_dai = {
	.name = "tas5805m-amplifier",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 2,
		     .channels_max = 8,
		     .rates = TAS5805M_RATES,
		     .formats = TAS5805M_FORMATS,
		     },
	.ops = &tas5805m_dai_ops,
};

static int tas5805m_parse_dt(
	struct tas5805m_priv *tas5805m,
	struct device_node *np)
{
	int ret = 0;
	int reset_pin = -1;

	reset_pin = of_get_named_gpio(np, "reset_pin", 0);
	if (reset_pin < 0) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
		ret = -1;
	} else {
		pr_info("%s pdata->reset_pin = %d!\n", __func__,
				tas5805m->pdata->reset_pin);
	}
	tas5805m->pdata->reset_pin = reset_pin;

	return ret;
}

static int tas5805m_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config = tas5805m_regmap;
	struct tas5805m_priv *tas5805m;
	struct tas5805m_platform_data *pdata;
	int ret = 0;

	tas5805m = devm_kzalloc(&i2c->dev,
		sizeof(struct tas5805m_priv), GFP_KERNEL);
	if (!tas5805m)
		return -ENOMEM;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	pdata = devm_kzalloc(&i2c->dev,
				sizeof(struct tas5805m_platform_data),
				GFP_KERNEL);
	if (!pdata) {
		pr_err("%s failed to kzalloc for tas5805 pdata\n", __func__);
		return -ENOMEM;
	}
	tas5805m->pdata = pdata;

	tas5805m_parse_dt(tas5805m, i2c->dev.of_node);
	tas5805m->regmap = regmap;
	tas5805m->vol = 100;	//100, -10dB

	dev_set_drvdata(&i2c->dev, tas5805m);

	ret =
		snd_soc_register_codec(&i2c->dev, &soc_codec_tas5805m,
			&tas5805m_dai, 1);
	if (ret != 0)
		return -ENOMEM;

	reset_tas5805m_GPIO(&i2c->dev);

	return ret;
}

static int tas5805m_i2c_remove(struct i2c_client *i2c)
{
	devm_kfree(&i2c->dev, i2c_get_clientdata(i2c));

	return 0;
}

static const struct i2c_device_id tas5805m_i2c_id[] = {
	{"tas5805",},
	{}
};

MODULE_DEVICE_TABLE(i2c, tas5805m_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id tas5805m_of_match[] = {
	{.compatible = "ti,tas5805",},
	{}
};

MODULE_DEVICE_TABLE(of, tas5805m_of_match);
#endif

static struct i2c_driver tas5805m_i2c_driver = {
	.probe = tas5805m_i2c_probe,
	.remove = tas5805m_i2c_remove,
	.id_table = tas5805m_i2c_id,
	.driver = {
		   .name = TAS5805M_DRV_NAME,
		   .of_match_table = tas5805m_of_match,
		   },
};

module_i2c_driver(tas5805m_i2c_driver);

MODULE_AUTHOR("Andy Liu <andy-liu@ti.com>");
MODULE_DESCRIPTION("TAS5805M Audio Amplifier Driver");
MODULE_LICENSE("GPL v2");
