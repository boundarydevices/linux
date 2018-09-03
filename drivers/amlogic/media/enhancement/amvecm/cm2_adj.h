#ifndef __CM2_ADJ__
#define __CM2_ADJ__


enum eCM2ColorMd {
	eCM2ColorMd_purple = 0,
	eCM2ColorMd_red,
	eCM2ColorMd_skin,
	eCM2ColorMd_yellow,
	eCM2ColorMd_green,
	eCM2ColorMd_cyan,
	eCM2ColorMd_blue,
};

/*H00 ~ H31*/
#define CM2_ENH_COEF0_H00 0x100
#define CM2_ENH_COEF1_H00 0x101
#define CM2_ENH_COEF2_H00 0x102
#define CM2_ENH_COEF3_H00 0x103
#define CM2_ENH_COEF4_H00 0x104

#define CM2_ENH_COEF0_H01 0x108
#define CM2_ENH_COEF1_H01 0x109
#define CM2_ENH_COEF2_H01 0x10a
#define CM2_ENH_COEF3_H01 0x10b
#define CM2_ENH_COEF4_H01 0x10c

#define CM2_ENH_COEF0_H02 0x110
#define CM2_ENH_COEF1_H02 0x111
#define CM2_ENH_COEF2_H02 0x112
#define CM2_ENH_COEF3_H02 0x113
#define CM2_ENH_COEF4_H02 0x114


extern void cm2_hue_by_hs(enum eCM2ColorMd colormode, int hue_val, int lpf_en);
extern void cm2_hue(enum eCM2ColorMd colormode, int hue_val, int lpf_en);
extern void cm2_luma(enum eCM2ColorMd colormode, int luma_val, int lpf_en);
extern void cm2_sat(enum eCM2ColorMd colormode, int sat_val, int lpf_en);

#endif

