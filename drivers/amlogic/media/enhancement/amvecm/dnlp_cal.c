#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/ve.h>
#include "dnlp_cal.h"
#include "amve.h"
#include "arch/vpp_regs.h"
#include <linux/amlogic/media/amvecm/ve.h>

#define pr_dnlp_dbg(fmt, args...)\
	do {\
		if (dnlp_printk)\
			pr_info("DNLP_CAL: " fmt, ## args);\
	} while (0)

unsigned char ve_dnlp_tgt[65] = {
	0, 4, 8, 12, 16, 20,
	24, 28, 32, 36, 40, 44, 48, 52,
	56, 60, 64, 68, 72, 76, 80, 84,
	88, 92, 96, 100, 104, 108, 112, 116,
	120, 124, 128, 132, 136, 140, 144,
	148, 152, 156, 160, 164, 168, 172,
	176, 180, 184, 188, 192, 196, 200,
	204, 208, 212, 216, 220, 224, 228,
	232, 236, 240, 244, 248, 252, 255
};

bool ve_en;
static bool menu_chg_en;
module_param(menu_chg_en, bool, 0664);
MODULE_PARM_DESC(menu_chg_en, "menu_chg_en");

static bool print_en;
module_param(print_en, bool, 0664);
MODULE_PARM_DESC(print_en, "print_en");

static bool BLE_en = 1;
module_param(BLE_en, bool, 0664);
MODULE_PARM_DESC(BLE_en, "BLE_en");


/*scene change threshold,>30 to avoid black menu effect*/
static int scn_chg_th = 48;
module_param(scn_chg_th, int, 0664);
MODULE_PARM_DESC(scn_chg_th, "scn_chg_th");
static int NORM = 10;
module_param(NORM, int, 0664);
MODULE_PARM_DESC(NORM, "NORM");

static int ve_hist_cur_gain = 8;
module_param(ve_hist_cur_gain, int, 0664);
MODULE_PARM_DESC(ve_hist_cur_gain, "ve_hist_cur_gain");

static int ve_hist_cur_gain_precise = 8;/*>0 */
module_param(ve_hist_cur_gain_precise, int, 0664);
MODULE_PARM_DESC(ve_hist_cur_gain_precise, "ve_hist_cur_gain_precise");


static int clahe_method;/*0-old,1-new*/
module_param(clahe_method, int, 0664);
MODULE_PARM_DESC(clahe_method, "clahe_method");

static unsigned int nTstCnt;

unsigned int ve_dnlp_rt;
unsigned int ve_dnlp_luma_sum;
ulong ve_dnlp_lpf[64], ve_dnlp_reg[16];
ulong ve_dnlp_reg_def[16] = {
	0x0b070400,	0x1915120e,	0x2723201c,	0x35312e2a,
	0x47423d38,	0x5b56514c,	0x6f6a6560,	0x837e7974,
	0x97928d88,	0xaba6a19c,	0xbfbab5b0,	0xcfccc9c4,
	0xdad7d5d2,	0xe6e3e0dd,	0xf2efece9,	0xfdfaf7f4
};

/*for read some params in calculating progress*/
int ro_luma_avg4;/*0~1024*/
int ro_var_d8;/*1024*/
int ro_scurv_gain;/*1024*/
int ro_blk_wht_ext0;/*1024*/
int ro_blk_wht_ext1;/*1024*/
int ro_dnlp_brightness;/*1024*/
int ro_mtdbld_rate;/*1024*/

/* pre-defined 5 s curve, for outside setting */
int dnlp_scurv_low[65] = {0, 14, 26, 37, 48,
						60, 74, 91, 112, 138,
						166, 196, 224, 249, 271,
						292, 312, 332, 352, 372,
						392, 412, 433, 453, 472,
						491, 509, 527, 544, 561,
						578, 594, 608, 621, 633,
						644, 656, 669, 683, 697,
						712, 727, 741, 755, 768,
						781, 793, 805, 816, 827,
						839, 851, 864, 878, 892,
						906, 920, 933, 945, 956,
						968, 980, 993, 1008,
						1024};
int dnlp_scurv_mid1[65] = {0, 12, 22, 31, 40,
						49, 58, 68, 80, 94, 110,
						127, 144, 162, 180, 198,
						216, 235, 254, 274, 296,
						319, 343, 368, 392, 416,
						440, 464, 488, 513, 537,
						561, 584, 605, 624, 642,
						660, 677, 695, 712, 728,
						744, 760, 776, 792, 809,
						825, 841, 856, 869, 881,
						893, 904, 916, 928, 940,
						952, 963, 973, 983, 992,
						1001, 1009, 1016, 1024
						};
int dnlp_scurv_mid2[65] = {0, 6, 11, 17, 24,
						32, 41, 51, 64, 79,
						95, 112, 128, 143, 157,
						171, 184, 198, 213, 229,
						248, 269, 293, 318, 344,
						370, 396, 422, 448, 475,
						501, 527, 552, 576, 598,
						620, 640, 660, 678, 696,
						712, 727, 740, 754, 768,
						783, 800, 816, 832, 847,
						861, 874, 888, 902, 916,
						930, 944, 957, 970, 981,
						992, 1002, 1010, 1018,
						1024};
int dnlp_scurv_hgh1[65] = {0, 11, 23, 35, 48,
						61, 75, 89, 104, 120,
						136, 152, 168, 184, 200,
						216, 232, 248, 265, 281,
						296, 310, 323, 337, 352,
						368, 386, 403, 420, 436,
						451, 465, 480, 495, 511,
						528, 544, 560, 576, 592,
						608, 624, 640, 656, 672,
						688, 704, 720, 736, 752,
						768, 784, 800, 816, 831,
						847, 864, 882, 900, 920,
						940, 961, 982, 1003,
						1024};
int dnlp_scurv_hgh2[65] = {0, 4, 10, 20, 32,
						46, 61, 76, 92, 108,
						123, 138, 152, 166, 179,
						193, 208, 224, 240, 256,
						272, 286, 300, 314, 328,
						343, 359, 376, 392, 408,
						424, 440, 456, 472, 488,
						504, 520, 536, 552, 568,
						584, 600, 616, 632, 648,
						665, 681, 697, 712, 726,
						739, 753, 768, 784, 802,
						821, 840, 859, 879, 899,
						920, 943, 967, 994,
						1024};
/* NOTE: put the gain-var LUT in tunning tool, */
/* and get the var/8 as read-only for the image */

int gain_var_lut49[49] = {
	16,  64, 128, 192, 256, 320, 384, 384,
	384, 384, 384, 384, 384, 384, 384, 384,
	384, 384, 384, 384, 384, 384, 384, 384,
	384, 368, 352, 352, 336, 320, 304, 288,
	272, 256, 240, 224, 208, 192, 176, 160,
	144, 128, 112, 96, 96, 96, 96, 96, 96
};
int wext_gain[48] = {
	256, 256, 256, 256, 256, 256, 256, 256,
	248, 240, 232, 224, 216, 208, 200, 192,
	184, 176, 168, 160, 152, 144, 136, 128,
	124, 120, 116, 112, 108, 104, 100, 96,
	92, 88, 84, 80, 76, 72, 68, 64,
	60, 56, 52, 48, 44, 40, 36, 32
};

static int debug_add_curve_en;
module_param(debug_add_curve_en, int, 0664);
MODULE_PARM_DESC(debug_add_curve_en, "debug_add_curve_en");

static int glb_scurve_bld_rate;
module_param(glb_scurve_bld_rate, int, 0664);
MODULE_PARM_DESC(glb_scurve_bld_rate, "glb_scurve_bld_rate");

static int glb_clash_curve_bld_rate;
module_param(glb_clash_curve_bld_rate, int, 0664);
MODULE_PARM_DESC(glb_clash_curve_bld_rate, "glb_clash_curve_bld_rate");

static int glb_pst_gamma_bld_rate;
module_param(glb_pst_gamma_bld_rate, int, 0664);
MODULE_PARM_DESC(glb_pst_gamma_bld_rate, "glb_pst_gamma_bld_rate");

static int ve_dnlp_gmma_rate = 82;
module_param(ve_dnlp_gmma_rate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_gmma_rate,
		"ve_dnlp_gmma_rate");

static int ve_dnlp_lowalpha_v3 = 40;
module_param(ve_dnlp_lowalpha_v3, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lowalpha_v3,
		"ve_dnlp_lowalpha_v3");

static int ve_dnlp_hghalpha_v3 = 28;
module_param(ve_dnlp_hghalpha_v3, int, 0664);
MODULE_PARM_DESC(ve_dnlp_hghalpha_v3,
		"ve_dnlp_hghalpha_v3");


/* light -pattern 1 */
int ve_dnlp_slow_end = 2;
module_param(ve_dnlp_slow_end, int, 0664);
MODULE_PARM_DESC(ve_dnlp_slow_end,
		"ve_dnlp_slow_end");

/* define the black or white scence */
static int ve_dnlp_almst_wht = 63;
module_param(ve_dnlp_almst_wht, int, 0664);
MODULE_PARM_DESC(ve_dnlp_almst_wht, "define the white scence");


static int ve_dnlp_adpalpha_lrate = 32;
module_param(ve_dnlp_adpalpha_lrate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adpalpha_lrate, "ve_dnlp_adpalpha_lrate");

static int ve_dnlp_adpalpha_hrate = 32;
module_param(ve_dnlp_adpalpha_hrate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adpalpha_hrate, "ve_dnlp_adpalpha_hrate");

/*the maximum bins > x/256*/
static int ve_dnlp_lgst_ratio = 100;
module_param(ve_dnlp_lgst_ratio, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lgst_ratio, "dnlp: define it ratio to larget bin num for th to 2nd bin");

/*two maximum bins' distance*/
static int ve_dnlp_lgst_dst = 30;
module_param(ve_dnlp_lgst_dst, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lgst_dst, "dnlp: two maximum bins' distance");

/* debug difference level */
static int ve_dnlp_dbg_diflvl;
module_param(ve_dnlp_dbg_diflvl, int, 0664);
MODULE_PARM_DESC(ve_dnlp_dbg_diflvl,
		"ve_dnlp_dbg_diflvl");

static int ve_dnlp_scv_dbg;
module_param(ve_dnlp_scv_dbg, int, 0664);
MODULE_PARM_DESC(ve_dnlp_scv_dbg,
		"ve_dnlp_scv_dbg");

/* print log once */
static int ve_dnlp_ponce = 1;
module_param(ve_dnlp_ponce, int, 0664);
MODULE_PARM_DESC(ve_dnlp_ponce, "ve_dnlp_ponce");


unsigned int dnlp_printk;
module_param(dnlp_printk, uint, 0664);
MODULE_PARM_DESC(dnlp_printk, "dnlp_printk");
/*v3 dnlp end */

static bool hist_sel = 1; /*1->vpp , 0->vdin*/
module_param(hist_sel, bool, 0664);
MODULE_PARM_DESC(hist_sel, "hist_sel");

/* last frame status */
static int prev_dnlp_mvreflsh;
static int prev_dnlp_final_gain;
static int prev_dnlp_clahe_gain_neg;
static int prev_dnlp_clahe_gain_pos;
static int prev_dnlp_gmma_rate;
static int prev_dnlp_lowalpha_v3;
static int prev_dnlp_hghalpha_v3;
static int prev_dnlp_sbgnbnd;
static int prev_dnlp_sendbnd;
static int prev_dnlp_cliprate_v3;
static int prev_dnlp_clashBgn;
static int prev_dnlp_clashEnd;
static int prev_dnlp_mtdbld_rate;

static int prev_dnlp_blk_cctr;
static int prev_dnlp_brgt_ctrl;
static int prev_dnlp_brgt_range;
static int prev_dnlp_brght_add;
static int prev_dnlp_brght_max;
static int prev_dnlp_lgst_ratio;
static int prev_dnlp_lgst_dst;
static int prev_dnlp_almst_wht;

static int prev_dnlp_blkext_rate;
static int prev_dnlp_whtext_rate;
static int prev_dnlp_blkext_ofst;
static int prev_dnlp_whtext_ofst;
static int prev_dnlp_bwext_div4x_min;
static int prev_dnlp_schg_sft;
static bool prev_dnlp_smhist_ck;
static int prev_dnlp_cuvbld_min;
static int prev_dnlp_cuvbld_max;
static int prev_dnlp_dbg_map;
static bool prev_dnlp_dbg_adjavg;
static int prev_dnlp_dbg_i2r;
static int prev_dnlp_slow_end;
static int prev_dnlp_pavg_btsft;
static int prev_dnlp_cliprate_min;
static int prev_dnlp_adpcrat_lbnd;
static int prev_dnlp_adpcrat_hbnd;
static int prev_dnlp_adpmtd_lbnd;
static int	prev_dnlp_adpmtd_hbnd;
static int prev_dnlp_satur_rat;
static int prev_dnlp_satur_max;
static int prev_dnlp_set_saturtn;
static int prev_dnlp_lowrange;
static int prev_dnlp_hghrange;
static int prev_dnlp_auto_rng;
static int prev_dnlp_bbd_ratio_low;
static int prev_dnlp_bbd_ratio_hig;
static int prev_dnlp_adpalpha_lrate;
static int prev_dnlp_adpalpha_hrate;


static unsigned int pre_1_gamma[65];
static unsigned int pre_0_gamma[65];
/* more 2-bit */

/* why here? */
static bool dnlp_scn_chg; /* scene change */
static int dnlp_bld_lvl; /* blend level */
static int RBASE = 1;

static int gma_scurve0[65]; /* gamma0 s-curve */
static int gma_scurve1[65]; /* gamma1 s-curve */
static int gma_scurvet[65]; /* gmma0+gamm1 s-curve */
int GmScurve[65] = {0,  16,  32,  48,  64,  80,  96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  };/*pre-defined s curve,0~1024*/
int clash_curve[65] = {0,   16,   32,  48,  64,  80,  96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  }; /* clash curve */
int clsh_scvbld[65] = {0,   16,   32,  48,  64,  80,  96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  }; /* clash + s-curve blend */
int blk_gma_crv[65] = {0,   16,   32,  48,  64,  80,  96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  }; /* black gamma curve */
int blk_gma_bld[65] = {0,   16,   32,  48,  64,  80,  96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  }; /* blending with black gamma */
int blkwht_ebld[65] = {0,   16,   32,  48,  64,  80,  96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  }; /* black white extension */

/* only for debug */
static unsigned int premap0[65];
struct dnlp_alg_param_s dnlp_alg_param;
struct ve_dnlp_curve_param_s dnlp_curve_param_load;
struct dnlp_parse_cmd_s dnlp_parse_cmd[] = {
	{"alg_enable", &(dnlp_alg_param.dnlp_alg_enable)},
	{"respond", &(dnlp_alg_param.dnlp_respond)},
	{"sel", &(dnlp_alg_param.dnlp_sel)},
	{"respond_flag", &(dnlp_alg_param.dnlp_respond_flag)},
	{"smhist_ck", &(dnlp_alg_param.dnlp_smhist_ck)},
	{"mvreflsh", &(dnlp_alg_param.dnlp_mvreflsh)},
	{"pavg_btsft", &(dnlp_alg_param.dnlp_pavg_btsft)},
	{"dbg_i2r", &(dnlp_alg_param.dnlp_dbg_i2r)},
	{"cuvbld_min", &(dnlp_alg_param.dnlp_cuvbld_min)},
	{"cuvbld_max", &(dnlp_alg_param.dnlp_cuvbld_max)},
	{"schg_sft", &(dnlp_alg_param.dnlp_schg_sft)},
	{"bbd_ratio_low", &(dnlp_alg_param.dnlp_bbd_ratio_low)},
	{"bbd_ratio_hig", &(dnlp_alg_param.dnlp_bbd_ratio_hig)},
	{"limit_rng", &(dnlp_alg_param.dnlp_limit_rng)},
	{"range_det", &(dnlp_alg_param.dnlp_range_det)},
	{"blk_cctr", &(dnlp_alg_param.dnlp_blk_cctr)},
	{"brgt_ctrl", &(dnlp_alg_param.dnlp_brgt_ctrl)},
	{"brgt_range", &(dnlp_alg_param.dnlp_brgt_range)},
	{"brght_add", &(dnlp_alg_param.dnlp_brght_add)},
	{"brght_max", &(dnlp_alg_param.dnlp_brght_max)},
	{"dbg_adjavg", &(dnlp_alg_param.dnlp_dbg_adjavg)},
	{"auto_rng", &(dnlp_alg_param.dnlp_auto_rng)},
	{"lowrange", &(dnlp_alg_param.dnlp_lowrange)},
	{"hghrange", &(dnlp_alg_param.dnlp_hghrange)},
	{"satur_rat", &(dnlp_alg_param.dnlp_satur_rat)},
	{"satur_max", &(dnlp_alg_param.dnlp_satur_max)},
	{"set_saturtn", &(dnlp_alg_param.dnlp_set_saturtn)},
	{"sbgnbnd", &(dnlp_alg_param.dnlp_sbgnbnd)},
	{"sendbnd", &(dnlp_alg_param.dnlp_sendbnd)},
	{"clashBgn", &(dnlp_alg_param.dnlp_clashBgn)},
	{"clashEnd", &(dnlp_alg_param.dnlp_clashEnd)},
	{"var_th", &(dnlp_alg_param.dnlp_var_th)},
	{"clahe_gain_neg", &(dnlp_alg_param.dnlp_clahe_gain_neg)},
	{"clahe_gain_pos", &(dnlp_alg_param.dnlp_clahe_gain_pos)},
	{"clahe_gain_delta", &(dnlp_alg_param.dnlp_clahe_gain_delta)},
	{"mtdbld_rate", &(dnlp_alg_param.dnlp_mtdbld_rate)},
	{"adpmtd_lbnd", &(dnlp_alg_param.dnlp_adpmtd_lbnd)},
	{"adpmtd_hbnd", &(dnlp_alg_param.dnlp_adpmtd_hbnd)},
	{"blkext_ofst", &(dnlp_alg_param.dnlp_blkext_ofst)},
	{"whtext_ofst", &(dnlp_alg_param.dnlp_whtext_ofst)},
	{"blkext_rate", &(dnlp_alg_param.dnlp_blkext_rate)},
	{"whtext_rate", &(dnlp_alg_param.dnlp_whtext_rate)},
	{"bwext_div4x_min", &(dnlp_alg_param.dnlp_bwext_div4x_min)},
	{"iRgnBgn", &(dnlp_alg_param.dnlp_iRgnBgn)},
	{"iRgnEnd", &(dnlp_alg_param.dnlp_iRgnEnd)},
	{"dbg_map", &(dnlp_alg_param.dnlp_dbg_map)},
	{"final_gain", &(dnlp_alg_param.dnlp_final_gain)},
	{"cliprate_v3", &(dnlp_alg_param.dnlp_cliprate_v3)},
	{"cliprate_min", &(dnlp_alg_param.dnlp_cliprate_min)},
	{"adpcrat_lbnd", &(dnlp_alg_param.dnlp_adpcrat_lbnd)},
	{"adpcrat_hbnd", &(dnlp_alg_param.dnlp_adpcrat_hbnd)},
	{"scurv_low_th", &(dnlp_alg_param.dnlp_scurv_low_th)},
	{"scurv_mid1_th", &(dnlp_alg_param.dnlp_scurv_mid1_th)},
	{"scurv_mid2_th", &(dnlp_alg_param.dnlp_scurv_mid2_th)},
	{"scurv_hgh1_th", &(dnlp_alg_param.dnlp_scurv_hgh1_th)},
	{"scurv_hgh2_th", &(dnlp_alg_param.dnlp_scurv_hgh2_th)},
	{"mtdrate_adp_en", &(dnlp_alg_param.dnlp_mtdrate_adp_en)},
	{"", NULL}
};



void dnlp_alg_param_init(void)
{
	dnlp_alg_param.dnlp_alg_enable = 0;
	dnlp_alg_param.dnlp_respond = 0;
	dnlp_alg_param.dnlp_sel = 2;
	dnlp_alg_param.dnlp_respond_flag = 0;
	dnlp_alg_param.dnlp_smhist_ck = 0;
	dnlp_alg_param.dnlp_mvreflsh = 6;
	dnlp_alg_param.dnlp_pavg_btsft = 5;
	dnlp_alg_param.dnlp_dbg_i2r = 255;
	dnlp_alg_param.dnlp_cuvbld_min = 3;
	dnlp_alg_param.dnlp_cuvbld_max = 17;
	dnlp_alg_param.dnlp_schg_sft = 1;
	dnlp_alg_param.dnlp_bbd_ratio_low = 16;
	dnlp_alg_param.dnlp_bbd_ratio_hig = 128;
	dnlp_alg_param.dnlp_limit_rng = 1;
	dnlp_alg_param.dnlp_range_det = 0;
	dnlp_alg_param.dnlp_blk_cctr = 8;
	dnlp_alg_param.dnlp_brgt_ctrl = 48;
	dnlp_alg_param.dnlp_brgt_range = 16;
	dnlp_alg_param.dnlp_brght_add = 32;
	dnlp_alg_param.dnlp_brght_max = 0;
	dnlp_alg_param.dnlp_dbg_adjavg = 0;
	dnlp_alg_param.dnlp_auto_rng = 0;
	dnlp_alg_param.dnlp_lowrange = 18;
	dnlp_alg_param.dnlp_hghrange = 18;
	dnlp_alg_param.dnlp_satur_rat = 30;
	dnlp_alg_param.dnlp_satur_max = 40;
	dnlp_alg_param.dnlp_set_saturtn = 0;
	dnlp_alg_param.dnlp_sbgnbnd = 4;
	dnlp_alg_param.dnlp_sendbnd = 4;
	dnlp_alg_param.dnlp_clashBgn = 4;
	dnlp_alg_param.dnlp_clashEnd = 59;
	dnlp_alg_param.dnlp_var_th = 16;
	dnlp_alg_param.dnlp_clahe_gain_neg = 120;
	dnlp_alg_param.dnlp_clahe_gain_pos = 24;
	dnlp_alg_param.dnlp_clahe_gain_delta = 32;
	dnlp_alg_param.dnlp_mtdbld_rate = 40;
	dnlp_alg_param.dnlp_adpmtd_lbnd = 19;
	dnlp_alg_param.dnlp_adpmtd_hbnd = 20;
	dnlp_alg_param.dnlp_blkext_ofst = 2;
	dnlp_alg_param.dnlp_whtext_ofst = 1;
	dnlp_alg_param.dnlp_blkext_rate = 32;
	dnlp_alg_param.dnlp_whtext_rate = 16;
	dnlp_alg_param.dnlp_bwext_div4x_min = 16;
	dnlp_alg_param.dnlp_iRgnBgn = 0;
	dnlp_alg_param.dnlp_iRgnEnd = 64;
	dnlp_alg_param.dnlp_dbg_map = 0;
	dnlp_alg_param.dnlp_final_gain = 8;
	dnlp_alg_param.dnlp_cliprate_v3 = 36;
	dnlp_alg_param.dnlp_cliprate_min = 19;
	dnlp_alg_param.dnlp_adpcrat_lbnd = 10;
	dnlp_alg_param.dnlp_adpcrat_hbnd = 20;
	dnlp_alg_param.dnlp_scurv_low_th = 32;
	dnlp_alg_param.dnlp_scurv_mid1_th = 48;
	dnlp_alg_param.dnlp_scurv_mid2_th = 112;
	dnlp_alg_param.dnlp_scurv_hgh1_th = 176;
	dnlp_alg_param.dnlp_scurv_hgh2_th = 240;
	dnlp_alg_param.dnlp_mtdrate_adp_en = 1;
}

static void ve_dnlp_add_cm(unsigned int value)
{
	unsigned int reg_value;
	VSYNC_WR_MPEG_REG(VPP_CHROMA_ADDR_PORT, 0x207);
	reg_value = VSYNC_RD_MPEG_REG(VPP_CHROMA_DATA_PORT);
	reg_value = (reg_value & 0xf000ffff) | (value << 16);
	VSYNC_WR_MPEG_REG(VPP_CHROMA_ADDR_PORT, 0x207);
	VSYNC_WR_MPEG_REG(VPP_CHROMA_DATA_PORT, reg_value);
}

void GetWgtLst(ulong *iHst, ulong tAvg, ulong nLen, ulong alpha)
{
	ulong iMax = 0;
	ulong iMin = 0;
	ulong iPxl = 0;
	ulong iT = 0;
	for (iT = 0; iT < nLen; iT++) {
		iPxl = iHst[iT];
		if (iPxl > tAvg) {
			iMax = iPxl;
			iMin = tAvg;
		} else {
			iMax = tAvg;
			iMin = iPxl;
		}
		if (alpha < 16) {
			iPxl = ((16-alpha)*iMin+8)>>4;
			iPxl += alpha*iMin;
		} else if (alpha < 32) {
			iPxl = (32-alpha)*iMin;
			iPxl += (alpha-16)*iMax;
		} else {
			iPxl = (48-alpha)+4*(alpha-32);
			iPxl *= iMax;
		}
		iPxl = (iPxl+8)>>4;
		iHst[iT] = iPxl < 1 ? 1 : iPxl;
	}
}

/*rGmIn[0:64]   ==>0:4:256, gamma*/
/*rGmOt[0:pwdth]==>0-0, 0~1024*/
void GetSubCurve(unsigned int *rGmOt,
		unsigned int *rGmIn, unsigned int pwdth)
{
	int nT0 = 0;
	unsigned int BASE = 64;

	unsigned int plft = 0;
	unsigned int prto = 0;
	unsigned int rst = 0;

	unsigned int idx1 = 0;
	unsigned int idx2 = 0;

	if (pwdth == 0)
		pwdth = 1;

	for (nT0 = 0; nT0 <= pwdth; nT0++) {
		plft = nT0*64/pwdth;
		prto = (BASE*(nT0*BASE-plft*pwdth) + pwdth/2)/pwdth;

		idx1 = plft;
		idx2 = plft+1;
		if (idx1 > 64)
			idx1 = 64;
		if (idx2 > 64)
			idx2 = 64;

		rst = rGmIn[idx1]*(BASE-prto) + rGmIn[idx2]*prto;
		rst = (rst + BASE/2)*4*pwdth/BASE;
		/* rst = ((rst + 128)>>8); */
		rst = ((rst + 32)>>6);

		if (nT0 == 0)
			rst = rGmIn[0];
		if (rst > (pwdth << 4))
			rst = (pwdth << 4);

		rGmOt[nT0] = rst;
	}
}

/*rGmOt[0:64]*/
/*rGmIn[0:64]*/
/* 0 ~1024 */
void GetGmCurves2(unsigned int *rGmOt, unsigned int *rGmIn,
		unsigned int pval, unsigned int BgnBnd, unsigned int EndBnd)
{
	int nT0 = 0;
	/*unsigned int rst=0;*/
	int pwdth = 0;
	unsigned int pLst[65];
	int nT1 = 0;

	if (pval >= ve_dnlp_almst_wht) {
		/*almost white scene, do not S_curve*/
		for (nT0 = 0; nT0 < 65; nT0++)
			rGmOt[nT0] = rGmIn[nT0] << 2;
		return;
	}

	if (BgnBnd > 10)
		BgnBnd = 10; /* 4 */
	if (EndBnd > 10)
		EndBnd = 10; /* 4 */

	if (pval < ve_dnlp_slow_end)
		pval = ve_dnlp_slow_end;

	if (pval <= BgnBnd)
		pval = BgnBnd + 1;/* very black */

	if (pval + EndBnd > 64)
		pval = 63 - EndBnd; /* close wht */

	for (nT0 = 0; nT0 < 65; nT0++)
		rGmOt[nT0] = (nT0<<4); /* 0~1024 */  /* init 45 dgree line */

	if (pval > BgnBnd) {
		pwdth = pval - BgnBnd;

		GetSubCurve(pLst, rGmIn, pwdth); /* 0~1024 */

		for (nT0 = BgnBnd; nT0 <= pval; nT0++) {
			/* <luma_avg band curve,arch down */
			nT1 = pLst[nT0 - BgnBnd] + (BgnBnd<<4);
			rGmOt[nT0] = nT1;

			if ((dnlp_printk >> 17) & 0x1)
				pr_dnlp_dbg("\n#***down_cur::(index)%02d: (pLst) %4d => (down_cur) %4d(%4d)\n",
				nT0, pLst[nT0 - BgnBnd], nT1, nT0<<4);
		}
	}

	if (pval + EndBnd < 64) {
		pwdth = 64 - pval - EndBnd;
		GetSubCurve(pLst, rGmIn, pwdth);

		for (nT0 = pval; nT0 <= 64 - EndBnd; nT0++) {
			nT1 = (1024 - (EndBnd<<4)
				- pLst[pwdth - (nT0 - pval)]);
			rGmOt[nT0] = nT1;  /* 0~1024 */
			/* >luma_avg band curve,arch up */
		}
	}

	if ((dnlp_printk >> 17) & 0x1) {
		pr_dnlp_dbg("\n#---GmCvs2: BgnBnd(%d) pval(%d,luma_avg) EndBnd(%d)\n",
			BgnBnd, pval, EndBnd);
		pr_dnlp_dbg("[index] => curve(45line)\n");
		for (nT1 = 0; nT1 < 65; nT1++)
			pr_dnlp_dbg("[%02d] => %4d(%4d)\n",
				nT1, rGmOt[nT1], nT1<<4);
		pr_dnlp_dbg("\n");
	}
}

/*	in: rGmIn,lsft_avg,BgnBnd,EndBnd*/
/*   out: rGmOt  */
void GetGmCurves(unsigned int *rGmOt, unsigned int *rGmIn,
	unsigned int lsft_avg, unsigned int BgnBnd, unsigned int EndBnd)
{
	/* luma_avg4, */
	int pval1 = (lsft_avg >> (2 + dnlp_alg_param.dnlp_pavg_btsft));
	int pval2 = pval1 + 1;
	int BASE  = (1 << (2 + dnlp_alg_param.dnlp_pavg_btsft));
	/* = 0; what for ? */
	int nRzn = lsft_avg - (pval1 << (2 + dnlp_alg_param.dnlp_pavg_btsft));
	unsigned int pLst1[65];
	unsigned int pLst2[65];
	int i = 0;
	int j = 0;

	if (pval2 > ve_dnlp_almst_wht)
		pval2 = ve_dnlp_almst_wht;

	GetGmCurves2(pLst1, rGmIn, pval1, BgnBnd, EndBnd);
	GetGmCurves2(pLst2, rGmIn, pval2, BgnBnd, EndBnd);
	if ((dnlp_printk >> 17) & 0x1)
		pr_dnlp_dbg("GetGmCurves 0/1: pval1=%d, pval2=%d\n",
			pval1, pval2);

	for (i = 0; i < 65; i++) {
		/* pLst1, pLst2 blend:blend level nRzn(0) */
		pval2 = pLst1[i] * (BASE - nRzn) + pLst2[i] * nRzn;
		pval2 = (pval2 + (BASE >> 1));
		pval2 =  (pval2 >> (2 + dnlp_alg_param.dnlp_pavg_btsft));

		if (pval2 < 0)
			pval2 = 0;
		else if (pval2 > 1023)
			pval2 = 1023;
		rGmOt[i] = pval2;
	}

	if ((dnlp_printk >> 17) & 0x1) {
		/* iHst */
		pr_dnlp_dbg("\n=====gma_s: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d\n"
				"%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
				rGmOt[i], rGmOt[i+1],
				rGmOt[i+2], rGmOt[i+3],
				rGmOt[i+4], rGmOt[i+5],
				rGmOt[i+6], rGmOt[i+7],
				rGmOt[i+8], rGmOt[i+9],
				rGmOt[i+10], rGmOt[i+11],
				rGmOt[i+12], rGmOt[i+13],
				rGmOt[i+14], rGmOt[i+15]);
		}
		pr_dnlp_dbg("%d ]\n", rGmOt[64]);
	}

}


/*
 *function: choose pre-defined s curve based on APL(luma_avg4)
 *in		: luma_avg(0~255),sBgnBnd,sEndBnd
 *out		:  the chosen s curve
 */
void GetGmScurve_apl_var(unsigned int *rGmOt, unsigned int luma_avg4,
		int var, unsigned int sBgnBnd, unsigned int sEndBnd)
{
	int nTmp, nFrac, i, j;
	static unsigned int pre_scurve[65] = {
		 0, 16, 32, 48, 64, 80, 96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  };
	int scurv_gain, left, right, var_d8, frac, norm, idx;

		/* typical histo <==> var	<==> var/8 */
	/*1-bin 0 0 */
	/*3-bin 8 1 */
	/*5-bin 16 2 */
	/*7-bin 32 4 */
	/*9-bin	52 7 */
	/*13-bin 112 15 */
	/*11-bin 83	11 */
	/*16-bin 204 25 */
	/*32-bin 748 93.5 */
	/*ramp 2500 325*/
	/* BW50% 8192 1024 */
	nTmp = 0;

	/* get the scurv_gain from lut via the var/8, piece wised */
	var_d8 = (var>>3);
	/*for read on tools*/
	ro_var_d8 = var_d8;
	frac = 0;
	nFrac = 0;
	if (var < 128) {
		idx = var_d8;
		nFrac = var -  0;
		frac = nFrac - ((nFrac >> 3) << 3);
		norm = 3;
	} else if (var < 640) {
		idx = 16 + ((var_d8 - 16) >> 2);
		nFrac = var - 128;
		frac = nFrac - ((nFrac >> 5) << 5);
		norm = 5;
	} else if (var < 2560) {
		idx = 32 + ((var_d8 - 80) >> 4);
		nFrac = var - 640;
		frac = nFrac - ((nFrac >> 7) << 7);
		norm = 7;
	} else {
		idx = 47;
		frac = 0;
		norm = 7;
	}
	left = gain_var_lut49[idx];
	right = gain_var_lut49[idx + 1];
	scurv_gain = (left * ((1 << norm) - frac) +
		right*frac + (1 << (norm - 1))) >> (norm);
	/*for read on tools*/
	ro_scurv_gain = scurv_gain;

	if ((dnlp_printk >> 3) & 0x1)
		pr_dnlp_dbg("@@@ GetGmScurve()--in: hist_var = %d\n"
			"luma_avg4(255) = %d, sBgn_Bnd,end = [%d %d]\n\n",
			var, luma_avg4, sBgnBnd, sEndBnd);
	if ((dnlp_printk >> 3) & 0x1)
		pr_dnlp_dbg("@@@ GetGmScurve()--scurv_gain: nFrac = %d,frac = %d\n"
			"idx= %d, left,right= [%d %d] => scurv_gain = %d\n\n",
			nFrac, frac, idx, left, right, scurv_gain);

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pre_scurve[i] = (i << 4); /* 0 ~1024 */
	}

	if (luma_avg4 > 255)
		luma_avg4 = 255;/* luma_avg = 0~255 */

	if ((dnlp_printk >> 3) & 0x1)
		pr_dnlp_dbg("@@@ luma_avg4:%d, 5_curv_th:[ %d,%d,%d,%d,%d]",
			luma_avg4, dnlp_alg_param.dnlp_scurv_low_th,
			dnlp_alg_param.dnlp_scurv_mid1_th,
			dnlp_alg_param.dnlp_scurv_mid2_th,
			dnlp_alg_param.dnlp_scurv_hgh1_th,
			dnlp_alg_param.dnlp_scurv_hgh2_th);

	/* choose s curve base on luma_avg */
	for (i = 0; i < 65; i++) {
		if (i <= sBgnBnd)
			nTmp  = i << 4;
		else if (i >= (64 - sBgnBnd))
			nTmp  = i << 4;
		else{
			if (luma_avg4 < dnlp_alg_param.dnlp_scurv_low_th)
				nTmp = dnlp_scurv_low[i];
			else if (luma_avg4 >= dnlp_alg_param.dnlp_scurv_hgh2_th)
				nTmp   = dnlp_scurv_hgh2[i];
			/* linear interpolations */
			else {
				if (luma_avg4 <
					dnlp_alg_param.dnlp_scurv_mid1_th) {
					left = dnlp_scurv_low[i];
					right = dnlp_scurv_mid1[i];
					frac = luma_avg4 -
					dnlp_alg_param.dnlp_scurv_low_th;
					norm = dnlp_alg_param.dnlp_scurv_mid1_th
					- dnlp_alg_param.dnlp_scurv_low_th;
				} else if (luma_avg4 <
					dnlp_alg_param.dnlp_scurv_mid2_th) {
					left = dnlp_scurv_mid1[i];
					right = dnlp_scurv_mid2[i];
					frac = luma_avg4 -
					dnlp_alg_param.dnlp_scurv_mid1_th;
					norm = dnlp_alg_param.dnlp_scurv_mid2_th
					- dnlp_alg_param.dnlp_scurv_mid1_th;
				} else if (luma_avg4 <
					dnlp_alg_param.dnlp_scurv_hgh1_th) {
					left = dnlp_scurv_mid2[i];
					right = dnlp_scurv_hgh1[i];
					frac = luma_avg4 -
					dnlp_alg_param.dnlp_scurv_mid2_th;
					norm =
					dnlp_alg_param.dnlp_scurv_hgh1_th
					- dnlp_alg_param.dnlp_scurv_mid2_th;
				} else {
					left = dnlp_scurv_hgh1[i];
					right = dnlp_scurv_hgh2[i];
					frac = luma_avg4 -
					dnlp_alg_param.dnlp_scurv_hgh1_th;
					norm = dnlp_alg_param.dnlp_scurv_hgh2_th
					- dnlp_alg_param.dnlp_scurv_hgh1_th;
				}
			/* linear blending base on APL	(Double check) */
			nTmp = (left*(norm - frac) +
				right*frac + (norm>>1))/norm;

		if ((dnlp_printk >> 3) & 0x1)
			pr_dnlp_dbg("@@@ apl_var():[%d]:lavg4=%d,left=%d, right=%d, frac=%d,norm=%d => nTmp=%d\n",
				i, luma_avg4, left, right, frac, norm, nTmp);

			}
		}

		/* apply a gain based on hist variance,*/
		/*	norm 256 to "1" */
		nTmp = (i << 4) +
			((nTmp - (i << 4)) * scurv_gain + 128)/256;

	if ((dnlp_printk >> 3) & 0x1)
		pr_dnlp_dbg("@@@ apl_var():[%d]:i4=%d,sgain=%d => out=%d\n",
			i, i << 4, scurv_gain, nTmp);

		/* clip */
		if (nTmp < 0)
			nTmp = 0;
		else if (nTmp > 1023)
			nTmp = 1023;

		/* output */
		rGmOt[i] = nTmp;
	}

	if (!dnlp_scn_chg)
		for (i = 0; i < 65; i++) {
			nTmp = dnlp_bld_lvl * rGmOt[i] + (RBASE >> 1); /*1024 */
			nTmp = nTmp + (RBASE - dnlp_bld_lvl) * pre_scurve[i];
			nTmp = (nTmp >> dnlp_alg_param.dnlp_mvreflsh);

			if (nTmp < 0)
				nTmp = 0;
			else if (nTmp > 1023)
				nTmp = 1023;

			if ((dnlp_printk >> 3) & 0x1)
				pr_dnlp_dbg(" @@@ apl_var(iir): %d, rGmOt[i]=%d, pre_scurve=%d => crt=%d\n",
					i, rGmOt[i], pre_scurve[i], nTmp);

			rGmOt[i] = nTmp;
		}

	for (i = 0; i < 65; i++)
		pre_scurve[i] = rGmOt[i];

	if ((dnlp_printk >> 13) & 0x1) {
		/* iHst */
		pr_dnlp_dbg("@@@ GetGmScurve()--out,chosen curve: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
						rGmOt[i], rGmOt[i+1],
						rGmOt[i+2], rGmOt[i+3],
						rGmOt[i+4], rGmOt[i+5],
						rGmOt[i+6], rGmOt[i+7],
						rGmOt[i+8], rGmOt[i+9],
						rGmOt[i+10], rGmOt[i+11],
						rGmOt[i+12], rGmOt[i+13],
						rGmOt[i+14], rGmOt[i+15]);
		}
		pr_dnlp_dbg("%d ]\n", rGmOt[64]);
	}
}


unsigned int cal_hist_avg(unsigned int pval)
{
	static int ppval;
	int ipval = (int) pval;
	int tmp = ipval;

	if (!dnlp_scn_chg) {
		tmp = dnlp_bld_lvl * ipval + (RBASE >> 1);
		tmp = tmp + (RBASE - dnlp_bld_lvl) * ppval;
		tmp = (tmp >> dnlp_alg_param.dnlp_mvreflsh);
	}
	ppval = tmp;

	return (unsigned int)tmp;
}


/* history */
unsigned int cal_hst_shft_avg(unsigned int pval)
{
	static int ppval;
	int ipval = (int)pval;
	int tmp = ipval;

	if (!dnlp_scn_chg) {
		tmp = dnlp_bld_lvl * ipval + (RBASE >> 1);
		tmp = tmp + (RBASE - dnlp_bld_lvl) * ppval;
		tmp = (tmp >> dnlp_alg_param.dnlp_mvreflsh);
	}
	ppval = tmp;
	return (unsigned int)tmp;
}


/*   in : mtdrate, luma_avg(0~64)*/
/*  return: mtdrate-- mtdrate in the mid-tone*/
static int dnlp_adp_mtdrate(int mtdrate, int luma_avg)
{
	int np = 0;
	int nt = mtdrate;

	if (dnlp_alg_param.dnlp_mtdrate_adp_en) {
		if (dnlp_alg_param.dnlp_adpmtd_lbnd > 0 &&
			luma_avg < dnlp_alg_param.dnlp_adpmtd_lbnd) {
			nt = mtdrate * luma_avg +
				(dnlp_alg_param.dnlp_adpmtd_lbnd >> 1);
			nt /= dnlp_alg_param.dnlp_adpmtd_lbnd;

			np = 4 * (dnlp_alg_param.dnlp_adpmtd_lbnd - luma_avg);
			if (np < mtdrate)
				np = mtdrate - np;
			else
				np = 0;

			if (np > nt)
				nt = np;
		} else if (dnlp_alg_param.dnlp_adpmtd_hbnd > 0 &&
			(luma_avg > 64 - dnlp_alg_param.dnlp_adpmtd_hbnd)) {
			nt = mtdrate * (64 - luma_avg);
			nt += (dnlp_alg_param.dnlp_adpmtd_hbnd >> 1);
			nt /= dnlp_alg_param.dnlp_adpmtd_hbnd;

			np = luma_avg - (64 - dnlp_alg_param.dnlp_adpmtd_hbnd);
			np = 4 * np;
			if (np < mtdrate)
				np = mtdrate - np;
			else
				np = 0;

			if (np > nt)
				nt = np;
		}
		if ((dnlp_printk >> 6) & 0x1)
			pr_dnlp_dbg("## adp_mtdrate(): imtdrate=%d, luma_avg=%d,ve_adpmtd_bnd=[%d %d], nt(mtdrate)=%d (np=%d)\n ",
			mtdrate, luma_avg, dnlp_alg_param.dnlp_adpmtd_lbnd,
			dnlp_alg_param.dnlp_adpmtd_hbnd, nt, np);
	} else
		nt = dnlp_alg_param.dnlp_mtdbld_rate;

	return nt;
}


unsigned int AdjHistAvg(unsigned int pval, unsigned int ihstEnd)
{
	unsigned int pEXT = 224;
	unsigned int pMid = 128;
	unsigned int pMAX = 236;

	if (ihstEnd > 59)
		pMAX = ihstEnd << 2;

	if (pval > pMid) {
		pval = pMid + (pMAX - pMid)*(pval - pMid)/(pEXT - pMid);
		if (pval > pMAX)
			pval = pMAX;
	}

	return pval;
}

/*function:  black white extension parameter calculation */
/*		in: iHstBgn,iHstEnd,hstSum,lmh_lavg */
/*		out : blk_wht_ext[0],blk_wht_ext[1] */
/*			 [0]: black level in u10 (0~1023) */
/*			 [1]: white level in u10 (0~1023) */
void cal_bwext_param(int *blk_wht_ext, unsigned int iHstBgn,
	unsigned int iHstEnd, int hstSum, unsigned int *lmh_lavg)
{
	/* black / white extension, for IIR*/
	static int pblk_wht_extx16[2];
	int nT0, nT1;

	/* black extension */
	if (dnlp_alg_param.dnlp_blkext_rate > 0) {
		blk_wht_ext[0] = (iHstBgn < lmh_lavg[0]) ? iHstBgn:lmh_lavg[0];
		blk_wht_ext[0] = (blk_wht_ext[0] > 16) ? 16 :
			(blk_wht_ext[0] < 0) ? 0 : blk_wht_ext[0];
	} else
		blk_wht_ext[0] = 0;
	/* white extension */
	if (dnlp_alg_param.dnlp_whtext_rate > 0) {
		blk_wht_ext[1] = (iHstEnd > lmh_lavg[2]) ? iHstEnd:lmh_lavg[2];
		blk_wht_ext[1] = (blk_wht_ext[1] < 48) ? 48 :
			(blk_wht_ext[1] > 63) ? 63 : blk_wht_ext[1];
	} else
		blk_wht_ext[1] = 63;

	if ((dnlp_printk >> 5) & 0x1)
		pr_dnlp_dbg("&&&&-1: iHstBgn-End = [%d,%d],blk_wht_ext[2] = [%d,%d],lmh_avg[L H]=[%d, %d]\n",
		iHstBgn, iHstEnd, blk_wht_ext[0], blk_wht_ext[1],
		lmh_lavg[0], lmh_lavg[2]);

	blk_wht_ext[0] = blk_wht_ext[0] << 4;/*u10*/
	blk_wht_ext[1] = blk_wht_ext[1] << 4;/*u10*/

	/* do IIR if no scene change to keep it stable */
	if (!dnlp_scn_chg) {
		nT0 = dnlp_bld_lvl * blk_wht_ext[0] + (RBASE >> 1);
		nT0 = nT0 + (RBASE - dnlp_bld_lvl) * pblk_wht_extx16[0];
		nT0 = (nT0 >> dnlp_alg_param.dnlp_mvreflsh);
		nT1 = dnlp_bld_lvl * blk_wht_ext[1] + (RBASE >> 1);
		nT1 = nT1 + (RBASE - dnlp_bld_lvl) * pblk_wht_extx16[1];
		nT1 = (nT1 >> dnlp_alg_param.dnlp_mvreflsh);

	if ((dnlp_printk >> 5) & 0x1)
		pr_dnlp_dbg("&&&&(iir):bwext[2] = [%d,%d], pbw_extx16[2]=  [%d,  %d] = > cur bw_ext[2]=[%d,%d]\n ",
		blk_wht_ext[0], blk_wht_ext[1],
		pblk_wht_extx16[0], pblk_wht_extx16[1],
		nT0, nT1);
	blk_wht_ext[0] = nT0;/* u10 */
	blk_wht_ext[1] = nT1;/* u10 */

	}
		/* output, plus fixed offset */
	pblk_wht_extx16[0] = blk_wht_ext[0];/* u10 */
	pblk_wht_extx16[1] = blk_wht_ext[1];/* u10 */

	if ((dnlp_printk >> 5) & 0x1)
		pr_dnlp_dbg("&&&&-2(iir):blk_wht_ext[2] = [%d,%d](u10)\n",
		blk_wht_ext[0], blk_wht_ext[1]);

}


static unsigned int dnlp_adp_cliprate(unsigned int clip_rate,
	unsigned int clip_rmin, unsigned int luma_avg)
{
	unsigned int nt = clip_rate;
	/* luma_avg is very low */
	if (luma_avg < dnlp_alg_param.dnlp_adpcrat_lbnd) {
		/* clip_rate blend clip_rmin, blend level:luma_avg */
		nt = clip_rate * (dnlp_alg_param.dnlp_adpcrat_lbnd - luma_avg) +
			 clip_rmin * luma_avg;
		nt = (nt << 4) + (dnlp_alg_param.dnlp_adpcrat_lbnd >> 1);
		nt /= dnlp_alg_param.dnlp_adpcrat_lbnd;
	} else if (luma_avg > 64 - dnlp_alg_param.dnlp_adpcrat_hbnd) {
		nt = clip_rmin * (64 - luma_avg) + clip_rate *
			(luma_avg + dnlp_alg_param.dnlp_adpcrat_hbnd - 64);
		nt += (nt << 4) + (dnlp_alg_param.dnlp_adpcrat_hbnd >> 1);
		nt /= dnlp_alg_param.dnlp_adpcrat_hbnd;
	} else
		nt = (clip_rmin << 4);

	return nt;
}

/*get the hist based curves, 32 nodes*/
void get_hist_cross_curve(unsigned int *oMap, unsigned int *iHst, int hist_sum,
		unsigned int hstBgn, unsigned int hstEnd)
{
	int i, j, t, tt, k, kk, xidx, xidx_prev, xnum, up_idx, dn_idx;
	int nTmp, nDif, sum, c, m, p, m3, m4, p2, p3, ofset;

	/*variables*/
	/*normalized 1:1 curve*/
	int curv_121n_node[33] = {   16, 48, 80, 112, 144, 176, 208, 240,
			272, 304, 336, 368, 400, 432, 464, 496,
			528, 560, 592, 624, 656, 688, 720, 752,
			784, 816, 848, 880, 912, 944, 976, 1008, 1032};
	/*32 node version of the oMap curve*/
	int curv_hist_node[33];
	/*average bin value.*/
	int hist_avg = hist_sum >> 6;
	int adp_thrd[33] = {13, 13, 13, 13, 13, 13, 13, 14,
		14, 14, 14, 14, 15, 15, 15, 15,
		15, 14, 14, 14, 14, 14, 13, 12,
		11, 10,  9,  8,  7,  6,  5,  4,  4};

	int trend[32];
	int xross_xidx[9], xross_slop[9], xross_ofst[9], xross_num;
	int xidx_rightmost_valid_hist = 0;
	int xidx_final_peak, max_shift;
	int maxbin_2ndabove, lpfbin_1stpeak, bb_protect;
	int bin_curr, bin_righ;
	int bin_lpf8, bin_max, bin_max_idx, bin_scl, bin_clip;
	int lmt_slope, slope_1st, slope_c, slope_p;
	int slope_step_norm, slope_step_c, slope_step_p;
	int delta_slop_step_c, delta_slop_step_p;
	int boost_ofst;
	int num_2, num_1, num_x, sum_step_2;
	int sum_step_1, sum_step_x, sum_step_x_min;
	int rat_1, rat_2, dlt_1, dlt_2, sub_x;
	int accum, num_xx, up_dn_dist, st_idx;
	int hist_lpf[33];


	/*programmable registers*/
	/*maximum slop of the first node, normalize to 256 as 1.0*/
	int reg_max_slop_1st = 614;
	/*maximum slop of the middle node, normalize to 256 as 1.0*/
	int reg_max_slop_mid = 400;
	/*maximum slop of the last node, normalize to 256 as 1.0*/
	int reg_max_slop_fin = 614;
	/*minimum slop of the first node, normalize to 256 as 1.0*/
	int reg_min_slop_1st =  77;
	/*minimum slop of the middle node, normalize to 256 as 1.0*/
	int reg_min_slop_mid = 144;
	/*minimum slop of the final node, normalize to 256 as 1.0*/
	int reg_min_slop_fin =  77;

	/*(8+6)*/
	int reg_no_trend_margin = (12 * hist_sum) >> 14;
	/*oMap offset on the first 12 nodes*/
	int reg_blk_boost_12[13] = {0, 8, 24, 40, 48,
		28, 12, 8,  5, 3, 2, 0, 0};
	/*normalized to 32 as 1.0 */
	int reg_adp_ofset_20[20] = {0, 0,  1,  2,  3,  3,  4, 5,  6, 7,
					8, 8,  7,  6,  5,  4,  3, 2,  1, 0};
	/*normalized to 256 as 1.0*/
	int reg_mono_protect[6] = {307, 332, 358, 371, 460, 560};
	/*mono color raster protection nodes range, */
	/*	only nodes within this range can be protected0~31 nodes*/
	int reg_mono_binrang[2] = {7, 26};
	/* min_num of transition nodes during local peak;*/
	int reg_trend_wht_expand_mode = 2;
	/*min_num of transition nodes during local peak*/
	int reg_trend_blk_expand_mode = 2;
	/*from 23 nodes to 30 nodes, the last peak maximum right shift step*/
	int reg_trend_wht_expand_lut8[9] = {0, 2, 4, 6, 5, 4, 3, 2, 0};

	/*initial */
	m = 8;
	for (k = 0; k < 33; k++)
		curv_hist_node[k] = curv_121n_node[k];
	for (k = 0; k < 9; k++) {
		xross_xidx[k] = -1;
		xross_slop[k] = 256;
		xross_ofst[k] = 0;
	}

	/*input params*/
	if ((dnlp_printk >> 14) & 0x1) {
		pr_dnlp_dbg("\n=====hist_raw: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
					iHst[i], iHst[i+1],
					iHst[i+2], iHst[i+3],
					iHst[i+4], iHst[i+5],
					iHst[i+6], iHst[i+7],
					iHst[i+8], iHst[i+9],
					iHst[i+10], iHst[i+11],
					iHst[i+12], iHst[i+13],
					iHst[i+14], iHst[i+15]);
		}
	pr_dnlp_dbg("]\n");
	pr_dnlp_dbg("hist_sum(iir) = %d\n", hist_sum);
	}
	/*step 4.0 gen:  adp_thrd[33], hist_lpf[33], trend[32]*/
	/* [1 1 1 1 0]/4 lpf filter*/
	hist_lpf[0] = ((iHst[0] << 1) + iHst[1] + iHst[2]) >> 2;
	for (xidx = 0; xidx < 32; xidx++) {
		/*generate the adp_thrd*/
		/*adaptive threshold to decide if */
		/* that node is valid hist or not.*/
		adp_thrd[xidx] = (adp_thrd[xidx] * hist_avg) >> 6;

		/*generate the hist_lpf_righ[]*/
		sum = 0;
		for (i = -2; i < 2; i++) {
			j = (xidx<<1) + 3 + i;   /*righ 2*(xidx+1)+1*/
			kk = (j > 63) ? 63 : j; /*0~63*/
			sum += (int) iHst[kk];
		}
		hist_lpf[xidx + 1] = (sum >> 2);

		/*get the bin_lpf*/
		bin_curr = hist_lpf[xidx];
		bin_righ = hist_lpf[xidx + 1];

		/*get the trend[32]*/
		if (bin_curr > adp_thrd[xidx]) {
			if (bin_curr < (bin_righ - reg_no_trend_margin))
				trend[xidx] = 2;  /*upward*/
			else if (bin_curr > (bin_righ + reg_no_trend_margin))
				trend[xidx] = 1;  /*downward*/
			else
				trend[xidx] = 0;  /*no trend*/
		} else
			trend[xidx] = -1;/*too small hist not valid trend*/
		if ((dnlp_printk >> 14) & 0x1)
			pr_dnlp_dbg("## step 4.0:(adp_thrd[%2d]) %4d, (bin_curr) %4d,bin_righ %4d => trend[%2d] %d\n",
				xidx, adp_thrd[xidx],
				bin_curr, bin_righ,
				xidx, trend[xidx]);
	}

	/*step 4.1 gen: xross_point/slope,*/
	xross_num  = -1;/*make sure it is -1 for C, and 0 for matlab*/
	lpfbin_1stpeak = maxbin_2ndabove = -1;/*initialization*/
	bb_protect = -3;
	slope_1st  = 256;

	for (xidx = 0; xidx < 32; xidx++) {
		/*get the bin_lpf*/
		bin_curr = hist_lpf[xidx];
		bin_righ = hist_lpf[xidx + 1];

		/* 4.1.1: get the cross-121curve point/slope*/
		if (xidx > 0 && xidx < 31 &&
			trend[xidx - 1] == 2 && trend[xidx + 1] == 2)
			trend[xidx] = 2;  /*avoid one node valley*/

		c = trend[xidx];
		m = (xidx < 1) ? trend[0] :
			(trend[xidx - 1] == 0) ? m : trend[xidx - 1];
		p = (xidx > 30) ? trend[31] :
			trend[xidx + 1];
		if (((xidx > 1 && ((c == 1 && m == 2) ||
			((c == 0 || c == -1) &&
			(m == -1) && p == 1))) ||
			((xidx == 1 || xidx == 2) &&
			(c == 1 || m == 1) && 0)) &&
			xross_num < 8) {
			/*4.1.1.1:set the index*/
			xross_num += 1;
			/*node index of cross 121 line*/
			xross_xidx[xross_num] = xidx;

		/*get the 2nd and above maximum bin*/
		if (xross_num == 0)
			lpfbin_1stpeak = bin_curr;
		else if (bin_curr > maxbin_2ndabove)
			maxbin_2ndabove = bin_curr;

		/*4.1.1.2:get the node slope*/
		/*get the bin_lpf8 for divider only for the peak node*/
		bin_lpf8 = 0;
		kk = (xidx << 1) + 1;/*bin index in histogram*/
		m3 = (kk < 3) ? 0 : (kk - 3);
		m4 = (kk < 4) ? 0 : (kk - 4);
		p2 = (kk > 61) ? 63 : (kk + 2);
		p3 = (kk > 60) ? 63 : (kk + 3);
		bin_lpf8 = ((((int) iHst[m3] + (int) iHst[m4] +
					  (int) iHst[p2] + (int) iHst[p3]) >> 2)
						+ bin_curr) >> 1;

		/*get the max bin of the [x x C x] four bins*/
		bin_max = 0; bin_max_idx = -3;
		for (k = -2; k < 2; k++) {
			kk = (xidx << 1) + 1 + k;  /*2x+1 + k*/
			kk = (kk < 0) ? 0 : (kk > 63) ? 63 : kk;
			if ((int)iHst[kk] > bin_max) {
				bin_max = (int)iHst[kk];
				bin_max_idx = k;
			}
		}

		/*get the blackbar protection*/
		bin_scl = (bin_curr << 1) +
			bin_curr + (bin_curr >> 2); /*3.25*bin*/
		if (xross_num == 1 && xidx < 4 && bin_max > bin_scl)
			bb_protect = bin_max_idx;  /*[-2,-1,0,1]*/

		/*bin_clip*/
		bin_scl = (bin_lpf8) >> 1;   /*0.5*/
		bin_clip = (bin_scl > hist_avg) ? hist_avg :
		(bin_scl < adp_thrd[xidx]) ? adp_thrd[xidx] : bin_scl;

		/*get the slope by the divider*/
		bin_scl = (bin_curr < bin_righ) ? bin_righ:bin_curr;
		/*256 normalized to 1.0 for the slope*/
		xross_slop[xross_num] = (bin_scl << 8) / (bin_clip + 1);

		/*4.1.1.3:get the cross node ofst*/
		/*get if local max bin localtes on which bin [-2,-1,0,1]*/
		bin_scl = (bin_clip << 1); /* 2x */
		if (xidx > 1 && xidx < 29 && bin_max > bin_scl)
			xross_ofst[xross_num] = bin_max_idx;
		else
			xross_ofst[xross_num] = -3;

		/*4.1.1.4 : detect the mono max bins and decrease the slop*/
		bin_scl = (hist_avg << 2);
		/*max(bin_curr, 4*hist_avg)*/
		bin_scl = (bin_curr > bin_scl) ? bin_curr : bin_scl;
		if (xidx >= reg_mono_binrang[0] &&
			xidx <= reg_mono_binrang[1]) {
			if (bin_max > ((bin_scl << 1) + (bin_scl)))
				lmt_slope = reg_mono_protect[0];/*3.5x*/
			else if (bin_max > ((bin_scl << 1) + (bin_scl >> 1)))
				lmt_slope = reg_mono_protect[1];/*2.5x*/
			else if (bin_max > ((bin_scl << 1)))
				lmt_slope = reg_mono_protect[2]; /*2.0x*/
			else if (bin_max > ((bin_scl << 1) - (bin_scl >> 2)))
				lmt_slope = reg_mono_protect[3]; /*1.75x*/
			else if (bin_max > ((bin_scl) + (bin_scl >> 1)))
				lmt_slope = reg_mono_protect[4]; /*1.5x*/
			else if (bin_max > ((bin_scl) + (bin_scl >> 2)))
				lmt_slope = reg_mono_protect[5]; /*1.25x*/
			else
				lmt_slope = 1024; /*4.0*/

				/* limit the slope for mono color(face part) */
			xross_slop[xross_num] =
			(xross_slop[xross_num] > lmt_slope) ?
			lmt_slope : xross_slop[xross_num];
		}

		}

		/*4.1.2 get the 1st node slope*/
		if (xidx == 0)
			/*256 normalized to 1.0*/
			slope_1st = (bin_curr << 8) / (adp_thrd[xidx] + 1);

		/*4.1.3 get the right most valid node*/
		if (bin_curr > adp_thrd[xidx])
			xidx_rightmost_valid_hist = xidx;
	}

	if (xross_num < 0)
		xross_num = 0;
	/*right most valid peak rightshift to */
	/* the right most valid peak	(patch)*/
	xidx_final_peak = xross_xidx[xross_num];
	if (xidx_final_peak >= 23 && xidx_final_peak < 32) {
		k = xidx_final_peak - 23;
		max_shift = reg_trend_wht_expand_lut8[k];
		t = 0;
	tt = xidx_final_peak + t;
		while (trend[tt] >= 0 && t <= max_shift && (tt) < 31) {
			trend[tt] = 2;
			xross_xidx[xross_num] = tt;
			t = t + 1;
			tt = xidx_final_peak + t;
		}
		if (t > 1)
			xross_ofst[xross_num] = -2;
	}

	/*step 4.2 right most peak ramp to max_range curve*/
	xross_num += 1;
	if (xross_num > 8)
		xross_num = 8;
	xross_xidx[xross_num] =
		(xidx_rightmost_valid_hist >= 22) ? 32 :
		(xidx_rightmost_valid_hist <= 13) ? 23 :
		(xidx_rightmost_valid_hist+10);
	xross_slop[xross_num] = 128;
		  /*0.5*/
	if ((dnlp_printk >> 4) & 0x1) {
		pr_dnlp_dbg("];\nxross_xidx =[");
		for (k = 0; k < 9; k++)
			pr_dnlp_dbg("%2d ", xross_xidx[k]);
		pr_dnlp_dbg("];\nxross_slop =[");
		for (k = 0; k < 9; k++)
			pr_dnlp_dbg("%2d ", xross_slop[k]);
		pr_dnlp_dbg("];\nxross_ofst(org) =[");
		for (k = 0; k < 9; k++)
			pr_dnlp_dbg("%2d ", xross_ofst[k]);
		pr_dnlp_dbg("];\n xross_num = %3d\n ", xross_num);
		pr_dnlp_dbg("lpfbin_1stpeak = %3d\n ", lpfbin_1stpeak);
		pr_dnlp_dbg("maxbin_2ndabove = %3d\n ", maxbin_2ndabove);
		pr_dnlp_dbg("bb_protect = %3d\n ", bb_protect);
		pr_dnlp_dbg("xidx_rightmost_valid_hist = %3d\n ",
			xidx_rightmost_valid_hist);
	}

	/*step 4.3 generate the 32-nodes curve base on the xcross nodes*/
	slope_step_norm = 32;  /*1024/32nodes*/
	for (xnum = 0; xnum <= xross_num; xnum++) {
		xidx = xross_xidx[xnum];
		if (xnum == 0)
			xidx_prev = 0;/*first peak*/
		else
			xidx_prev = xross_xidx[xnum - 1];

		/*4.3.1 black/white expansion of each peak*/
		if (xnum < xross_num) {
			/*black expansion left side (modify the trend[xidx])*/
			if (reg_trend_blk_expand_mode >= 1 &&
				xidx > 2 && xidx < 29 && trend[xidx - 1] <= 0)
				trend[xidx - 1] = 2;
			if (reg_trend_blk_expand_mode >= 2 &&
				xidx > 2 && xidx < 29 && trend[xidx - 2] <= 0)
				trend[xidx - 2] = 2;
			/*white expansion right side */
			if (reg_trend_wht_expand_mode >= 1 &&
				xidx >= 0 && xidx < 30 && trend[xidx + 1] <= 0)
				trend[xidx + 1] = 1;
			if (reg_trend_wht_expand_mode >= 2 &&
				xidx >= 0 && xidx < 29 && trend[xidx + 2] <= 0)
				trend[xidx + 2] = 1;
		}
		if ((dnlp_printk >> 14) & 0x1)
			pr_dnlp_dbg("\n\n## step 4.3.1 BWE : xidx = %2d, trend[%2d-1]= %4d,trend[%2d-2]= %4d\n",
				xidx, xidx, trend[xidx - 1],
				xidx, trend[xidx - 2]);
		/*4.3.2 get the slope of each range*/
		slope_c = xross_slop[xnum];
		if (xnum == 0)
			slope_c =
				(slope_c < reg_min_slop_1st) ?
				reg_min_slop_1st :
				(slope_c > reg_max_slop_1st) ?
				reg_max_slop_1st :
				slope_c;
		else if (xnum >= (xross_num - 1))
			slope_c =
				(slope_c < reg_min_slop_fin) ?
				reg_min_slop_fin :
				(slope_c > reg_max_slop_fin) ?
				reg_max_slop_fin :
				slope_c;
		else
			slope_c =
				(slope_c < reg_min_slop_mid) ?
				reg_min_slop_mid :
				(slope_c > reg_max_slop_mid) ?
				reg_max_slop_mid :
				slope_c;

		if (xnum == 0)
			slope_p =
				(slope_1st < reg_min_slop_1st) ?
				reg_min_slop_1st :
				(slope_1st > reg_max_slop_1st) ?
				reg_max_slop_1st :
				slope_1st;
		else {
			slope_p = xross_slop[xnum - 1];
			slope_p =
			(slope_p < reg_min_slop_mid) ?
			reg_min_slop_mid :
			(slope_p > reg_max_slop_mid) ?
			reg_max_slop_mid :
			slope_p;
		}
		if ((dnlp_printk >> 14) & 0x1)
			pr_dnlp_dbg("## step 4.3.2,get the slope: xidx = %2d, slope_c= %4d\n",
				xidx, slope_c);
		/*4.3.3 get the slop_steps*/
		slope_step_c = (slope_c >> 3);   /*(32*slope_c)/256*/
		slope_step_p = (slope_p >> 3);   /*(32*slope_p)/256*/
		delta_slop_step_c = slope_step_c - slope_step_norm;
		delta_slop_step_p = slope_step_p - slope_step_norm;
		if ((dnlp_printk >> 14) & 0x1)
			pr_dnlp_dbg("## step 4.3.2,get the slope: xnum = %2d, slope_p= %4d\n",
				xnum, slope_p);
		/*4.4.4 estimate the curves between nodes*/
		if (xidx > 0) {
			/*4.4.4.1 initialize the output curve */
	/*	ws consideration of black level boost up*/
			if (xnum == 0 && xidx <= 11) {
				boost_ofst = 0;
				m = (xidx < 1) ? 0 : (xidx - 1);
				p = (xidx > 10) ? 11 : (xidx + 1);
				c = xidx;

				if (xross_ofst[xnum] == -2)
					/*local max is on (xidx-2)*/
					boost_ofst = reg_blk_boost_12[m];
				else if (xross_ofst[xnum] == -1)
					/*local max is on (xidx-1)*/
					boost_ofst = (reg_blk_boost_12[m] +
						reg_blk_boost_12[c]) >> 1;
				else if (xross_ofst[xnum] == 0)
					/*local max is on (xidx-0)*/
					boost_ofst = reg_blk_boost_12[c];
				else if (xross_ofst[xnum] ==  1)
					/*local max is on (xidx+1)*/
					boost_ofst = (reg_blk_boost_12[c] +
						reg_blk_boost_12[p]) >> 1;
				else
					boost_ofst = reg_blk_boost_12[c];


				if (lpfbin_1stpeak > (maxbin_2ndabove << 1))
					boost_ofst = boost_ofst;/*1.0*/
				else if (lpfbin_1stpeak > (maxbin_2ndabove +
						(maxbin_2ndabove >> 1)))
					boost_ofst = boost_ofst -
						(boost_ofst >> 2); /*0.75*/
				else if (lpfbin_1stpeak > (maxbin_2ndabove))
					boost_ofst = (boost_ofst >> 1); /*0.5*/
				else if (lpfbin_1stpeak > (maxbin_2ndabove -
						(maxbin_2ndabove >> 2)))
					boost_ofst =
						(boost_ofst >> 2); /*0.25*/
				else
					boost_ofst =
						(boost_ofst >> 3); /*0.125*/

				/*black bar protection to */
				/* overwrite the boost_ofst*/
				if (bb_protect == -2)
					/*local max is on (xidx-2)*/
					boost_ofst =  12;
				else if (bb_protect == -1)
					/*local max is on (xidx-1)*/
					boost_ofst = 8;
				else if (bb_protect == 0)
					/*local max is on (xidx-0)*/
					boost_ofst = 0;
				else if (bb_protect == 1)
					/*local max is on (xidx+1)*/
					boost_ofst =  -8;
				else if (bb_protect == 2)
					/*local max is on (xidx+2)*/
					boost_ofst = -12;
				else
					boost_ofst = boost_ofst;
				/* initialize the curv_hist_node */
				curv_hist_node[xidx] =
					curv_121n_node[xidx] + boost_ofst;
				if ((dnlp_printk >> 14) & 0x1)
					pr_dnlp_dbg("**4.4.4.1((xnum==0 && xidx<=11)):xidx-%2d,boost_ofst-%2d,=> curv_hist_node[%2d]= %4d\n ",
					xidx, boost_ofst, xidx,
					curv_hist_node[xidx]);
			} else {
				if (xross_ofst[xnum] == -2)
					/*local max is on (xidx-2)*/
					boost_ofst = 0;
				else if (xross_ofst[xnum] == -1)
					/*local max is on (xidx-1)*/
					boost_ofst = 0;
				else if (xross_ofst[xnum] == 0)
					/*local max is on (xidx-0)*/
					boost_ofst = 0;
				else if (xross_ofst[xnum] ==  1)
					/*local max is on (xidx+1)*/
					boost_ofst = 0;
				else if (xross_ofst[xnum] == -2)
					/*local max is on (xidx+2)*/
					boost_ofst = 0;
				else
					boost_ofst = 0;
				/*initialize the curv_hist_node*/
				curv_hist_node[xidx] =  curv_121n_node[xidx] +
					boost_ofst;
				if ((dnlp_printk >> 14) & 0x1)
					pr_dnlp_dbg("**4.4.4.1( ):xidx-%2d,boost_ofst-%2d,=> curv_hist_node[%2d]= %4d\n ",
					xidx, boost_ofst,
					xidx, curv_hist_node[xidx]);
			}

			/*4.4.4.2 analyse the trend*/
			up_idx = xidx;
			dn_idx = xidx_prev;
			for (k = (xidx - 1); k >= xidx_prev; k--) {
				if (trend[k] == 2 && k > 1)
					up_idx = k; /*up-trend*/
				if (trend[k] == 2 ||
					((trend[k] == -1 || trend[k] == 0)
					&& (k > xidx_prev)))
					dn_idx = k;  /*down trend ends*/
			}
			/*num of bins of up-trend*/
			num_2 = xidx - up_idx;
			/*num of bins of down-trend*/
			num_1 = dn_idx - xidx_prev;
			/*num of bins of too small hist*/
			num_x = up_idx - dn_idx;
			if (num_x > 0)
				num_x += 1;
			if ((dnlp_printk >> 14) & 0x1)
				pr_dnlp_dbg("\n**4.4.4.2:xidx=%2d,up_idx=%2d,xidx_prev=%2d,dn_idx=%2d,num_2_1_x[%d(%d-%d) %d(%d-%d) %d(%d-%d)]\n",
					xidx, up_idx, xidx_prev,
					dn_idx, num_2, xidx, up_idx,
					num_1, dn_idx, xidx_prev,
					num_x, up_idx, dn_idx);
			/*4.4.4.3 get the 1st round of */
			/* estimation of the curves*/
			if (num_2 <= 0)
				sum_step_2 = (delta_slop_step_c >> 1);
			else
				/*(1.5-(2^(-num_2)))*delta_slop_step; */
				/* % 0.5 + .5 + .25 +.125.*/
			sum_step_2 = (num_2 << 5) +
					delta_slop_step_c +
					(delta_slop_step_c>>1) -
					(delta_slop_step_c>>num_2);

			if (num_1 == 0)
				sum_step_1 = 0;
				/*no up-trend most likely for left most*/
			else
			/*(2- (2^(-num_1)))*delta_slop_step_prev; */
			/*	1+.5+.25+.125+...*/
			sum_step_1 = (num_1 << 5) +
				(delta_slop_step_p << 1) -
				(delta_slop_step_p >> num_1);

		sum_step_x = curv_hist_node[xidx] -
				curv_hist_node[xidx_prev] -
				sum_step_2 - sum_step_1;
			if ((dnlp_printk >> 14) & 0x1)
				pr_dnlp_dbg(" **4.4.4.3:num_2=%2d,num_1=%2d,sum_step_x=%2d(%4d-%4d-%d-%d)\n",
					num_2, num_1, sum_step_x,
					curv_hist_node[xidx],
					curv_hist_node[xidx_prev],
					sum_step_2, sum_step_1);

		/*4.4.4.4 adaptive offset for nearby peak protection */
		/*	(2nd peak curve start from 1'st results)*/
			if (xnum < 3 && sum_step_x < 0 && xidx < 20) {
				ofset = (sum_step_x *
					reg_adp_ofset_20[xidx]) >> 5;
				curv_hist_node[xidx] -= ofset;
				sum_step_x -= ofset;
			}

		/*4.4.4.5 if x range slop too low,*/
		 /* *do the fallback on up/dn range*/
			sum_step_x_min = (num_x * reg_min_slop_mid) >> 3;
			sub_x = sum_step_x_min - sum_step_x;
			if (sub_x > 0) {
				/*scale down the up-trend and dn-trend range*/
				if (num_2 == 0 || sum_step_2 == 0)
					dlt_2 = 0;
				else
					dlt_2 = sub_x * sum_step_2 / (sum_step_2
						+ sum_step_1);

				dlt_1 = sub_x - dlt_2;

			/*ratio to gain back the slope*/
			if (sum_step_2 == 0)
				rat_2 = 256;
			else
				rat_2 = ((sum_step_2 -
					dlt_2) << 8) / sum_step_2;
			if (sum_step_1 == 0)
				rat_1 = 256;
			else
				rat_1 = ((sum_step_1 -
					dlt_1) << 8) / sum_step_1;
			if ((dnlp_printk >> 14) & 0x1) {
				pr_dnlp_dbg("\n$$ 4.4.4.5:sub_x = sum_step_x_min-sum_step_x,sum_step_2(1),dlt_2(1),rat_2(1) ");
				pr_dnlp_dbg("\n$$ 4.4.4.5:%d= %d  -  %d,%d(%d),%d(%d), %d(%d)  ",
					sub_x, sum_step_x_min,
					sum_step_x, sum_step_2,
					sum_step_1, dlt_2, dlt_1,
					rat_2, rat_1);
			}
			} else {
				 rat_2 = 256;
				 rat_1 = 256; /*normalize 256 to 1.0*/
			}

			/*4.4.4.6 calculate the up-trend curve*/
			t = 0;
		accum = curv_hist_node[xidx];
			for (k = (xidx - 1); k >= up_idx; k--) {
				kk = (t < 1) ? 1 : t;
				if (rat_2 == 256)
					accum -= (slope_step_norm +
						(delta_slop_step_c >> kk));
				else
					accum -= (((slope_step_norm +
					(delta_slop_step_c>>kk)) * rat_2) >> 8);

				/*get the node curve*/
				curv_hist_node[k] = accum;
				t = t + 1;
				if ((dnlp_printk >> 14) & 0x1)
					pr_dnlp_dbg("==4.4.4.6 up-trend curve:k-%d,kk-%d,rat_2-%d, slope_step_norm-%d,delta_slop_step_c-%d => curv_hist_node[%d]=%4d\n",
						k, kk, rat_2, slope_step_norm,
						delta_slop_step_c,
						k, accum);
			}

			/*4.4.4.7 calculate the dn-trend curve*/
			t = 0;
			accum = curv_hist_node[xidx_prev];
			if (xidx_prev < (dn_idx - 1)) {
				for (k = xidx_prev; k <= (dn_idx - 1); k++) {
					curv_hist_node[k] = accum;
				if (rat_1 == 256)
					accum = accum + (slope_step_norm +
					(delta_slop_step_p >> t));
				else
					accum = accum + (((slope_step_norm +
					(delta_slop_step_p>>t)) * rat_1) >> 8);

				t = t + 1;
				/*for not num_x cases, the dn_idx== up_idx*/
				if (num_x == 0 && k == (dn_idx - 1))
					curv_hist_node[dn_idx] =
					(curv_hist_node[dn_idx] +
					 accum + 1) >> 1;
				if ((dnlp_printk >> 14) & 0x1)
					pr_dnlp_dbg("==4.4.4.7-1:  dn-trend curve:k-%d,t-%d,rat_1-%d, slope_step_norm-%d,delta_slop_step_p-%d => accum[%d]=%4d(%4d)\n",
						k, t, rat_1, slope_step_norm,
						delta_slop_step_p, k,
						accum, curv_hist_node[dn_idx]);
			}
		} else
		curv_hist_node[xidx_prev] = accum;
			if ((dnlp_printk >> 14) & 0x1)
				pr_dnlp_dbg("==4.4.4.7-2:dn-trend curve: xidx_prev= %d,curv_hist_node[%d]= %d  ",
					xidx_prev, xidx_prev,
					curv_hist_node[xidx_prev]);

		/*4.4.4.8 get the in-between x range curve*/
		kk = (dn_idx < 1) ? 0 : (dn_idx - 1);
		up_dn_dist = curv_hist_node[up_idx] - curv_hist_node[kk];
		if (num_x > 0) {
			accum = curv_hist_node[kk];
			if (dn_idx <= 0) {
				st_idx = dn_idx + 1;
				/*max(num_x -1,1);*/
				num_xx = (num_x < 2) ? 1 : (num_x - 1);
			} else{
				st_idx = dn_idx;
				num_xx = num_x;
			}

			for (k = st_idx; k <= (up_idx - 1); k++) {
				accum = accum + (up_dn_dist / num_xx);
				curv_hist_node[k] = accum;
			}
			if ((dnlp_printk >> 14) & 0x1)
				pr_dnlp_dbg("==4.4.4.8:k-%d,up_dn_dist-%d,num_xx-%d =>curv_hist_node[%d]=%d\n",
					k, up_dn_dist, num_xx, k, accum);
			}
	} /*for xidx*/
	} /*for xnum*/

	/*1st two nodes blending (optional)*/
	curv_hist_node[0] = curv_121n_node[0] +
	((curv_hist_node[1] - curv_121n_node[1] + 1) >> 1);

	/*do interpolation from 32 nodes to 64 bins.*/
	for (k = 0; k < 65; k++) {
		if (k == 0)
			oMap[k] = 0;
		else if (k == 64)
			oMap[k] = 1023;
		else if ((k%2 == 1))
			oMap[k] = (unsigned int) curv_hist_node[(k >> 1)];
		else
			oMap[k] = (unsigned int)
				((curv_hist_node[(k >> 1) - 1] +
				curv_hist_node[(k >> 1)] + 1) >> 1);

	nDif = oMap[k] - (k << 4);
	nTmp = (k << 4) + ((nDif * ve_hist_cur_gain +
		(ve_hist_cur_gain_precise >> 1)) / ve_hist_cur_gain_precise);

	if ((dnlp_printk >> 4) & 0x1)
		pr_dnlp_dbg("# add_gain:[%d]:oMap[k]_%d,line_%d, Dif_%d => final_%d (gain %d)\n ",
		k, oMap[k], k << 4, nDif, nTmp, ve_hist_cur_gain);

		oMap[k] = nTmp;
	}

	if ((dnlp_printk >> 4) & 0x1) {
		pr_dnlp_dbg("\n=====hist_lpf: [\n");
		for (j = 0; j < 2; j++) {
			i = j * 16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
					hist_lpf[i], hist_lpf[i+1],
					hist_lpf[i+2], hist_lpf[i+3],
					hist_lpf[i+4], hist_lpf[i+5],
					hist_lpf[i+6], hist_lpf[i+7],
					hist_lpf[i+8], hist_lpf[i+9],
					hist_lpf[i+10], hist_lpf[i+11],
					hist_lpf[i+12], hist_lpf[i+13],
					hist_lpf[i+14], hist_lpf[i+15]);
		}
		pr_dnlp_dbg("]\n");

		pr_dnlp_dbg("\n=====trend_32: [\n");
		for (j = 0; j < 2; j++) {
			i = j * 16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
					trend[i], trend[i+1],
					trend[i+2], trend[i+3],
					trend[i+4], trend[i+5],
					trend[i+6], trend[i+7],
					trend[i+8], trend[i+9],
					trend[i+10], trend[i+11],
					trend[i+12], trend[i+13],
					trend[i+14], trend[i+15]);
		}
		pr_dnlp_dbg("]\n");

		pr_dnlp_dbg("\n=====curv_nod: [\n");
		for (j = 0; j < 2; j++) {
			i = j * 16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
					curv_hist_node[i],
					curv_hist_node[i+1],
					curv_hist_node[i+2],
					curv_hist_node[i+3],
					curv_hist_node[i+4],
					curv_hist_node[i+5],
					curv_hist_node[i+6],
					curv_hist_node[i+7],
					curv_hist_node[i+8],
					curv_hist_node[i+9],
					curv_hist_node[i+10],
					curv_hist_node[i+11],
					curv_hist_node[i+12],
					curv_hist_node[i+13],
					curv_hist_node[i+14],
					curv_hist_node[i+15]);
		}
		pr_dnlp_dbg("]\n");
	}

	k = 0;
}

/*iHst[0:63]: [0,4)->iHst[0], [252,256)->iHst[63]*/
/*oMap[0:64]:0:16:1024*/
void get_clahe_curve(unsigned int *oMap, unsigned int *olAvg4,
		unsigned int *iHst, int var, unsigned int hstBgn,
		unsigned int hstEnd)
{
	int i = 0, j = 0, tmp, tmp1, tmp2;
	unsigned int tmax = 0;
	unsigned int tsum = 0, sum_clip = 0;
	unsigned int oHst[64], Hst_clip[64];
	unsigned int cLmt = 0;
	unsigned int tLen = (hstEnd - hstBgn);
	unsigned int tAvg = 0;
	unsigned int lAvg4 = 0;
	unsigned int lAvg1 = 0;
	unsigned int tbin, tbin_clip = 0, norm14;
	int sumshft, dlt_acc[64];
	/*unsigned int uLmt = 0;*/
	/*unsigned int stp = 0;*/
	unsigned int tHst[64];
	unsigned int clip_rate  = dnlp_alg_param.dnlp_cliprate_v3;
	unsigned int clip_rmin  = dnlp_alg_param.dnlp_cliprate_min;
	unsigned int adp_crate = clip_rate;/* u8 */
	int clahe_gain_neg;
	int clahe_gain_pos;
	unsigned short *glb_clash_curve;

	if (clip_rmin > clip_rate)
		clip_rmin = clip_rate;
	if (dnlp_alg_param.dnlp_adpcrat_lbnd < 2)
		dnlp_alg_param.dnlp_adpcrat_lbnd = 2;
	else if (dnlp_alg_param.dnlp_adpcrat_lbnd > 30)
		dnlp_alg_param.dnlp_adpcrat_lbnd = 30;

	if (dnlp_alg_param.dnlp_adpcrat_hbnd < 2)
		dnlp_alg_param.dnlp_adpcrat_hbnd = 2;
	else if (dnlp_alg_param.dnlp_adpcrat_hbnd > 30)
		dnlp_alg_param.dnlp_adpcrat_hbnd = 30;

	if (hstBgn > 16)
		hstBgn = 16;

	if (hstEnd > 64)
		hstEnd = 64;
	else if (hstEnd < 48)
		hstEnd = 48;

	if ((dnlp_printk >> 4) & 0x1)
		pr_dnlp_dbg("step B-2.x( get_clahe_curve): hstBgn = %d,hstEnd = %d\n",
		hstBgn, hstEnd);

	oMap[64] = 1024; /* 0~1024 */
	/*4.1: loop hist 64 bins to get: max, avg, sum of hist*/
	lAvg4 = 0;
	lAvg1 = 0;
	for (i = 0; i < 64; i++) {
		oMap[i] = (i << 4); /* 0~1024, 45 degree initialization */

		/* limited range [4,59] */
		if (i >= hstBgn && i <= hstEnd) {
			/* boundary bin use in-bound bin if it is */
			/* larger to avoid BB/WB peaks */
			tbin = (i == hstBgn && iHst[i] > iHst[i+1]) ?
				iHst[i+1] :
				(i == (hstEnd-1) && iHst[i] > iHst[i-1]) ?
				iHst[i-1] : iHst[i];
			if (tmax < tbin)
				/* max hist num within range (but not use) */
				tmax = tbin;
			/* num of pixels within range */
			tsum += tbin;
			/* lum sum of pixels within range */
			lAvg4 += (tbin * i);
		} else
			tbin = 0;

		oHst[i] = tbin;
		tHst[i] = tbin;/* for clipping */
	}
	if (hstEnd <= hstBgn)
		return;/* set to oMap as 45 degree map */

	lAvg4 = (lAvg4 << 2) + tsum / 2;
	lAvg4 = lAvg4 / (tsum + 1);/*luminance avgerage (0~255)*/
	lAvg1 = (lAvg4 + 2) >> 2;/* bin num of average (0~63)*/

	if ((dnlp_printk >> 4) & 0x1)/* echo 4 */
		pr_dnlp_dbg("#Step B-2.x:within range luma_avg4 = %d, max_hst(tmax) =%d\n",
			lAvg4, tmax);

	/*4.2 get the adptive clip rate,  << 4 */
	/* <<4, norm to 4096 as "1" */
	adp_crate = dnlp_adp_cliprate(clip_rate, clip_rmin, lAvg1);

	/* clip bin num for each bin	;max pixel num in each bin */
	cLmt = (adp_crate * tsum + 2048) >> 12;

	/*4.3 clip the histo to Hst_clip */
	sum_clip = 0;
	for (i = 0; i < 64; i++) {
		if (i >= hstBgn && i <= hstEnd) {
			if (tHst[i] > cLmt) {
				Hst_clip[i] =  cLmt;
				sum_clip += (tHst[i] - cLmt);
			} else
				Hst_clip[i] = tHst[i];
			} else
				Hst_clip[i] = 0;
	}
	/*
	 *hstBgn = 0;
	 *hstEnd = 64;
	 *tLen = 64;
	 */
	tLen = hstEnd - hstBgn + 1;
	/* clipped portion distribute to each valid bin amount */
	tAvg = (sum_clip + tLen / 2) / tLen;

    /*4.4 get the hist accum to ge delta curve*/
    /* 4.4.1 normalization gain for the histo, normlized to u14 */
	sumshft = (tsum >= (1 << 24)) ? 8 : (tsum >= (1 << 22)) ?
		6 : (tsum >= (1 << 20)) ? 4 : (tsum >= (1 << 18)) ? 2 :
		(tsum >= (1<<16)) ? 0 : (tsum >= (1 << 14)) ?
		-2 : (tsum >= (1 << 12)) ? -4 : (tsum >= (1 << 10)) ?
		-6 : (tsum >= (1 << 8)) ? -8 : (tsum >= (1 << 6)) ?
		-10 : (tsum >= (1 << 4)) ? -12 : -16;
	if (sumshft >= 0)
		norm14 = (1 << 30) / (tsum >> sumshft);
	else if (sumshft > -16)
		norm14 = (1 << (30 + sumshft)) / tsum;
	else {
		norm14 = 0;
		return;
	}

	if ((dnlp_printk >> 4) & 0x1)
		pr_dnlp_dbg("# stepB-(get_clahe)CL:\n Range[hstBgn %02d ~ hstEnd %02d] lAvg4=%d(%d),\n"
			"(crate /4096)=%d, tsum=%5d, cLmt=%3d, tAvg=%4d, sumshft=%d, norm14=%d\n",
			hstBgn, hstEnd, lAvg4, lAvg1, adp_crate,
			tsum, cLmt, tAvg,  sumshft, norm14);
		/* hstBgn, hstEnd must[0,64] */

	/* hstBgn=0; */
   /* hstEnd=64; */
	sum_clip = 0;
	tmp1 = 0;
	clahe_gain_neg = 0;
	clahe_gain_pos = 0;
	for (i = 0; i < 64; i++) {
		/* 4.4.3 calculate the accu and get the delta */
		if (i >= hstBgn && i <= hstEnd) {

			tbin_clip = (Hst_clip[i] + tAvg);
			/*4.4.2 normalization bins to 2^16 */
			if (sumshft >= 0)
				tbin_clip = ((tbin_clip >> sumshft) * norm14 +
					(1 << 13)) >> 14;
			else/* if (sumshft<0) */
				tbin_clip =
					((tbin_clip << (-sumshft)) * norm14 +
						(1 << 13)) >> 14;

			/* acc */
			sum_clip += tbin_clip; /* 2^16 as "1" ---cdf */
			j = (i-hstBgn+1);
			tmp1 = (j<<10);
			/* signed dif to 45 degree line,*/
			/*	normalized to u16 precision */
			dlt_acc[i] = ((int)sum_clip - tmp1);
		} else
			dlt_acc[i] =  0;

		tmp2 = dlt_acc[i];
		/* 4.4.3 gain to delta */
		/* if hist var is small,use clahe curve more */
		if (var < dnlp_alg_param.dnlp_var_th) {
			clahe_gain_neg =
				(int)dnlp_alg_param.dnlp_clahe_gain_neg +
				dnlp_alg_param.dnlp_clahe_gain_delta;
			clahe_gain_pos =
				(int)dnlp_alg_param.dnlp_clahe_gain_pos +
				dnlp_alg_param.dnlp_clahe_gain_delta;
		} else {
			clahe_gain_neg = dnlp_alg_param.dnlp_clahe_gain_neg;
			clahe_gain_pos = dnlp_alg_param.dnlp_clahe_gain_pos;
		}

		if (dlt_acc[i] < 0)
			/* apply the gain of CLAHE, */
			dlt_acc[i] = (dlt_acc[i] * (int)clahe_gain_neg + 128)
				/ 256;
		else
			dlt_acc[i] = (dlt_acc[i] * (int)clahe_gain_pos + 128)
				/ 256;

		/* get the omap and clip to range */
		tmp = (i << 10) + (dlt_acc[i]); /* normalized to u16 */
		tmp = (tmp < 0) ? 0 : tmp;
		tmp = (tmp + 32) >> 6;
		oMap[i] = (tmp > 1023) ? 1023 : tmp;

		if ((dnlp_printk >> 4) & 0x1)/* echo 4 */
			pr_dnlp_dbg("StepB-2.x:CLASHE:\n[%02d:iHst=%5d,Hclip=%5d,bin_clip=%5d]:[sum_clp=%5d,j=%2d,tmp1=%6d,tmp2=%6d]%4d(45lin)+%5d(dlt_acc)=>%4d(C)],\n",
				i, iHst[i], Hst_clip[i],
				tbin_clip, sum_clip, j,
				tmp1, tmp2, i<<4,
				dlt_acc[i]/64, oMap[i]);
	}

    /* 4.5 output the *olAvg4 */
	*olAvg4 = lAvg4;

	if (debug_add_curve_en)	{
		glb_clash_curve = kzalloc(65*sizeof(unsigned short),
			GFP_KERNEL);
		if (glb_clash_curve == NULL)
			return;
		for (i = 0; i < 65; i++) {
			oMap[i] = ((128 - glb_clash_curve_bld_rate) * oMap[i] +
				glb_clash_curve_bld_rate *
				(glb_clash_curve[i]<<2) +
				64) >> 7;
		}
		kfree(glb_clash_curve);
	}

	if ((dnlp_printk >> 4) & 0x1)
		pr_dnlp_dbg("\n@@@get_clahe_curve(): clahe_gain_neg = %d,clahe_gain_pos = %d\n ",
		clahe_gain_neg, clahe_gain_pos);

	if ((dnlp_printk >> 14) & 0x1) {
		/* iHst */

		pr_dnlp_dbg("\n=====pre_0_gma: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
					iHst[i], iHst[i+1],
					iHst[i+2], iHst[i+3],
					iHst[i+4], iHst[i+5],
					iHst[i+6], iHst[i+7],
					iHst[i+8], iHst[i+9],
					iHst[i+10], iHst[i+11],
					iHst[i+12], iHst[i+13],
					iHst[i+14], iHst[i+15]);
		}
		pr_dnlp_dbg("]\n");

		/* clip_Hst */
		pr_dnlp_dbg("\n=====clipped pre_0_gma: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
				Hst_clip[i] + tAvg, Hst_clip[i+1] + tAvg,
				Hst_clip[i+2] + tAvg, Hst_clip[i+3] + tAvg,
				Hst_clip[i+4] + tAvg, Hst_clip[i+5] + tAvg,
				Hst_clip[i+6] + tAvg, Hst_clip[i+7] + tAvg,
				Hst_clip[i+8] + tAvg, Hst_clip[i+9] + tAvg,
				Hst_clip[i+10] + tAvg, Hst_clip[i+11] + tAvg,
				Hst_clip[i+12] + tAvg, Hst_clip[i+13] + tAvg,
				Hst_clip[i+14] + tAvg, Hst_clip[i+15] + tAvg);
		}
		pr_dnlp_dbg("]\n");

		/* clash_curve */
		pr_dnlp_dbg("\n=====clash_curve: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
				oMap[i], oMap[i+1], oMap[i+2], oMap[i+3],
				oMap[i+4], oMap[i+5], oMap[i+6], oMap[i+7],
				oMap[i+8], oMap[i+9], oMap[i+10], oMap[i+11],
				oMap[i+12], oMap[i+13], oMap[i+14], oMap[i+15]);
		}
		pr_dnlp_dbg("]\n");
	}
}


/*functon: brightness calculated baed on luma_avg and low_lavg4,
 *IIR filtered;for black white extension
 *in: luma_avg4,low_lavg4
 *(return)out: dnlp_brightness
 */
/* TBC: how it works? */
static int cal_brght_plus(int luma_avg4, int low_lavg4)
{
	int avg_dif = 0;
	int dif_rat = 0;
	int low_rng = 0;
	int low_rat = 0;

	int dnlp_brightness = 0;
	static int pbrtness;

	if (luma_avg4 > low_lavg4)
		avg_dif = luma_avg4 - low_lavg4;

	if (avg_dif < dnlp_alg_param.dnlp_blk_cctr)
		dif_rat = dnlp_alg_param.dnlp_blk_cctr - avg_dif;

	if (luma_avg4 > dnlp_alg_param.dnlp_brgt_ctrl)
		low_rng = luma_avg4 - dnlp_alg_param.dnlp_brgt_ctrl;
	else
		low_rng = dnlp_alg_param.dnlp_brgt_ctrl - luma_avg4;

	if (low_rng < dnlp_alg_param.dnlp_brgt_range)
		low_rat = dnlp_alg_param.dnlp_brgt_range - low_rng;

	/* <<2 */
	dnlp_brightness =
		(dnlp_alg_param.dnlp_brght_max*dif_rat*low_rat + 16) >> 5;
	/* add=32 => add 0 */
	dnlp_brightness += ((dnlp_alg_param.dnlp_brght_add - 32) << 2);

	if (!dnlp_scn_chg) {
		dnlp_brightness = dnlp_bld_lvl * dnlp_brightness + (RBASE >> 1);
		dnlp_brightness = dnlp_brightness +
				(RBASE - dnlp_bld_lvl) * pbrtness;
		dnlp_brightness =
			(dnlp_brightness >> dnlp_alg_param.dnlp_mvreflsh);
	}
	pbrtness = dnlp_brightness;


	return dnlp_brightness; /* 0 ~ 1024 */
}


static void clahe_tiir(void)
{
	int i = 0;
	int nTmp0 = 0;
	static unsigned int pgmma[65] = {
		  0,   16,   32,  48,  64,  80,  96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  };

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pgmma[i] = (i << 4); /* 0 ~1024 */
	}

	if (!dnlp_scn_chg && ((dnlp_alg_param.dnlp_dbg_i2r >> 3) & 0x1))
		for (i = 0; i < 65; i++) {
			nTmp0 = dnlp_bld_lvl * clash_curve[i] + (RBASE >> 1);
			nTmp0 = nTmp0 + (RBASE - dnlp_bld_lvl) * pgmma[i];
			nTmp0 = (nTmp0 >> dnlp_alg_param.dnlp_mvreflsh);
		if ((dnlp_printk >> 4) & 0x1)
			pr_dnlp_dbg("\n(C_iir):[%d]: clash_curve[i]:%d,pgmma[i]:%d => nTmp0:%d(%d/%d)\n",
				i, clash_curve[i], pgmma[i],
				nTmp0, dnlp_bld_lvl, RBASE);

		clash_curve[i] = nTmp0;
		}


	for (i = 0; i < 65; i++)
		pgmma[i] = clash_curve[i];

}

/*function:iir filter coef cal base on the luma_avg change in time domain */
/*	   in : hstSum, rbase */
/*	   out: */
/*	return: bld_lvl (tiir coefs)  */
int curve_rfrsh_chk(int hstSum, int rbase)
{
	static unsigned int tLumAvg[30];
	static unsigned int tAvgDif[30];
	int lSby = 0;
	int bld_lvl = 0;
	int i = 0;

	for (i = 0; i < 29; i++) {
		tLumAvg[i] = tLumAvg[i+1];
		tAvgDif[i] = tAvgDif[i+1];
	}

	tLumAvg[29] = (ve_dnlp_luma_sum + (hstSum >> 1)) / hstSum;
	tLumAvg[29] = ((tLumAvg[29] + 4) >> 3);
	tAvgDif[29] = (tLumAvg[29] > tLumAvg[28]) ?
		(tLumAvg[29] - tLumAvg[28]) : (tLumAvg[28] - tLumAvg[29]);

	/* prt_flg = ((dnlp_printk >> 7) & 0x1); */

	lSby = 0;
	for (i = 0; i < 8; i++)
		lSby = lSby + tAvgDif[28 - i];
	lSby = ((lSby + 4) >> 3);

	if (tAvgDif[29] > tAvgDif[28])
		bld_lvl = tAvgDif[29] - tAvgDif[28];
	else
		bld_lvl = tAvgDif[28] - tAvgDif[29];

	bld_lvl = (bld_lvl << dnlp_alg_param.dnlp_schg_sft);

	if (dnlp_printk & 0x1)
		pr_dnlp_dbg("step 0.2.0: bld_lvl=%02d\n", bld_lvl);

	/* play station: return with black scene intersection */
	if (tAvgDif[29] > bld_lvl)
		bld_lvl = tAvgDif[29];

	if (bld_lvl > rbase)
		bld_lvl = rbase;
	else if (bld_lvl < dnlp_alg_param.dnlp_cuvbld_min)
		bld_lvl = dnlp_alg_param.dnlp_cuvbld_min;
	else if (bld_lvl > dnlp_alg_param.dnlp_cuvbld_max)
		bld_lvl = dnlp_alg_param.dnlp_cuvbld_max;

	/* print the logs */
	if (dnlp_printk & 0x1) {
		pr_dnlp_dbg("step 0.2.1: bld_lvl=%02d, lSby=%02d\n",
			bld_lvl, lSby);
		for (i = 0; i < 10; i++)
			pr_dnlp_dbg("tLumAvg[%d]: = %d\n",
				i, tLumAvg[29 - i]);
		for (i = 0; i < 10; i++)
			pr_dnlp_dbg("tAvgDif[%d]: = %d\n",
				i, tAvgDif[29 - i]);
	}

	/* post processing */
	if (dnlp_alg_param.dnlp_respond_flag) {
		bld_lvl = RBASE;
		dnlp_scn_chg = 1;
	} else if (bld_lvl >= RBASE) {
		bld_lvl = RBASE;
		dnlp_scn_chg = 1;
	}

	return bld_lvl;  /* tiir coefs */
}



int curve_rfrsh_chk_v2(int hstSum, int rbase)
{
	int i = 0;
	int sumshft = 0;
	unsigned int norm14 = 0;
	unsigned int dif = 0;
	unsigned int ihst_norm[65];
	static unsigned int HstDif[8];
	static unsigned int ScnDif[8];
	static unsigned int pre_ihst_norm[65];
	static unsigned int tLumAvg[8];
	static unsigned int tAvgDif[8];
	int bld_lvl = 0;

	/*store frame dif for 8*/
	for (i = 0; i < 7; i++) {
		HstDif[i] = HstDif[i+1];
		ScnDif[i] = ScnDif[i+1];
		tLumAvg[i] = tLumAvg[i+1];
		tAvgDif[i] = tAvgDif[i+1];

	}

	tLumAvg[7] = 0;
	tAvgDif[7] = 0;
	/*cal bld_lvl*/
	tLumAvg[7] = (ve_dnlp_luma_sum + (hstSum >> 1)) / hstSum;
	tLumAvg[7] = ((tLumAvg[7] + 4) >> 3);
	tAvgDif[7] = (tLumAvg[7] > tLumAvg[6]) ?
		(tLumAvg[7] - tLumAvg[6]) : (tLumAvg[6] - tLumAvg[7]);

	bld_lvl = tAvgDif[7];
	bld_lvl = (bld_lvl << dnlp_alg_param.dnlp_schg_sft);
	/* play station: return with black scene intersection */
	if (tAvgDif[7] > bld_lvl)
		bld_lvl = tAvgDif[7];

	if (bld_lvl > rbase) {
		if (print_en) {
			for (i = 7; i >= 4; i--)
				pr_info("tLumAvg[%d]: = %d\n",
							i, tLumAvg[i]);
			for (i = 7; i >= 4; i--)
				pr_info("tAvgDif[%d]: = %d\n ",
							i, tAvgDif[i]);
		}
		bld_lvl = dnlp_alg_param.dnlp_cuvbld_min;

	} else if (bld_lvl < dnlp_alg_param.dnlp_cuvbld_min)
		bld_lvl = dnlp_alg_param.dnlp_cuvbld_min;
	else if (bld_lvl > dnlp_alg_param.dnlp_cuvbld_max)
		bld_lvl = dnlp_alg_param.dnlp_cuvbld_max;

	/*new add,180202*/
	sumshft = (hstSum >= (1 << 24)) ? 8 :
			  (hstSum >= (1 << 22)) ? 6 :
			  (hstSum >= (1 << 20)) ? 4 :
			  (hstSum >= (1 << 18)) ? 2 :
			  (hstSum >= (1 << 16)) ? 0 :
			  (hstSum >= (1 << 14)) ? -2 :
			  (hstSum >= (1 << 12)) ? -4 :
			  (hstSum >= (1 << 10)) ? -6 :
			  (hstSum >= (1 << 8)) ? -8 :
			  (hstSum >= (1 << 6)) ? -10 :
			  (hstSum >= (1 << 4)) ? -12 : -16;
	if (sumshft >= 0)
		norm14 = (1 << 30) / (hstSum >> sumshft);
	else if (sumshft > -16)
		norm14 = (1 << (30 + sumshft)) / hstSum;
	else
		norm14 = 0;

	ScnDif[7] = 0;
	HstDif[7] = 0;
	for (i = 0; i < 64; i++) {
		ihst_norm[i] = pre_0_gamma[i];
		/*calc hist diff*/
		dif = ihst_norm[i] > pre_ihst_norm[i] ?
			(ihst_norm[i] - pre_ihst_norm[i]) :
			(pre_ihst_norm[i] - ihst_norm[i]);
		HstDif[7] += dif;
		/*hist delay*/
		pre_ihst_norm[i] = ihst_norm[i];
	}
	/*hist_diff sum norm to u16*/
	if (sumshft >= 0)
		HstDif[7] = ((HstDif[7] >> sumshft) * norm14 +
			(1 << 13)) >> 14;
	else
		HstDif[7] = ((HstDif[7] << (-sumshft)) * norm14 +
			(1 << 13)) >> 14;

	/*if scene change,hist dif is large*/
	ScnDif[7] = HstDif[7] > HstDif[6] ?
		(HstDif[7] - HstDif[6]) :
		(HstDif[6] - HstDif[7]);
	ScnDif[7] = ScnDif[7] >> NORM;/*u6*/
	if ((dnlp_printk >> 1) & 0x01) {
		for (i = 7; i >= 5; i--)
			pr_dnlp_dbg("\n$$$curve_rfrsh_chk_v2():ScnDif[%d]: = %d\n",
			i, ScnDif[i]);
	}

	if (ScnDif[7] > scn_chg_th) {/*scn_chg_th need tunning*/
		dnlp_scn_chg = 1;

		if (print_en) {
			pr_info("\n*********************\n");
			pr_info("\n === nTstCnt= %d (in chg) ==\n\n", nTstCnt);
			pr_info("\n$$$hstSum=%d,sumshft=%d,norm14=%d,ve_dnlp_luma_sum=%d\n",
				hstSum, sumshft, norm14, ve_dnlp_luma_sum);
			pr_info("### scene changed ! ScnDif[19](%d) > scn_chg_th(%d)!\n ",
				ScnDif[7], scn_chg_th);
			for (i = 7; i >= 2; i--)
				pr_info("$$$curve_rfrsh_chk_v2():HstDif[%d]: = %d\n",
					i, HstDif[i]);
			pr_info("================================\n");
			for (i = 7; i >= 2; i--)
				pr_info("$$$curve_rfrsh_chk_v2():ScnDif[%d]: = %d\n",
					i, ScnDif[i]);
		}
	} else if (dnlp_alg_param.dnlp_respond_flag)
		dnlp_scn_chg = 1;
	else
		dnlp_scn_chg = 0;

	return bld_lvl;  /* tiir coefs */
}


static void dnlp3_param_refrsh(void)
{
	if (dnlp_alg_param.dnlp_respond) {
		if ((prev_dnlp_mvreflsh != dnlp_alg_param.dnlp_mvreflsh) ||
			(prev_dnlp_final_gain !=
				dnlp_alg_param.dnlp_final_gain) ||
			(prev_dnlp_clahe_gain_neg !=
				dnlp_alg_param.dnlp_clahe_gain_neg) ||
			(prev_dnlp_clahe_gain_pos !=
				dnlp_alg_param.dnlp_clahe_gain_pos) ||

			(prev_dnlp_gmma_rate != ve_dnlp_gmma_rate) ||
			(prev_dnlp_lowalpha_v3 != ve_dnlp_lowalpha_v3) ||
			(prev_dnlp_hghalpha_v3 != ve_dnlp_hghalpha_v3) ||
			(prev_dnlp_sbgnbnd != dnlp_alg_param.dnlp_sbgnbnd) ||
			(prev_dnlp_sendbnd != dnlp_alg_param.dnlp_sendbnd) ||
			(prev_dnlp_cliprate_v3 !=
				dnlp_alg_param.dnlp_cliprate_v3) ||
			(prev_dnlp_clashBgn != dnlp_alg_param.dnlp_clashBgn) ||
			(prev_dnlp_clashEnd  != dnlp_alg_param.dnlp_clashEnd) ||
			(prev_dnlp_mtdbld_rate !=
				dnlp_alg_param.dnlp_mtdbld_rate) ||
			/*(prev_dnlp_pst_gmarat != ve_dnlp_pst_gmarat) || */
			(prev_dnlp_blk_cctr !=
				dnlp_alg_param.dnlp_blk_cctr) ||
			(prev_dnlp_brgt_ctrl !=
				dnlp_alg_param.dnlp_brgt_ctrl) ||
			(prev_dnlp_brgt_range !=
				dnlp_alg_param.dnlp_brgt_range) ||
			(prev_dnlp_brght_add !=
				dnlp_alg_param.dnlp_brght_add) ||
			(prev_dnlp_brght_max !=
				dnlp_alg_param.dnlp_brght_max) ||
			(prev_dnlp_lgst_ratio != ve_dnlp_lgst_ratio) ||
			(prev_dnlp_lgst_dst != ve_dnlp_lgst_dst) ||
			(prev_dnlp_almst_wht != ve_dnlp_almst_wht) ||
			/*(prev_dnlp_pstgma_end != ve_dnlp_pstgma_end) || */
			/* (prev_dnlp_pstgma_ratio  */
			/* != ve_dnlp_pstgma_ratio) || */
			/* (prev_dnlp_pstgma_brghtrate != */
			/* ve_dnlp_pstgma_brghtrate) || */
			/* (prev_dnlp_pstgma_brghtrat1 != */
			/* ve_dnlp_pstgma_brghtrat1) || */
			(prev_dnlp_blkext_rate !=
				dnlp_alg_param.dnlp_blkext_rate) ||
			(prev_dnlp_whtext_rate !=
				dnlp_alg_param.dnlp_whtext_rate) ||
			(prev_dnlp_blkext_ofst !=
				dnlp_alg_param.dnlp_blkext_ofst) ||
			(prev_dnlp_whtext_ofst !=
				dnlp_alg_param.dnlp_whtext_ofst) ||
			(prev_dnlp_bwext_div4x_min !=
				dnlp_alg_param.dnlp_bwext_div4x_min) ||
			(prev_dnlp_schg_sft !=
				dnlp_alg_param.dnlp_schg_sft) ||
			(prev_dnlp_smhist_ck !=
				dnlp_alg_param.dnlp_smhist_ck) ||
			(prev_dnlp_cuvbld_min !=
				dnlp_alg_param.dnlp_cuvbld_min) ||
			(prev_dnlp_cuvbld_max !=
				dnlp_alg_param.dnlp_cuvbld_max) ||
			(prev_dnlp_dbg_map !=
				dnlp_alg_param.dnlp_dbg_map) ||
			(prev_dnlp_dbg_adjavg !=
				dnlp_alg_param.dnlp_dbg_adjavg) ||
			(prev_dnlp_dbg_i2r !=
				dnlp_alg_param.dnlp_dbg_i2r) ||
			(prev_dnlp_slow_end != ve_dnlp_slow_end) ||
			(prev_dnlp_pavg_btsft !=
				dnlp_alg_param.dnlp_pavg_btsft) ||
			(prev_dnlp_cliprate_min !=
				dnlp_alg_param.dnlp_cliprate_min) ||
			(prev_dnlp_adpcrat_lbnd !=
				dnlp_alg_param.dnlp_adpcrat_lbnd) ||
			(prev_dnlp_adpcrat_hbnd !=
				dnlp_alg_param.dnlp_adpcrat_hbnd) ||
			(prev_dnlp_adpmtd_lbnd !=
				dnlp_alg_param.dnlp_adpmtd_lbnd) ||
			(prev_dnlp_adpmtd_hbnd !=
				dnlp_alg_param.dnlp_adpmtd_hbnd) ||
			(prev_dnlp_satur_rat !=
				dnlp_alg_param.dnlp_satur_rat) ||
			(prev_dnlp_satur_max !=
				dnlp_alg_param.dnlp_satur_max) ||
			(prev_dnlp_set_saturtn !=
				dnlp_alg_param.dnlp_set_saturtn) ||

			(prev_dnlp_lowrange != dnlp_alg_param.dnlp_lowrange) ||
			(prev_dnlp_hghrange != dnlp_alg_param.dnlp_hghrange) ||
			(prev_dnlp_auto_rng != dnlp_alg_param.dnlp_auto_rng) ||
			(prev_dnlp_bbd_ratio_low !=
				dnlp_alg_param.dnlp_bbd_ratio_low) ||
			(prev_dnlp_bbd_ratio_hig !=
				dnlp_alg_param.dnlp_bbd_ratio_hig) ||
			(prev_dnlp_adpalpha_lrate !=
				ve_dnlp_adpalpha_lrate) ||
			(prev_dnlp_adpalpha_hrate !=
				ve_dnlp_adpalpha_hrate))
			dnlp_alg_param.dnlp_respond_flag = 1;
		else
			dnlp_alg_param.dnlp_respond_flag = 0;
	}

	prev_dnlp_mvreflsh	 = dnlp_alg_param.dnlp_mvreflsh;
	prev_dnlp_final_gain = dnlp_alg_param.dnlp_final_gain;
	prev_dnlp_clahe_gain_neg =
		dnlp_alg_param.dnlp_clahe_gain_neg; /* clahe_gain */
	prev_dnlp_clahe_gain_pos =
		dnlp_alg_param.dnlp_clahe_gain_pos; /* clahe_gain */
	prev_dnlp_gmma_rate  = ve_dnlp_gmma_rate;
	prev_dnlp_lowalpha_v3 = ve_dnlp_lowalpha_v3;
	prev_dnlp_hghalpha_v3 = ve_dnlp_hghalpha_v3;
	prev_dnlp_sbgnbnd	  = dnlp_alg_param.dnlp_sbgnbnd;
	prev_dnlp_sendbnd	  = dnlp_alg_param.dnlp_sendbnd;
	prev_dnlp_cliprate_v3 = dnlp_alg_param.dnlp_cliprate_v3;
	prev_dnlp_clashBgn	  = dnlp_alg_param.dnlp_clashBgn;
	prev_dnlp_clashEnd	  = dnlp_alg_param.dnlp_clashEnd;
	prev_dnlp_mtdbld_rate = dnlp_alg_param.dnlp_mtdbld_rate;
	/* prev_dnlp_pst_gmarat  = ve_dnlp_pst_gmarat; */
	prev_dnlp_blk_cctr	  = dnlp_alg_param.dnlp_blk_cctr;
	prev_dnlp_brgt_ctrl   = dnlp_alg_param.dnlp_brgt_ctrl;
	prev_dnlp_brgt_range  = dnlp_alg_param.dnlp_brgt_range;
	prev_dnlp_brght_add   = dnlp_alg_param.dnlp_brght_add;
	prev_dnlp_brght_max   = dnlp_alg_param.dnlp_brght_max;
	prev_dnlp_lgst_ratio	= ve_dnlp_lgst_ratio;
	prev_dnlp_lgst_dst	  = ve_dnlp_lgst_dst;
	prev_dnlp_almst_wht = ve_dnlp_almst_wht;

	/* prev_dnlp_pstgma_end = ve_dnlp_pstgma_end; */
	/* prev_dnlp_pstgma_ratio = ve_dnlp_pstgma_ratio; */
	/* prev_dnlp_pstgma_brghtrate = ve_dnlp_pstgma_brghtrate; */
	/* prev_dnlp_pstgma_brghtrat1 = ve_dnlp_pstgma_brghtrat1; */

	prev_dnlp_blkext_rate = dnlp_alg_param.dnlp_blkext_rate;
	prev_dnlp_whtext_rate = dnlp_alg_param.dnlp_whtext_rate;
	prev_dnlp_blkext_ofst = dnlp_alg_param.dnlp_blkext_ofst;
	prev_dnlp_whtext_ofst = dnlp_alg_param.dnlp_whtext_ofst;
	prev_dnlp_bwext_div4x_min = dnlp_alg_param.dnlp_bwext_div4x_min;

	prev_dnlp_schg_sft = dnlp_alg_param.dnlp_schg_sft;

	prev_dnlp_smhist_ck = dnlp_alg_param.dnlp_smhist_ck;
	prev_dnlp_cuvbld_min = dnlp_alg_param.dnlp_cuvbld_min;
	prev_dnlp_cuvbld_max = dnlp_alg_param.dnlp_cuvbld_max;
	prev_dnlp_dbg_map = dnlp_alg_param.dnlp_dbg_map;
	prev_dnlp_dbg_adjavg = dnlp_alg_param.dnlp_dbg_adjavg;
	prev_dnlp_dbg_i2r = dnlp_alg_param.dnlp_dbg_i2r;
	prev_dnlp_slow_end = ve_dnlp_slow_end;
	prev_dnlp_pavg_btsft = dnlp_alg_param.dnlp_pavg_btsft;
	prev_dnlp_pavg_btsft = dnlp_alg_param.dnlp_pavg_btsft;
	prev_dnlp_cliprate_min = dnlp_alg_param.dnlp_cliprate_min;
	prev_dnlp_adpcrat_lbnd = dnlp_alg_param.dnlp_adpcrat_lbnd;
	prev_dnlp_adpcrat_hbnd = dnlp_alg_param.dnlp_adpcrat_hbnd;

	prev_dnlp_adpmtd_lbnd = dnlp_alg_param.dnlp_adpmtd_lbnd;
	prev_dnlp_adpmtd_hbnd = dnlp_alg_param.dnlp_adpmtd_hbnd;

	prev_dnlp_satur_rat = dnlp_alg_param.dnlp_satur_rat;
	prev_dnlp_satur_max = dnlp_alg_param.dnlp_satur_max;
	prev_dnlp_set_saturtn = dnlp_alg_param.dnlp_set_saturtn;

	prev_dnlp_lowrange = dnlp_alg_param.dnlp_lowrange;
	prev_dnlp_hghrange = dnlp_alg_param.dnlp_hghrange;
	prev_dnlp_auto_rng = dnlp_alg_param.dnlp_auto_rng;

	prev_dnlp_bbd_ratio_low = dnlp_alg_param.dnlp_bbd_ratio_low;
	prev_dnlp_bbd_ratio_hig = dnlp_alg_param.dnlp_bbd_ratio_hig;
	prev_dnlp_adpalpha_hrate = ve_dnlp_adpalpha_hrate;
	prev_dnlp_adpalpha_lrate = ve_dnlp_adpalpha_lrate;
}


static void dnlp_rfrsh_subgmma(void)
{
	int i = 0;
	static unsigned int pgmma0[65]; /* 0~4096*/
	static unsigned int pgmma1[65];

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++) {
			pgmma0[i] = (i << 6); /* 0 ~4096 */
			pgmma1[i] = (i << 6); /* 0 ~4096 */
		}
	}

	if (!dnlp_scn_chg)
		for (i = 0; i < 65; i++) {
			gma_scurve0[i] = dnlp_bld_lvl *
					(gma_scurve0[i] << 2) + (RBASE >> 1);
			gma_scurve1[i] = dnlp_bld_lvl *
					(gma_scurve1[i] << 2) + (RBASE >> 1);

			gma_scurve0[i] = gma_scurve0[i] +
					(RBASE - dnlp_bld_lvl) * pgmma0[i];
			gma_scurve1[i] = gma_scurve1[i] +
					(RBASE - dnlp_bld_lvl) * pgmma1[i];

			gma_scurve0[i] = (gma_scurve0[i] >>
				dnlp_alg_param.dnlp_mvreflsh);
			gma_scurve1[i] = (gma_scurve1[i] >>
				dnlp_alg_param.dnlp_mvreflsh);

			pgmma0[i] = gma_scurve0[i]; /* 0~ 4095 */
			pgmma1[i] = gma_scurve1[i]; /* 0~ 4095 */

			gma_scurve0[i] = (gma_scurve0[i] + 2) >> 2; /* 1023 */
			gma_scurve1[i] = (gma_scurve1[i] + 2) >> 2; /* 1023 */
		}
	else
		for (i = 0; i < 65; i++) {
			pgmma0[i] = (gma_scurve0[i] << 2);
			pgmma1[i] = (gma_scurve1[i] << 2);
		}
}


/* function: IIR filter (time domain) on the input histogram and luma__sum*/
/*	 in&out: pre_0_gamma, luma_sum (ve_luma_sum)*/
static void dnlp_inhist_tiir(void)
{
	int i = 0;
	int j = 0;
	int nTmp = 0;
	static unsigned int pgmma0[65];
	/* local static variables for IIR filter */
	static unsigned int luma_sum;

	if (!dnlp_scn_chg && (dnlp_alg_param.dnlp_dbg_i2r & 0x1)) {
		for (i = 0; i < 65; i++) {
			nTmp = dnlp_bld_lvl * pre_0_gamma[i] + (RBASE >> 1);
			nTmp = nTmp + (RBASE - dnlp_bld_lvl) * pgmma0[i];
			nTmp = (nTmp >> dnlp_alg_param.dnlp_mvreflsh);
			pre_0_gamma[i] = nTmp;
		}
	/* data overrun error !
		nTmp = dnlp_bld_lvl * ve_dnlp_luma_sum + (RBASE >> 1);
		nTmp = nTmp + (RBASE - dnlp_bld_lvl) * luma_sum;
		nTmp = (nTmp >> dnlp_alg_param.dnlp_mvreflsh);
		ve_dnlp_luma_sum = nTmp;
		*/
	}

	for (i = 0; i < 65; i++)
		pgmma0[i] = pre_0_gamma[i];
	luma_sum = ve_dnlp_luma_sum;

	if ((dnlp_printk >> 1) & 0x1) {
		pr_dnlp_dbg("\n#### dnlp_inhist_tiir()-3: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
				pre_0_gamma[i], pre_0_gamma[i+1],
				pre_0_gamma[i+2], pre_0_gamma[i+3],
				pre_0_gamma[i+4], pre_0_gamma[i+5],
				pre_0_gamma[i+6], pre_0_gamma[i+7],
				pre_0_gamma[i+8], pre_0_gamma[i+9],
				pre_0_gamma[i+10], pre_0_gamma[i+11],
				pre_0_gamma[i+12], pre_0_gamma[i+13],
				pre_0_gamma[i+14], pre_0_gamma[i+15]);
		}
		pr_dnlp_dbg("  ]\n");
		pr_dnlp_dbg("### ve_dnlp_luma_sum(iir) =%d\n",
			ve_dnlp_luma_sum);
	}

}

/*0 ~ 65*/
/*  in : gmma_rate, low_alpha, hgh_alpha (from dnlp_params_hist()),*/
/*	lsft_avg(from cal_hst_shft_avf() )*/
/*	out: gma_scurvet (0~65)  */
static void dnlp_gmma_cuvs(unsigned int gmma_rate,
	unsigned int low_alpha, unsigned int hgh_alpha,
	unsigned int lsft_avg)
{
	int i = 0;
	int j = 0;
	int nTmp = 0;
	unsigned int luma_avg4 = (lsft_avg >> dnlp_alg_param.dnlp_pavg_btsft);

	static unsigned int pgmma[65];
	unsigned short *glb_scurve;

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pgmma[i] = (i << 6); /* 0 ~4096 */
	}

	/* refresh sub gamma */
	if ((dnlp_alg_param.dnlp_dbg_i2r >> 1) & 0x1)
		dnlp_rfrsh_subgmma();

	for (i = 0; i < 65; i++) {
		nTmp = (((256 - gmma_rate)*gma_scurve0[i] +
			gma_scurve1[i]*gmma_rate + 128) >> 8); /* 0 ~1023 */

		if (nTmp <= (luma_avg4<<2))
			nTmp = (nTmp*(64 - low_alpha) +
				(low_alpha*i<<4) + 8)>>4; /*4096*/
		else
			nTmp = (nTmp*(64 - hgh_alpha) +
				(hgh_alpha*i<<4) + 8)>>4;

	if (debug_add_curve_en) {
		glb_scurve = kzalloc(65*sizeof(unsigned short),
			GFP_KERNEL);
		if (glb_scurve == NULL)
			return;
		nTmp = ((128 - glb_scurve_bld_rate)*nTmp +
			glb_scurve_bld_rate*(glb_scurve[i]<<4) + 64)>>7;
		kfree(glb_scurve);
	}

		if (nTmp < 0)
			nTmp = 0;
		else if (nTmp > 4095)
			nTmp = 4095;
		gma_scurvet[i] = nTmp;

		if ((dnlp_printk >> 17) & 0x1)
			pr_dnlp_dbg("Step D-1.z: gmma_cuvs_bld: [%02d] (s0)%4d (s1)%4d => (s)%4d\n",
				i, gma_scurve0[i], gma_scurve1[i],
				gma_scurvet[i]);
	}

	if (!dnlp_scn_chg && ((dnlp_alg_param.dnlp_dbg_i2r >> 2) & 0x1))
		for (i = 0; i < 65; i++) {
			nTmp = dnlp_bld_lvl * gma_scurvet[i] + (RBASE >> 1);
			nTmp = nTmp + (RBASE - dnlp_bld_lvl) * pgmma[i];
			nTmp = (nTmp >> dnlp_alg_param.dnlp_mvreflsh);

			gma_scurvet[i] = nTmp; /* 4095 */
		}

	for (i = 0; i < 65; i++)
		pgmma[i] = gma_scurvet[i]; /* 4095 */

	for (i = 0; i < 65; i++)
		gma_scurvet[i] = ((gma_scurvet[i] + 2) >> 2); /*1023*/

	if ((dnlp_printk >> 17) & 0x1) {
		pr_dnlp_dbg("Step D-1.z: gmma_cuvs_bld: gma_scurvet cal paramets: lsft_avg=%d gmma_rate=%d low_alpha%d hgh_alpha=%d\n",
			lsft_avg, gmma_rate, low_alpha, hgh_alpha);
	}


	/* --draw curve */
	if ((dnlp_printk>>17) & 0x1) {
		pr_dnlp_dbg("\n#### gma_s_bld: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
				gma_scurvet[i],
				gma_scurvet[i+1],
				gma_scurvet[i+2],
				gma_scurvet[i+3],
				gma_scurvet[i+4],
				gma_scurvet[i+5],
				gma_scurvet[i+6],
				gma_scurvet[i+7],
				gma_scurvet[i+8],
				gma_scurvet[i+9],
				gma_scurvet[i+10],
				gma_scurvet[i+11],
				gma_scurvet[i+12],
				gma_scurvet[i+13],
				gma_scurvet[i+14],
				gma_scurvet[i+15]);
		}
		pr_dnlp_dbg(" %d ]\n", gma_scurvet[64]);

	}

}


/* clsh_scvbld = clash_curve + gma_scurvet */
/*  in : mtdbld_rate(from dnlp_adp_alpharate() )*/
/*	out: clsh_scvbld   */
static void dnlp_clsh_sbld(unsigned int mtdbld_rate)
{
	int i = 0;
	int j = 0;
	int nTmp0 = 0;

	static unsigned int pgmma[65] = {
		  0,   16,   32,  48,  64,  80,  96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  };

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pgmma[i] = (i << 4); /* 0 ~1024 */
	}

	for (i = 0; i < 65; i++) {
		/* nTmp0 = gma_scurvet[i];*/ /* 0 ~1024 */
		nTmp0 = GmScurve[i];/* GmScurve,new s_curve 2017.12.23 */
		nTmp0 = nTmp0*mtdbld_rate + clash_curve[i]*(64 - mtdbld_rate);
		nTmp0 = (nTmp0 + 32)>>6; /* 0~1024 */
		clsh_scvbld[i] = nTmp0;

		if ((dnlp_printk >> 7) & 0x1)
			pr_dnlp_dbg("!!c_s_bld(): GmScurve:%d, clash_curve:%d => clsh_scvbld=%d(%d/64)\n",
				GmScurve[i], clash_curve[i],
				clsh_scvbld[i], mtdbld_rate);
	}
	if ((dnlp_printk >> 7) & 0x1)
		pr_dnlp_dbg(" @@@ dnlp_clsh_sbld(before use): mtdbld_rate=%d, dnlp_bld_lvl=%d, dbg_i2r=%d, scn_chg=%d\n",
			mtdbld_rate, dnlp_bld_lvl,
			dnlp_alg_param.dnlp_dbg_i2r, dnlp_scn_chg);

	if (!dnlp_scn_chg && ((dnlp_alg_param.dnlp_dbg_i2r >> 4) & 0x1))
		for (i = 0; i < 65; i++) {
			nTmp0 = dnlp_bld_lvl * clsh_scvbld[i] + (RBASE >> 1);
			nTmp0 = nTmp0 + (RBASE - dnlp_bld_lvl) * pgmma[i];
			nTmp0 = (nTmp0 >> dnlp_alg_param.dnlp_mvreflsh);
			if ((dnlp_printk >> 7) & 0x1)
				pr_dnlp_dbg("!! clsh_sbld_iir: clsh_scvbld=%d, pre=%d => crt=%d\n",
					clsh_scvbld[i], pgmma[i], nTmp0);
			clsh_scvbld[i] = nTmp0;
		}

	for (i = 0; i < 65; i++)
		pgmma[i] = clsh_scvbld[i]; /* 1023 */

	/* --draw curve */
	if ((dnlp_printk >> 7) & 0x1) {
		pr_dnlp_dbg("\n#### clsh_scv_bld: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
				clsh_scvbld[i], clsh_scvbld[i+1],
				clsh_scvbld[i+2], clsh_scvbld[i+3],
				clsh_scvbld[i+4], clsh_scvbld[i+5],
				clsh_scvbld[i+6], clsh_scvbld[i+7],
				clsh_scvbld[i+8], clsh_scvbld[i+9],
				clsh_scvbld[i+10], clsh_scvbld[i+11],
				clsh_scvbld[i+12], clsh_scvbld[i+13],
				clsh_scvbld[i+14], clsh_scvbld[i+15]);
		}
		pr_dnlp_dbg(" %d ]\n", clsh_scvbld[64]);
	}

}


/*  in : blk_gma_rat[64] (from get_blk_gma_rat() ) */
/* out : blk_gma_bld = blk_gma_crv + clsh_scvbld */




/* blkwht_ebld = blk_gma_bld + extension */
/* input:  blk_gma_bld[] , blk_wht_ext(from cal_bwext_param() ),
 * dnlp_brightness (from cal_brght_plus() ), luma_avg4,luma_avg,
 * iRgnBgn,iRgnEnd
 */
/* output: blkwht_ebld[] */
static void dnlp_blkwht_bld(int *o_curv, int *i_curv,
	int *blk_wht_ext, int bright,
	unsigned int luma_avg4, unsigned int luma_avg,
	unsigned int iRgnBgn, unsigned int iRgnEnd)
{
	int i, j, i4;
	int tmp0, tmp1, margin_wht64x, margin_blk64x;
	int div4x[4], dist4, mid4, lft4, rgh4, divd;
	int margin64x, delta, st, ed, whtext_gain;
	/* local of bw extension */
	static unsigned int bwext_curv[65] = {
		  0,   16,   32,  48,  64,  80,  96, 112,
		  128, 144, 160, 176, 192, 208, 224, 240,
		  256, 272, 288, 304, 320, 336, 352, 368,
		  384, 400, 416, 432, 448, 464, 480, 496,
		  512, 528, 544, 560, 576, 592, 608, 624,
		  640, 656, 672, 688, 704, 720, 736, 752,
		  768, 784, 800, 816, 832, 848, 864, 880,
		  896, 912, 928, 944, 960, 976, 992, 1008, 1024
		  };
	/* get it parametered */
	int min_div4x = dnlp_alg_param.dnlp_bwext_div4x_min;

	/* adaptive calc on bin st and ed. */
	st = iRgnBgn - dnlp_alg_param.dnlp_blkext_ofst;   if (st < 0)	st = 0;
	ed = iRgnEnd + dnlp_alg_param.dnlp_whtext_ofst;   if (ed > 63)	ed = 63;
	if ((dnlp_printk >> 8) & 0x1) {
		pr_dnlp_dbg("stepD-4.z: blk/wht ext: blk_wht_ext[%d %d] + bright%d, Rgn=[%d %d], sted[%d %d]\n",
			blk_wht_ext[0], blk_wht_ext[1],
			bright, iRgnBgn, iRgnEnd, st, ed);
	}

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			bwext_curv[i] = (i << 4); /* 0 ~1024 */
	}

	/* get the maximum margin for BW extension */
	tmp0 = (luma_avg < 48) ? luma_avg:47;
	/* norm to 256 as 1, white region will
	 * not apply strong white extension
	 */
	whtext_gain = wext_gain[tmp0];
	whtext_gain = (whtext_gain*dnlp_alg_param.dnlp_whtext_rate)>>8;

	margin_blk64x = (i_curv[blk_wht_ext[0]>>4]-(st<<4));
	if (margin_blk64x < 0)
		margin_blk64x = 0;
	margin_wht64x = ((ed<<4) - i_curv[blk_wht_ext[1]>>4]);
	if (margin_wht64x < 0)
		margin_wht64x = 0;
	margin_blk64x = margin_blk64x*dnlp_alg_param.dnlp_blkext_rate;/* 32 */
	margin_wht64x = margin_wht64x*whtext_gain;

	/* get the mid point blk and wht to the luma_avg */
	lft4 = (blk_wht_ext[0]>>2);
	rgh4 = (blk_wht_ext[1]>>2);
	if (luma_avg4 < lft4)
		mid4  =  (lft4);
	else if (luma_avg4 > rgh4)
		mid4  =  (rgh4);
	else
		mid4  =  luma_avg4;

	div4x[0] = mid4 - (lft4);
	if (div4x[0] < min_div4x)
		div4x[0] = min_div4x;
	div4x[1] = (rgh4) - mid4;
	if (div4x[1] < min_div4x)
		div4x[1] = min_div4x;
	div4x[2] = (lft4) - (st<<2);
	if (div4x[2] < min_div4x)
		div4x[2] = min_div4x;
	div4x[3] = (ed<<2) - (rgh4);
	if (div4x[3] < min_div4x)
		div4x[3] = min_div4x;

	if ((dnlp_printk >> 8) & 0x1)
		pr_dnlp_dbg("bwext: lft4=%04d, mid4=%04d, rgh4=%4d, div4x=[%4d %4d %4d %4d]\n",
			lft4, mid4, rgh4, div4x[0],
			div4x[1], div4x[2], div4x[3]);

	/* black / white extension */
	for (i = 0; i < 64; i++) {
		i4 = (i<<2);
		if (i4 <= lft4) {
			dist4 = i4 - (st<<2);
			divd = div4x[2];
			margin64x = -margin_blk64x;
		} else if (i4 < mid4) {
			dist4 = (mid4 - i4);
			divd = div4x[0];
			margin64x = -margin_blk64x;
		} else if (i4 < rgh4) {
			dist4 = (i4 - mid4);
			divd = div4x[1];
			margin64x =  margin_wht64x;
		} else {
			dist4 = ((ed<<2) - i4);
			divd = div4x[3];
			margin64x =  margin_wht64x;
		}

		/* calculate the delta */
		if (dist4 > 0) {
			delta = margin64x*dist4/divd;
			delta = (delta > 0) ? (delta>>6) :
				(delta < 0) ? (-((-delta)>>6)) : 0;
		} else
			delta = 0;

		/* nTmp += dnlp_brightness; */
	if (BLE_en)
		tmp0 = bright + delta + (int)i_curv[i];
	else
		tmp0 = (int)i_curv[i];

	if (tmp0 < 0)
		tmp0 = 0;
	else if (tmp0 > 1023)
		tmp0 = 1023;
	else
		tmp0 = tmp0;

	   /* IIR filter */
	if (!dnlp_scn_chg && ((dnlp_alg_param.dnlp_dbg_i2r >> 6) & 0x1)) {
		tmp1 = dnlp_bld_lvl * tmp0 + (RBASE >> 1);
		tmp1 = tmp1 + (RBASE - dnlp_bld_lvl) * bwext_curv[i];
		tmp1 = (tmp1 >> dnlp_alg_param.dnlp_mvreflsh);

		o_curv[i] = tmp1;
	} else
			o_curv[i] = tmp0;
		 /* for next frame */
		 bwext_curv[i] = o_curv[i];

		if ((dnlp_printk >> 8) & 0x1)
		pr_dnlp_dbg("Step D-4.z: bwext[%3d]: dst4=%04d, divd=%04d, mg64x=%4d, delt(%4d)+ icurv(%4d) + brght(%4d) => blkwht_ebld(%04d ) iir(%4d)\n",
			i, dist4, divd, margin64x, delta,
			i_curv[i], bright, tmp0, o_curv[i]);

	}
	/* o_curv[64] = 1023; */
	o_curv[0]  = 0;
	o_curv[64] = 1023;	 /* output */

	if ((dnlp_printk>>8) & 0x1) {
		pr_dnlp_dbg("\n#### BW_extent: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
				o_curv[i], o_curv[i+1],
				o_curv[i+2], o_curv[i+3],
				o_curv[i+4], o_curv[i+5],
				o_curv[i+6], o_curv[i+7],
				o_curv[i+8], o_curv[i+9],
				o_curv[i+10], o_curv[i+11],
				o_curv[i+12], o_curv[i+13],
				o_curv[i+14], o_curv[i+15]);
		}
		pr_dnlp_dbg("]\n");

	}


}


/*   patch for black+white stripe */
/*	   in: mMaxLst,mMaxIdx, iir_hstSum */
/*	  out: *gmma_rate,*low_alpha,*hgh_alpha,*mtdbld_rate	*/
void patch_blk_wht(unsigned int *mMaxLst, unsigned int *mMaxIdx,
	unsigned int *gmma_rate, unsigned int *low_alpha,
	unsigned int *hgh_alpha, unsigned int *mtdbld_rate,
	unsigned int iir_hstSum)
{
	int nTmp = 0, nT0 = 0, nT1 = 0;

	if (mMaxIdx[1] > mMaxIdx[0]) {
		nT0 = mMaxIdx[0];
		nT1 = 63 - mMaxIdx[1];
	} else {
		nT0 = mMaxIdx[1];
		nT1 = 63 - mMaxIdx[0];
	}
	nTmp = (nT0 < nT1) ? nT0 : nT1;
	nTmp = (nTmp > 16) ? 16 : nTmp;

	if ((mMaxLst[1] > (ve_dnlp_lgst_ratio*iir_hstSum>>8)) &&
		((mMaxIdx[1] > (mMaxIdx[0] + ve_dnlp_lgst_dst)) ||
		(mMaxIdx[0] > (mMaxIdx[1] + ve_dnlp_lgst_dst)))) {
		*gmma_rate += (nTmp*(255 - *gmma_rate)>>4);
		*low_alpha -= (*low_alpha*nTmp>>4);
		*hgh_alpha -= (*hgh_alpha*nTmp>>4);
		*mtdbld_rate += (nTmp*(64 - *mtdbld_rate)>>4);

		if ((dnlp_printk >> 18) & 0x01)
			pr_dnlp_dbg("## patch_blk_wht()-2: special case:gmma_rate=%d low_alpha=%d hgh_alpha=%d mtdbld_rate=%d\n",
				*gmma_rate, *low_alpha,
				*hgh_alpha, *mtdbld_rate);
	}

}


/* function: based on luma_avg,luma_avg4 to calc blending coefs*/
/*	 out: gmma_rate,low_alpha,hgh_alpha;*/
/*		 mtdbld_rate;      */
static void dnlp_params_hist(unsigned int *gmma_rate,
	unsigned int *low_alpha, unsigned int *hgh_alpha,
	unsigned int *mtdbld_rate,
	unsigned int luma_avg, unsigned int luma_avg4)
{
	static unsigned int pgmma0[4][7] = {
		{0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0}
	};
	int nTmp = 0;
	int i = 0;
	int trate = *gmma_rate;
	int tlowa = *low_alpha;
	int thgha = *hgh_alpha;
	int tmrat = *mtdbld_rate;
	static unsigned int xL[32];  /* for test */

	nTmp = (luma_avg > 31) ? luma_avg-31 : 31-luma_avg;
	nTmp = (32 - nTmp + 2) >> 2;

	trate = trate + nTmp;
	if (trate > 255)
		trate = 255;
	if (luma_avg4 <= 32)
		tlowa = tlowa + (32 - luma_avg4);

	if (luma_avg4 >= 224) {
		if (tlowa < (luma_avg4 - 224))
			tlowa = 0;
		else
			tlowa = tlowa - (luma_avg4 - 224);
	}

	if (!dnlp_scn_chg) {
		for (i = 0; i < 7; i++) {
			trate += pgmma0[0][i];
			tlowa += pgmma0[1][i];
			thgha += pgmma0[2][i];
			tmrat += pgmma0[3][i];
		}
		trate = ((trate + 4)>>3);
		tlowa = ((tlowa + 4)>>3);
		thgha = ((thgha + 4)>>3);
		tmrat = ((tmrat + 4)>>3);

		for (i = 0; i < 6; i++) {
			pgmma0[0][i] = pgmma0[0][i + 1];
			pgmma0[1][i] = pgmma0[1][i + 1];
			pgmma0[2][i] = pgmma0[2][i + 1];
			pgmma0[3][i] = pgmma0[3][i + 1];
		}
		pgmma0[0][6] = trate;
		pgmma0[1][6] = tlowa;
		pgmma0[2][6] = thgha;
		pgmma0[3][6] = tmrat;
	} else
		for (i = 0; i < 7; i++) {
			pgmma0[0][i] = trate;
			pgmma0[1][i] = tlowa;
			pgmma0[2][i] = thgha;
			pgmma0[3][i] = tmrat;
		}
	*gmma_rate = trate;
	*low_alpha = tlowa;
	*hgh_alpha = thgha;
	*mtdbld_rate = tmrat;

	/* for debug only */
	for (i = 0; i < 31; i++)
		xL[i] = xL[i+1];
	xL[31] = luma_avg4;

	if ((dnlp_printk>>18)&0x1) {
		pr_dnlp_dbg("params_hist: gmma_rate=%d [%d %d %d %d %d %d %d],Tmp=%d luma_avg=%d (%d), alpha=[%d %d], mthd_rate=%d\n",
			(*gmma_rate), pgmma0[0][0], pgmma0[0][1],
			pgmma0[0][2], pgmma0[0][3], pgmma0[0][4],
			pgmma0[0][5], pgmma0[0][6], nTmp, luma_avg,
			luma_avg4, *low_alpha, *hgh_alpha, *mtdbld_rate);
		i = 1;
		pr_dnlp_dbg("params_hist: low_alpha=%d [%d %d %d %d %d %d %d]\n",
			(*low_alpha), pgmma0[i][0], pgmma0[i][1],
			pgmma0[i][2], pgmma0[i][3], pgmma0[i][4],
			pgmma0[i][5], pgmma0[i][6]);
		i = 2;
		pr_dnlp_dbg("params_hist: hgh_alpha=%d [%d %d %d %d %d %d %d]\n",
			(*hgh_alpha), pgmma0[i][0], pgmma0[i][1],
			pgmma0[i][2], pgmma0[i][3], pgmma0[i][4],
			pgmma0[i][5], pgmma0[i][6]);
		i = 3;
		pr_dnlp_dbg("params_hist: mtdbld_rate=%d [%d %d %d %d %d %d %d]\n",
			(*mtdbld_rate), pgmma0[i][0], pgmma0[i][1],
			pgmma0[i][2], pgmma0[i][3], pgmma0[i][4],
			pgmma0[i][5], pgmma0[i][6]);

	}
}


/* function: black bord detection, and histogram clipping*/
/*	     in: hstSum,pre_0_gamma*/
/*	    out: pre_0_gamma  */
static void dnlp_refine_bin0(int hstSum)
{
	static unsigned int tmp_sum[7];
	unsigned int nTmp = 0;
	unsigned int nTmp0 = 0;
	unsigned int nsum = 0;
	int i = 0;
	int j = 0;

	nTmp = (hstSum * dnlp_alg_param.dnlp_bbd_ratio_low + 128) >> 8;
	nTmp0 = pre_0_gamma[1] + pre_0_gamma[2];
	if (nTmp0 > nTmp)
		nTmp = nTmp0;

	if (pre_0_gamma[0] > nTmp) {
		if (pre_0_gamma[1] > nTmp)
			nTmp = pre_0_gamma[1];
		if (pre_0_gamma[2] > nTmp)
			nTmp = pre_0_gamma[2];

		nsum = pre_0_gamma[0] - nTmp;

		nTmp = (hstSum * dnlp_alg_param.dnlp_bbd_ratio_hig + 128) >> 8;
		if (nsum > nTmp)
			nsum = nTmp;
	}

	if (!dnlp_scn_chg) {
		for (j = 0; j < 7; j++)
			nsum += tmp_sum[j];
		nsum = ((nsum + 4) >> 3);

		for (j = 0; j < 6; j++)
			tmp_sum[j] = tmp_sum[j + 1];
		tmp_sum[6] = nsum;
	} else {
		for (j = 0; j < 7; j++)
			tmp_sum[j] = nsum;
	}

	if (dnlp_printk & 0x1)
		pr_dnlp_dbg("Bin0 Refine: -%4d\n", nsum);

	if (nsum >= pre_0_gamma[0])
		pre_0_gamma[0] = 0;
	else
		pre_0_gamma[0] = pre_0_gamma[0] - nsum;

	if ((dnlp_printk >> 1) & 0x1) {
		pr_dnlp_dbg("\n#### refine_bin0()-2: [\n");
		for (j = 0; j < 4; j++) {
			i = j*16;
			pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
			pre_0_gamma[i], pre_0_gamma[i+1],
			pre_0_gamma[i+2], pre_0_gamma[i+3],
			pre_0_gamma[i+4], pre_0_gamma[i+5],
			pre_0_gamma[i+6], pre_0_gamma[i+7],
			pre_0_gamma[i+8], pre_0_gamma[i+9],
			pre_0_gamma[i+10], pre_0_gamma[i+11],
			pre_0_gamma[i+12], pre_0_gamma[i+13],
			pre_0_gamma[i+14], pre_0_gamma[i+15]);
		}
		pr_dnlp_dbg("  ]\n");
	}
}


/*function: adaptive calc of coefs*/
/*	in : lmh_avg,dnlp_lowrange,dnlp_hghrange,*/
/*	&low_alpha,&hgh_alpha,&dnlp_pst_gmarat*/
/*	out:low_alpha,hgh_alph; (for gma_scurvet)*/
/*	dnlp_pst_gmarat;    (for blk_gma_rat calc) */
static void dnlp_adp_alpharate(unsigned int *lmh_avg,
	unsigned int *low_alpha, unsigned int *hgh_alpha,
	/*unsigned int *pst_gmarat, */
	unsigned int dnlp_lowrange, unsigned int dnlp_hghrange)
{
	int nTmp = 0;
	int ndif = 0;
	int nlap = 0;

	if ((dnlp_printk>>17)&0x1)
		pr_dnlp_dbg("input:lmh_avg= %3d %3d %3d %3d %3d, low_alpha=%2d, hgh_alpha=%2d\n",
			lmh_avg[0], lmh_avg[1], lmh_avg[2],
			lmh_avg[3], lmh_avg[4], *low_alpha, *hgh_alpha);

	if (dnlp_lowrange + lmh_avg[3] < 64) { /* decrease low alpha */
		nTmp = 64 - (dnlp_lowrange + lmh_avg[3]);
		nTmp = (ve_dnlp_adpalpha_lrate * nTmp + 16) >> 5;
		if (*low_alpha < nTmp)
			*low_alpha = 0;
		else
			*low_alpha = *low_alpha - nTmp;

		if ((dnlp_printk>>17)&0x1)
			pr_dnlp_dbg("Step C-1.y.1:out : low alpha-- (%3d) -> %2d\n",
				nTmp, *low_alpha);
	} else if (lmh_avg[3] > 64) { /* increase low alpha */
		ndif = lmh_avg[3] - 64;
		nlap = (ve_dnlp_adpalpha_lrate * ndif + 16) >> 5;
		if ((nlap + *low_alpha) > 64)
			*low_alpha = 64;
		else
			*low_alpha += nlap;

		if ((dnlp_printk>>17)&0x1)
			pr_dnlp_dbg("Step C-1.y.1:out :low alpha++ (%3d) -> %2d\n",
			nlap, *low_alpha);


	/*if (lmh_avg[4] < 16) { */
	/*		nbrt0 = ve_dnlp_pstgma_brghtrat1 * (16 - lmh_avg[4]); */
	/*		nbrt0 = (nbrt0 + 8) >> 4; */
	/*	} */
	/*	nbrt1 = (ve_dnlp_pstgma_brghtrate * ndif + 16) >> 6; */

	/*	nTmp = nbrt0 + nbrt1; */

	/*	if ((*pst_gmarat + nTmp) > 64) */
	/*		*pst_gmarat = 64; */
	/*	else */
	/*		*pst_gmarat += nTmp; */

	/*	if (prt_flag) */
	/*		pr_info("Step C-1.y.1:out :  */
	/* pstgma(+%2d +%2d)(%2d)\n", */
	/*		nbrt0, nbrt1, *pst_gmarat); */

	}

	if (lmh_avg[2] < 64 - dnlp_hghrange) { /* decrease hgh alpha */
		nTmp = 64 - dnlp_hghrange - lmh_avg[2];
		nTmp = (ve_dnlp_adpalpha_hrate * nTmp + 16) >> 5;
		if (*hgh_alpha < nTmp)
			*hgh_alpha = 0;
		else
			*hgh_alpha = *hgh_alpha - nTmp;
		if ((dnlp_printk>>17)&0x1)
			pr_dnlp_dbg("Step C-1.y.1:out: hgh alpha-- (%3d) -> %2d\n",
				nTmp, *hgh_alpha);
	} else if (lmh_avg[2] > 63) { /* increase hgh alpha */
		nTmp = lmh_avg[2] - 63;
		nTmp = (ve_dnlp_adpalpha_hrate * nTmp + 16) >> 5;
		if ((nTmp + *hgh_alpha) > 64)
			*hgh_alpha = 64;
		else
			*hgh_alpha += nTmp;

		if ((dnlp_printk>>17)&0x1)
			pr_dnlp_dbg("Step C-1.y.1: out: hgh alpha++ (%3d) -> %2d\n",
				nTmp, *hgh_alpha);
	}
}


static void dnlp_tgt_sort(void)
{
	int i = 0;
	int j = 0;
	unsigned char t = 0;
	int chk = 0;
	/* unsigned char ve_dnlp_tgt[64]; */
	for (j = 0; j < 63; j++) {
		chk = 0;
		for (i = 0; i < (63 - i); i++) {
			if (ve_dnlp_tgt[i] > ve_dnlp_tgt[i+1]) {
				t = ve_dnlp_tgt[i];
				ve_dnlp_tgt[i] = ve_dnlp_tgt[i+1];
				ve_dnlp_tgt[i+1] = t;
				chk = chk+1;
			}
		}
		if (chk == 0)
			break;
	}
}


/*  in: vf (hist), h_sel*/
/*   out: hstSum,pre_0_gamma, *osamebin_num*/
/*return: hstSum  */
static int load_histogram(int *osamebin_num, struct vframe_s *vf,
	int h_sel, unsigned int nTstCnt)
{
	struct vframe_prop_s *p = &vf->prop;
	int nT0;   /* counter number of bins same to last frame */
	int hstSum, nT1, i, j; /* sum of pixels in histogram */

	nT0 = 0;	hstSum = 0;
	for (i = 0; i < 64; i++) {
		/* histogram stored for one frame delay */
		pre_1_gamma[i] = pre_0_gamma[i];
		if (h_sel)
			pre_0_gamma[i] = (unsigned int)p->hist.vpp_gamma[i];
		else
			pre_0_gamma[i] = (unsigned int)p->hist.gamma[i];

		/* counter the same histogram */
		if (pre_1_gamma[i] == pre_0_gamma[i])
			nT0++;
		else if (pre_1_gamma[i] > pre_0_gamma[i])
			nT1 = (pre_1_gamma[i] - pre_0_gamma[i]);
		else
			nT1 = (pre_0_gamma[i] - pre_1_gamma[i]); /* not use */

		hstSum += pre_0_gamma[i];
	}

	if (dnlp_printk)
		pr_dnlp_dbg("\nRflsh%03d: %02d same bins hstSum(%d)\n",
			nTstCnt, nT0, hstSum);

	/* output, same hist bin nums of this frame */
	*osamebin_num = nT0;

	if (dnlp_printk) {
		pr_dnlp_dbg("\n ==nTstCnt= %d====\n\n", nTstCnt);
			pr_dnlp_dbg("\n#### load_histogram()-1: [\n");
			for (j = 0; j < 4; j++) {
				i = j*16;
				pr_dnlp_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
					pre_0_gamma[i], pre_0_gamma[i+1],
					pre_0_gamma[i+2], pre_0_gamma[i+3],
					pre_0_gamma[i+4], pre_0_gamma[i+5],
					pre_0_gamma[i+6], pre_0_gamma[i+7],
					pre_0_gamma[i+8], pre_0_gamma[i+9],
					pre_0_gamma[i+10], pre_0_gamma[i+11],
					pre_0_gamma[i+12], pre_0_gamma[i+13],
					pre_0_gamma[i+14], pre_0_gamma[i+15]);
			}
			pr_dnlp_dbg("  ]\n");
		pr_dnlp_dbg("### ===== hstSum = %d ======\n", hstSum);
		pr_dnlp_dbg("### ve_dnlp_luma_sum(raw) = %d\n",
			ve_dnlp_luma_sum);
	}

	return hstSum;
}


/* input:  pre_0_gamma[] (iir filtered hist), raw_hst_sum;*/
/*  output:  iir_hstSum,  ihstBgn, ihstEnd, iRgnBgn, iRgnEnd */
int get_hist_bgn_end(unsigned int *iRgnBgn, unsigned int *iRgnEnd,
	unsigned int *ihstBgn, unsigned int *ihstEnd,
	unsigned int ihst_sum)
{
	unsigned int i, iir_hstSum;
	unsigned int hstBgn, hstEnd;

	/* detect the hstBgn and hstEnd */
	iir_hstSum = 0; hstBgn = 0; hstEnd = 0;
	for (i = 0; i < 64; i++) {
		if (pre_0_gamma[i] >= (ihst_sum>>8)) {
			if (hstBgn == 0 && pre_0_gamma[0] <= (ihst_sum>>9))
				hstBgn = i;
		}
		if (pre_0_gamma[i] >= (ihst_sum>>8)) {
			if (hstEnd != 64)
				hstEnd = i+1;
		}
		clash_curve[i] = (i<<4); /* 0~1024 */

		iir_hstSum += pre_0_gamma[i];
	}
	clash_curve[64] = 1024;

	if (dnlp_alg_param.dnlp_limit_rng) {
		/* i=ihstBgn, i<ihstEnd */
		*iRgnBgn = 4; /* 4 */
		*iRgnEnd = 59;/* 59 */
	} else {
		*iRgnBgn = 0;
		*iRgnEnd = 64;
	}

	if (dnlp_alg_param.dnlp_range_det) {
		if (hstBgn <= 4)
			*iRgnBgn = hstBgn;
		if (hstEnd >= 59)
			*iRgnEnd = hstEnd;
	}

	/* outputs */
	*ihstBgn = hstBgn;	*ihstEnd = hstEnd;
	return iir_hstSum;
}


/*input:  pre_0_gamma[] (iir filtered hist),iRgnBgn,iRgnEnd,ihstBgn,ihstEnd*/
/*  output: mMaxLst[], mMaxIdx[], *low_lsum, *low_bsum,*/
/*  *rgn_hstSum, *rgn_hstMax, *lsft_avg, *luma_avg4, *low_lavg4,*/
/*max4_sum  */
int get_hist_max4_avgs(unsigned int *mMaxLst, unsigned int *mMaxIdx,
	unsigned int *low_lsum, unsigned int *low_bsum,
	unsigned int *rgn_lumSum, unsigned int *rgn_hstSum,
	unsigned int *rgn_hstMax, unsigned int *lsft_avg,
	unsigned int *luma_avg4, unsigned int *low_lavg4,
	unsigned int *max4_sum, unsigned int iRgnBgn, unsigned int iRgnEnd,
	unsigned int ihstBgn, unsigned int ihstEnd)
{
	int i, nT0, nT1;
	unsigned int nTmp, nTmp0, sum, max, luma_sum;

	mMaxLst[0] = mMaxLst[1] = mMaxLst[2] = mMaxLst[3] = 0;
	mMaxIdx[0] = mMaxIdx[1] = mMaxIdx[2] = mMaxIdx[3] = 0;
	nT0 = 0;  sum = max = luma_sum = 0;
	for (i = iRgnBgn; i < iRgnEnd; i++) {
		nTmp = pre_0_gamma[i];
		sum += nTmp;

		if (max < nTmp)
			max = nTmp;

		/*lower extension [0-63]*/
		nTmp0 = nTmp*i;
		luma_sum += nTmp0;
		if (i == 31) {
			*low_lsum = luma_sum;/*low luma sum, sum of lumas*/
			*low_bsum = sum;/*low bin sum, num of pixels*/
		}

		/*Get the maximum4*/
		for (nT0 = 0; nT0 < 4; nT0++) {
			if (nTmp >= mMaxLst[nT0]) {
				for (nT1 = 3; nT1 >= nT0+1; nT1--) {
					mMaxLst[nT1] = mMaxLst[nT1-1];
					mMaxIdx[nT1] = mMaxIdx[nT1-1];
				}
				mMaxLst[nT0] = nTmp;
				mMaxIdx[nT0] = i;
				break;
			}
		}

		if ((dnlp_printk>>16) & 0x1)
			pr_dnlp_dbg("#(get_hist_max4_avgs)-1:i=%2d, luma_sum=%6d, sum=%6d, low_lsum=%6d, low_lsum=%6d\n",
				i, luma_sum, sum, *low_lsum, *low_bsum);

	}



	/* Filter to get the max4 bin */
	if (mMaxIdx[0] == 0)
		nTmp = (mMaxIdx[1] * 2) + mMaxIdx[2] + mMaxIdx[3];
	else {
		if (mMaxIdx[1] == 0)
			nTmp = mMaxIdx[0] + mMaxIdx[2] + mMaxIdx[3];
		else if (mMaxIdx[2] == 0)
			nTmp = mMaxIdx[0] + mMaxIdx[1] + mMaxIdx[3];
		else if (mMaxIdx[3] == 0)
			nTmp = mMaxIdx[0] + mMaxIdx[1] + mMaxIdx[2];
		else
			nTmp = mMaxIdx[1] + mMaxIdx[2] + mMaxIdx[3];

		nTmp += mMaxIdx[0];
	}
	/* outputs */
	*max4_sum = nTmp;  /* maximum 4 bin's idx sum */

	/* outputs */
	*rgn_lumSum = luma_sum;/* [iRgnBgn, iRgnEnd],not use ihstRgn data */
	*rgn_hstSum = sum;
	*rgn_hstMax = max;
	/*invalid histgram: freeze dnlp curve*/
	if ((*rgn_hstMax <= 55 || *rgn_hstSum == 0)
		&& (!dnlp_alg_param.dnlp_respond_flag)) {
		if (dnlp_printk)
			pr_dnlp_dbg("WARNING: invalid hist @ step 0.6: [%d %d %d]\n",
			(*rgn_hstMax), (*rgn_hstSum),
			dnlp_alg_param.dnlp_respond_flag);
		return -1;
	}

	/* get the averages and output */
	nTmp = (luma_sum << (2 + dnlp_alg_param.dnlp_pavg_btsft));
	/* with (2 + ve_dnlp_pavg_btsft) precision */
	*lsft_avg = (nTmp + (sum >> 1)) / (sum+1);
	*luma_avg4 = ((*lsft_avg) >> dnlp_alg_param.dnlp_pavg_btsft);
	/* low range hist average binx4, equivalent to lmh_avg[0]???? */
	*low_lavg4 = 4*(*low_lsum)/((*low_bsum)+1);
	if ((dnlp_printk>>16) & 0x1) {
		pr_dnlp_dbg("#(get_hist_max4_avgs)-2:luma_sum(shft)=%d, btsft=%d, lsft_avg=%d, luma_avg4=%d; low_lavg4=4*(%d by %d)= %d",
			nTmp, dnlp_alg_param.dnlp_pavg_btsft,  (*lsft_avg),
			(*luma_avg4),  (*low_lsum),
			(*low_bsum), (*low_lavg4));
	}
	return 0;
}



/*function: calc ihst variance */
/*in		: pre_0_gamma[i], rgn_hstSum,luma_avg4,iRgnBgn,iRgnEnd*/
/*out	: var*/

unsigned int cal_hist_var(unsigned int rgn_hstSum, unsigned int luma_avg4,
	unsigned int iRgnBgn, unsigned int iRgnEnd)
{
	unsigned int i, dif, dif_sum, i4;
	unsigned int var;

	dif_sum = 0;
	for (i = iRgnBgn; i < iRgnEnd; i++) {
		i4 = (i << 2);
		/* luma_avg4:0~256 */
		dif =  i4 > luma_avg4 ? (i4 - luma_avg4):(luma_avg4 - i4);
		dif = (dif*dif) >> 4;
		if ((dnlp_printk >> 13) & 0x1)
			pr_dnlp_dbg("@@@cal_hist_var():i4 = %d, luma_avg4 = %d, dif*dif = %4d,	",
				i4, luma_avg4, dif);

		dif = dif * pre_0_gamma[i];/* num of i */
		dif_sum += dif;

		if ((dnlp_printk >> 13) & 0x1)
			pr_dnlp_dbg("cal_hist_var():pre_0_gamma[i] = %4d, dif_sum = %4d\n ",
				pre_0_gamma[i], dif_sum);
	}
	var = dif_sum/((rgn_hstSum>>3)+1);/* mse */

	if ((dnlp_printk >> 13) & 0x1)
		pr_dnlp_dbg("@@@cal_hist_var():  var = %d(before clip)\n ",
			var);

	/* var = (var>2048)? 2048: var; */
	var = (var > 8192) ? 8192 : var;
	/* typical histo <==> var	<==> var/8 */
	/* 1-bin 0 0 */
	/* 3-bin 8 1 */
	/* 5-bin 16 2 */
	/* 7-bin 32 4 */
	/* 9-bin 52 7 */
	/* 11-bin 83 11 */
	/* 13-bin 112 15 */
	/* 16-bin 204 25 */
	/* 32-bin 748 93.5 */
	/* ramp 2500 325 */
	/* BW50% xx 1024 */

	if (dnlp_printk)
		pr_dnlp_dbg("+++ cal_hist_var(): iRgn=[%d %d], luma_avg4= %d, rgn_hstSum = %d,=> var = %d(u8 %d)\n ",
		iRgnBgn, iRgnEnd, luma_avg4, rgn_hstSum, var, var/8);

	return var;

}


/* input:  luma_avg (0~63)*/
 /* output: *auto_rng, *lowrange, *hghrange  */
void get_auto_range(unsigned int *auto_rng, unsigned int *lowrange,
	unsigned int *hghrange, unsigned int luma_avg)
{
	unsigned int dnlp_auto_rng, dnlp_lowrange, dnlp_hghrange;

	/* auto_range */
	if (luma_avg < dnlp_alg_param.dnlp_auto_rng)
		dnlp_auto_rng = luma_avg;
	else if (luma_avg + dnlp_alg_param.dnlp_auto_rng > 64)
		dnlp_auto_rng = 64 - luma_avg;
	else
		dnlp_auto_rng = dnlp_alg_param.dnlp_auto_rng;
	if (dnlp_auto_rng < 2)
		dnlp_auto_rng = 2;
	else if (dnlp_auto_rng > 10)
		dnlp_auto_rng = 10;

	/* lowrange and hghrange calcluation */
	if (dnlp_alg_param.dnlp_auto_rng > 0) {
		if (luma_avg <= dnlp_auto_rng + 2) {
			dnlp_lowrange = 2;
			dnlp_hghrange = 64 - (luma_avg + dnlp_auto_rng);
		} else if (luma_avg >= 61 - dnlp_auto_rng) {
			dnlp_lowrange = luma_avg - dnlp_auto_rng;
			dnlp_hghrange = 2;
		} else {
			dnlp_lowrange = luma_avg - dnlp_auto_rng;
			dnlp_hghrange = (63 - (luma_avg + dnlp_auto_rng));
		}
	} else {
		dnlp_lowrange = dnlp_alg_param.dnlp_lowrange;
		dnlp_hghrange = dnlp_alg_param.dnlp_hghrange;
	}
	if (dnlp_lowrange > 31)
		dnlp_lowrange = 31;
	else if (dnlp_lowrange < 2)
		dnlp_lowrange = 2;
	if (dnlp_hghrange > 31)
		dnlp_hghrange = 31;
	else if (dnlp_hghrange < 2)
		dnlp_hghrange = 2;

	/* output */
	*auto_rng = dnlp_auto_rng;
	*lowrange = dnlp_lowrange;
	*hghrange = dnlp_hghrange;
}


/*function : calc for lmh_avg[5], further use in cal_bwext_param(),*/
 /*dnlp_adap_alpharate()*/
 /*input:  pre_0_gamma (iir filtered version), dnlp_lowrange,*/
/* dnlp_hghrange, *iRgnBgn, iRgnEnd */
 /*output: lmh_sum[3](not use), lmh_avg[5]*/
void get_lmh_avg(unsigned int *lmh_sum, unsigned int *lmh_avg,
	unsigned int dnlp_lowrange, unsigned int dnlp_hghrange,
	unsigned int iRgnBgn, unsigned int iRgnEnd)
{
	unsigned int i, nTmp0;

	lmh_sum[0] = lmh_sum[1] = lmh_sum[2] = 0;  /* jas_debug */
	for (i = iRgnBgn; i < iRgnEnd; i++) {
		/* use iRgn([4,59]) ,not ihstRgn */
		nTmp0 = pre_0_gamma[i] * i;

		if (i < dnlp_lowrange) {/* low tone */
			lmh_sum[0] += pre_0_gamma[i];
			lmh_avg[0] += nTmp0;
			lmh_avg[3] += pre_0_gamma[i] * (64 - i);
		} else if (i > (63 - dnlp_hghrange)) {	/* hgh tone */
			lmh_sum[2] += pre_0_gamma[i];
			lmh_avg[2] += nTmp0;
		} else{ /* mid tone */
			lmh_sum[1] += pre_0_gamma[i];
			lmh_avg[1] += nTmp0;
		}
	}

	/* low/mid/high tone average */
	/* low tone avg in bins (0~63) */
	lmh_avg[0] = (lmh_avg[0] + (lmh_sum[0] >> 1)) /
		(lmh_sum[0] + 1);
	lmh_avg[3] = (lmh_avg[3] + (lmh_sum[0] >> 1)) /
		(lmh_sum[0] + 1);
	/* mid tone avg in bins (0~63) */
	lmh_avg[1] = (lmh_avg[1] + (lmh_sum[1] >> 1)) /
		(lmh_sum[1] + 1);
	/* hig tone avg in bins (0~63) */
	lmh_avg[2] = (lmh_avg[2] + (lmh_sum[2] >> 1)) /
		(lmh_sum[2] + 1);
}

/*	function:  get the blend coef between blk_gmma and clsh_scvbld */
/*		in: *dnlp_pstgma_ratio, dnlp_pst_gmarat */
/*(return)output: blk_gma_rat[0~63],  */



/* dnlp saturation compensations */
/* input: ve_dnlp_tgt[]; */
/* output: ve_dnlp_add_cm(nTmp + 512), and delta saturation; */
int dnlp_sat_compensation(void)
{
	int nT0, nT1, nTmp0, nTmp, i;
	unsigned int pre_stur = 0;

	nT0 = 0;	nT1 = 0;
	for (i = 1; i < 64; i++) {
		if (ve_dnlp_tgt[i] > 4*i) {
			nT0 += (ve_dnlp_tgt[i] - 4*i) * (65 - i);
			nT1 += (65 - i);
		}
	}
	nTmp0 = nT0 * dnlp_alg_param.dnlp_satur_rat + (nT1 >> 1);
	nTmp0 = nTmp0 / (nT1 + 1);
	nTmp0 = ((nTmp0 + 4) >> 3);

	nTmp =	(dnlp_alg_param.dnlp_satur_max << 3);
	if (nTmp0 < nTmp)
		nTmp = nTmp0;

	if (((dnlp_printk>>11) & 0x1))
		pr_dnlp_dbg("#sat_comp: pre(%3d) => %5d / %3d => %3d cur(%3d)\n",
			pre_stur, nT0, nT1, nTmp0, nTmp);

	if (dnlp_alg_param.dnlp_set_saturtn == 0) {
		if (nTmp != pre_stur) {
			ve_dnlp_add_cm(nTmp + 512);
			pre_stur = nTmp;
		}
	} else {
		if (pre_stur != dnlp_alg_param.dnlp_set_saturtn) {
			if (dnlp_alg_param.dnlp_set_saturtn < 512)
				ve_dnlp_add_cm(dnlp_alg_param.dnlp_set_saturtn +
					512);
			else
				ve_dnlp_add_cm(dnlp_alg_param.dnlp_set_saturtn);
			pre_stur = dnlp_alg_param.dnlp_set_saturtn;
		}
	}

	return	nTmp;
}


/* function: final selection of the curves */
/* input: ve_dnlp_tgt[], ve_dnlp_final_gain */
/* output: ve_dnlp_tgt[], premap0[] */
void curv_selection(void)
{
	int i, nTmp0;
	static unsigned int pst_0_gamma[65] = {
				 0, 4, 8, 12, 16, 20, 24, 28, 32, 36,
				40, 44, 48, 52, 56, 60, 64, 68, 72, 76,
				80, 84, 88, 92, 96, 100, 104,
				108, 112, 116, 120, 124, 128,
				132, 136, 140, 144, 148, 152,
				156, 160, 164, 168, 172, 176,
				180, 184, 188, 192, 196, 200,
				204, 208, 212, 216, 220, 224,
				228, 232, 236, 240, 244, 248,
				252, 255
			};

	if (!ve_dnlp_luma_sum) {
		for (i = 0; i < 65; i++)
			pst_0_gamma[i] = (i << 4);
	}

	for (i = 0; i < 65; i++) {
		premap0[i] = ve_dnlp_tgt[i];

		if (dnlp_alg_param.dnlp_dbg_map == 1)
			nTmp0 = gma_scurve0[i];
		else if (dnlp_alg_param.dnlp_dbg_map == 2)
			nTmp0 = gma_scurve1[i];
		else if (dnlp_alg_param.dnlp_dbg_map == 3)
			nTmp0 = gma_scurvet[i];
		else if (dnlp_alg_param.dnlp_dbg_map == 4)
			nTmp0 = clash_curve[i];
		else if (dnlp_alg_param.dnlp_dbg_map == 5)
			nTmp0 = clsh_scvbld[i];
		else if (dnlp_alg_param.dnlp_dbg_map == 6)
			nTmp0 = blk_gma_crv[i];
		else if (dnlp_alg_param.dnlp_dbg_map == 7)
			nTmp0 = blk_gma_bld[i];
		else if (dnlp_alg_param.dnlp_dbg_map == 8)
			nTmp0 = (i<<4); /* 45 degree */
		else if (dnlp_alg_param.dnlp_dbg_map == 9)
			nTmp0 = blkwht_ebld[i]; /* 1023 */
		else if (dnlp_alg_param.dnlp_dbg_map == 10)
			nTmp0 = GmScurve[i];
		else
			nTmp0 = blkwht_ebld[i];

		/* add a gain here to finally change the DCE strength:*/
		/*  final_gain normalized to 8 as  1 */
		nTmp0 = ((i)<<4) +
			(((nTmp0-((i)<<4)) *
				((int)dnlp_alg_param.dnlp_final_gain) + 4)/8);

		nTmp0 = ((nTmp0 + 2) >> 2);

		if (nTmp0 > 255)
			nTmp0 = 255;
		else if (nTmp0 < 0)
			nTmp0 = 0;

		if ((dnlp_printk >> 10) & 0x1) {
			pr_dnlp_dbg("curv_sel[%02d]:(var_s)%4d, (c)%4d, (cs)%4d, (blk_gma)%4d, (blk_bld)%4d, (blkwht_ebld)%4d	=> (final) %3d\n",
				i, GmScurve[i], clash_curve[i],
				clsh_scvbld[i], blk_gma_crv[i],
				blk_gma_bld[i], blkwht_ebld[i], nTmp0);
		}

		ve_dnlp_tgt[i] = nTmp0;
	}
	if ((dnlp_printk >> 10) & 0x1)
		pr_dnlp_dbg("## final iir param:dnlp_scn_chg=%d, menu_chg_en =%d,dnlp_alg_param.dnlp_dbg_i2r = %d\n",
			dnlp_scn_chg, menu_chg_en, dnlp_alg_param.dnlp_dbg_i2r);

	if (((!dnlp_scn_chg) && (!menu_chg_en)) &&
		((dnlp_alg_param.dnlp_dbg_i2r >> 4) & 0x1)) {
		for (i = 0; i < 65; i++) {
			nTmp0 = dnlp_bld_lvl * ve_dnlp_tgt[i] +
				(RBASE >> 1);/*u8*/
			nTmp0 = nTmp0 + (RBASE - dnlp_bld_lvl) * pst_0_gamma[i];
			nTmp0 = (nTmp0 >> dnlp_alg_param.dnlp_mvreflsh);

	if ((dnlp_printk >> 10) & 0x1)
		pr_dnlp_dbg("### final curv(iir):[%d],ve_dnlp_tgt:%d, pst_0_gamma:%d,=> O_curv:%d (%d/%d,%d)\n ",
			i, ve_dnlp_tgt[i], pst_0_gamma[i], nTmp0, dnlp_bld_lvl,
			RBASE, dnlp_alg_param.dnlp_mvreflsh);

			/*clip*/
			if (nTmp0 < 0)
				nTmp0 = 0;
			else if (nTmp0 > 255)
				nTmp0 = 255;

			ve_dnlp_tgt[i] = nTmp0;
		}

	}

	if (menu_chg_en == 1)
		menu_chg_en = 0;

	for (i = 0; i < 65; i++)
		pst_0_gamma[i] = ve_dnlp_tgt[i];
}

/* for draw curve */


void ve_dnlp_calculate_tgtx_v3(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;

	/* curve blending coef parms */
	/* 1- scurve0 + scurve1 = gma_scurvet : no use anymaore.180227*/
	unsigned int gmma_rate = (unsigned int)ve_dnlp_gmma_rate;/*no use*/
	/* gma_scurvet with 45 dgree in low luma */
	unsigned int low_alpha = (unsigned int)ve_dnlp_lowalpha_v3;
	/* gma_scurvet with 45 dgree in high luma */
	unsigned int hgh_alpha  = (unsigned int)ve_dnlp_hghalpha_v3;


	/* 2- gma_scurvet + clahe_curve = clsh_scvbld */
	unsigned int mtdbld_rate  =
		(unsigned int)dnlp_alg_param.dnlp_mtdbld_rate;

	/* clash, s curve  begin & end */
	unsigned int clashBgn = (unsigned int)dnlp_alg_param.dnlp_clashBgn;
	unsigned int clashEnd = (unsigned int)dnlp_alg_param.dnlp_clashEnd;
	unsigned int sBgnBnd = (unsigned int)dnlp_alg_param.dnlp_sbgnbnd;
	unsigned int sEndBnd = (unsigned int)dnlp_alg_param.dnlp_sendbnd;

	unsigned int ihstBgn, ihstEnd;	/* hist begin,end */

	/* hist auto range parms */
	unsigned int dnlp_lowrange = (unsigned int)dnlp_alg_param.dnlp_lowrange;
	unsigned int dnlp_hghrange = (unsigned int)dnlp_alg_param.dnlp_hghrange;
	unsigned int dnlp_auto_rng = 0;

	/*static unsigned int nTstCnt;*/

	/*---- some intermediate variables ----*/
	unsigned int raw_hst_sum, iir_hstSum;
	int i, smbin_num, nTmp0, dnlp_brightness, var, sat_compens;

	/* get_hist_max4_avgs () */
	/* only used in get_hist_max4_avgs */
	unsigned int rgn_lumSum, rgn_hstSum, rgn_hstMax;
	unsigned int low_sum = 0;
	unsigned int low_lsum = 0;

	unsigned int lsft_avg = 0; /* luma shift average */
	/* luma average(64),luma average(256) */
	unsigned int luma_avg, luma_avg4;
	unsigned int low_lavg4 = 0; /* low luma average */

	unsigned int mMaxLst[4]; /* patch for black+white stripe */
	unsigned int mMaxIdx[4];

	/* get_lmh_avg () */
	unsigned int lmh_avg[5] = {0, 0, 0, 0, 0};
	/* low mid hig range hist_sum for lmh_avg calculation */
	unsigned int lmh_sum[3] = {0, 0, 0};

	/* get_clahe_curve() */
	unsigned int cluma_avg4;	/* clahe luma avg4, not use */

	/* black white extension params */
	int blk_wht_ext[2] = {0, 0};   /* u10 precision, */

	/* pre-define gamma curve,1.2 & 1.8 no use anymaore.180227 */
	unsigned int rGm1p2[] = {
		0,	2,	4,	7,	9, 12, 15, 18, 21, 24,
		28, 31, 34, 38, 41, 45, 49, 52, 56, 60,
		63, 67, 71, 75, 79, 83, 87, 91, 95, 99,
		103, 107, 111, 116, 120, 124, 128, 133,
		137, 141, 146, 150, 154, 159, 163, 168,
		172, 177, 181, 186, 190, 195, 200, 204,
		209, 213, 218, 223, 227, 232, 237, 242,
		246, 251, 255};
	/* 2.0 for full range */
	unsigned int rGm1p8[] = {
		0,	0,	0,	1,	1,	2,	2, 3,  4,  5,
		6,	8,	9, 11, 12, 14, 16, 18, 20, 23,
		25, 28, 30, 33, 36, 39, 42, 46, 49, 53,
		56, 60, 64, 68, 72, 77, 81, 86, 90, 95,
		100, 105, 110, 116, 121, 127, 132, 138,
		144, 150, 156, 163, 169, 176, 182, 189,
		196, 203, 210, 218, 225, 233, 240, 248, 255};


	/* -------some pre-processing--------------------------- */

	/* calc iir-coef params */
	if (dnlp_alg_param.dnlp_mvreflsh < 1)
		dnlp_alg_param.dnlp_mvreflsh = 1;
	RBASE = (1 << dnlp_alg_param.dnlp_mvreflsh);

	/* parameters refresh */
	dnlp3_param_refrsh();
	dnlp_scn_chg = 0;

	/* initalizations */
	if (low_alpha > 64)
		low_alpha = 64;
	if (hgh_alpha > 64)
		hgh_alpha = 64;
	if (clashBgn > 16)
		clashBgn  = 16;
	if (clashEnd > 64)
		clashEnd  = 64;
	if (clashEnd < 49)
		clashEnd  = 49;

	/* old historic luma sum*/
	/* v3 historic luma sum */
	if (hist_sel)
		ve_dnlp_luma_sum = p->hist.vpp_luma_sum;
	else
		ve_dnlp_luma_sum = p->hist.luma_sum;

	/* counter the calling function  */
	nTstCnt++;
	if (nTstCnt > 16384)
		nTstCnt = 0;


	/*--------------------------------------------------------*/


	/*------  STEP A: load histogram and do histogram */
	/*  -pre-processing and detections ---*/
	/*step 0.0 load the histogram*/
	/* nTstCnt not use */
	raw_hst_sum = load_histogram(&smbin_num, vf, hist_sel, nTstCnt);

	/*step 0.01 all the same histogram as last frame, freeze DNLP */
	if (smbin_num == 64 &&
		dnlp_alg_param.dnlp_smhist_ck &&
		(!dnlp_alg_param.dnlp_respond_flag)) {
		if (dnlp_printk & 0x1)
			pr_dnlp_dbg("WARNING: invalid hist @ step 0.10\n");
		return;
	}

	/* step 0.02 v3 luma sum is 0,something is wrong,freeze dnlp curve*/
	if (!ve_dnlp_luma_sum) {
		dnlp_scn_chg = 1;
		if (dnlp_printk & 0x1)
			pr_dnlp_dbg("WARNING: invalid hist @ step 0.11\n");
		return;
	}

	/*step 0.1 calc T-IIR blending coef base on historic avg_luma change */
	dnlp_bld_lvl = curve_rfrsh_chk_v2(raw_hst_sum, RBASE);
	if (dnlp_printk)
		pr_dnlp_dbg("+++ curve_rfrsh_chk(): dnlp_bld_lvl = %d,scn_chg = %d\n",
			dnlp_bld_lvl, dnlp_scn_chg);

	/*step 0.2 black bord detection,and histogram clipping*/
	dnlp_refine_bin0(raw_hst_sum);

	/*step 0.3 histogram and luma_sum IIR filters in time domain, */
	dnlp_inhist_tiir();

	/*step 0.4 get the hist begin and end bins */
	iir_hstSum = get_hist_bgn_end(&dnlp_alg_param.dnlp_iRgnBgn,
		&dnlp_alg_param.dnlp_iRgnEnd,
		&ihstBgn, &ihstEnd, raw_hst_sum);
	if ((dnlp_printk>>2) & 0x1)
		pr_dnlp_dbg("## hist_bgn_end(): iRgnBgn_end= [%d,%d],ihstBgn_end= [%d,%d]\n",
			dnlp_alg_param.dnlp_iRgnBgn,
			dnlp_alg_param.dnlp_iRgnEnd, ihstBgn, ihstEnd);

	/*step 0.5 Get the maximum 4 bins and averages of the histogram*/
	if (get_hist_max4_avgs(&mMaxLst[0],
		&mMaxIdx[0], &low_lsum, &low_sum,
		&rgn_lumSum, &rgn_hstSum, &rgn_hstMax,
		&lsft_avg, &luma_avg4, &low_lavg4, &lmh_avg[4],
		dnlp_alg_param.dnlp_iRgnBgn,
		dnlp_alg_param.dnlp_iRgnEnd,
		ihstBgn, ihstEnd) < 0)
		return; /* rgn_vehstSum ? */

	luma_avg  = (luma_avg4 >> 2);
	if (dnlp_printk)
		pr_dnlp_dbg("+++ max4_avgs()-1:rgn_lumSum(ve_dnlp_luma_sum)= %d(%d),luma_avg4(raw) = %d\n",
		rgn_lumSum, ve_dnlp_luma_sum, luma_avg4);

	if ((dnlp_printk>>2) & 0x1)
		pr_dnlp_dbg("## get_hist_max4_avgs():lsft_avg=%d luma_avg=%d (%d), low_lavg4=%d,BE[%d %d %d %d], max4[%d %d %d %d][%d %d %d %d],rgn[%d %d %d], low<%d %d>\n",
			lsft_avg, luma_avg, luma_avg4, low_lavg4,
			dnlp_alg_param.dnlp_iRgnBgn,
			dnlp_alg_param.dnlp_iRgnEnd,
			ihstBgn, ihstEnd,
			mMaxLst[0], mMaxLst[1], mMaxLst[2],
			mMaxLst[3], mMaxIdx[0], mMaxIdx[1],
			mMaxIdx[2], mMaxIdx[3], rgn_lumSum,
			rgn_hstSum, rgn_hstMax, low_lsum, low_sum);

	/*step 0.6 IIR filter of lsft_avg in time domain,(for getting s0,s1,*/
	/*	   no use anymaore. 180227)*/
	lsft_avg  = cal_hst_shft_avg(lsft_avg);
	if (ve_dnlp_scv_dbg != 0) {
		nTmp0 = lsft_avg + 16 * ve_dnlp_scv_dbg;
		lsft_avg = (nTmp0 < 0) ? 0 : nTmp0;
	}
	if ((dnlp_printk>>2) & 0x1)
		pr_dnlp_dbg("after cal_hst_shft_avg(): lsft_avg = %d\n",
			lsft_avg);

	/*step 0.7 calclulate brightness  based on luma_avg4 and low_lavg4*/
	dnlp_brightness = cal_brght_plus(luma_avg4, low_lavg4);
	if ((dnlp_printk>>2) & 0x1) {
		pr_dnlp_dbg("cal_brght_plus():luma_avg4=%02d, low_lavg4=%4d,brightness=%d,\n",
			luma_avg4, low_lavg4, dnlp_brightness);
	}

	/*step 0.8 150918 for 32-step luma pattern, not work180227*/
	if (dnlp_alg_param.dnlp_dbg_adjavg)
		luma_avg4 = AdjHistAvg(luma_avg4, ihstEnd);

	/*step 0.9 do IIR filter of luma_avg4 on time domain*/
	luma_avg4 = cal_hist_avg(luma_avg4);  /* IIR filtered in time domain */

	/*for read in tool*/
	ro_luma_avg4 = luma_avg4;
	luma_avg = (luma_avg4>>2);
	if (dnlp_printk)
		pr_dnlp_dbg("+++ cal_hist_avg(iir)-2: luma_avg4 = %d(iir)\n",
		luma_avg4);

	/*step 0.a cal hist variance */
	var = cal_hist_var(rgn_hstSum, luma_avg4,
		dnlp_alg_param.dnlp_iRgnBgn, dnlp_alg_param.dnlp_iRgnEnd);

	/*step 0.b auto range detection and lmh_avg calc for further steps*/
	get_auto_range(&dnlp_auto_rng, &dnlp_lowrange,
		&dnlp_hghrange, luma_avg);
	get_lmh_avg(&lmh_sum[0], &lmh_avg[0], dnlp_lowrange,
		dnlp_hghrange, dnlp_alg_param.dnlp_iRgnBgn,
		dnlp_alg_param.dnlp_iRgnEnd);
	if ((dnlp_printk >> 2) & 0x1) {
		pr_dnlp_dbg("## get_lmh_avg():auto_rgn=%d, lowrange=%d, hghrange=%d,lmh_sum=[%d %d %d], lmh_avg=[%d %d %d %d %d]\n",
			dnlp_auto_rng, dnlp_lowrange, dnlp_hghrange,
			lmh_sum[0], lmh_sum[1], lmh_sum[2], lmh_avg[0],
			lmh_avg[1], lmh_avg[2], lmh_avg[3], lmh_avg[4]);
	}


	/* -- Step B: prepare different curves---  */

	/*S curve method 1,old algorithm,no use anymaore. 180227*/
	GetGmCurves(gma_scurve0, rGm1p2, lsft_avg, sBgnBnd, sEndBnd);
	GetGmCurves(gma_scurve1, rGm1p8, lsft_avg, sBgnBnd, sEndBnd);

	/*Step 1.0 S curve method 2*/
	GetGmScurve_apl_var(GmScurve, luma_avg4, var, sBgnBnd, sEndBnd);

	/* Step 1.1: CLAHE curve calculations */
	if (clahe_method == 0)
		get_clahe_curve(clash_curve, &cluma_avg4, pre_0_gamma, var,
		clashBgn, clashEnd);
	else if (clahe_method == 1)
		get_hist_cross_curve(clash_curve, pre_0_gamma, iir_hstSum,
			clashBgn, clashEnd);

	clahe_tiir();	/* iir filter on time domain */

	/* Step 1.2: BWext parameters calc ,u10 */
	cal_bwext_param(blk_wht_ext, ihstBgn, ihstEnd, rgn_hstSum, lmh_avg);
	if (dnlp_printk)
		pr_dnlp_dbg("++:luma_avg4=%d,blkwht_ext[2]=[%d,%d],lmh_avg[L H]=[%d,%d]\n",
			luma_avg4, blk_wht_ext[0], blk_wht_ext[1],
			lmh_avg[0], lmh_avg[2]);


	/* --- Step C: do the blending coefs calculations -----  */
	/* Step 2.0.0  blending rate betwen gama_scurve and CLAHE*/
	mtdbld_rate = dnlp_adp_mtdrate(mtdbld_rate, luma_avg);
	if (dnlp_printk)
		pr_dnlp_dbg("+++ dnlp_adp_mtdrate()-1: mtdbld_rate = %d\n",
			mtdbld_rate);

	/*step 2.1. blending rate of s0,s1  no use anymaore.180227*/
	dnlp_adp_alpharate(lmh_avg, &low_alpha, &hgh_alpha,
	dnlp_lowrange, dnlp_hghrange);

	/*step 2.0.1.: patch for black+white stripe, mtdbld_rate*/
	patch_blk_wht(&mMaxLst[0], &mMaxIdx[0], &gmma_rate,
		&low_alpha, &hgh_alpha, &mtdbld_rate, iir_hstSum);
	if (dnlp_printk)
		pr_dnlp_dbg("+++ patch_blk_wht()-2: mtdbld_rate = %d\n",
			mtdbld_rate);

	/*step 2.0.2:post processing ofthe coefs based on the luma_avgs*/
	dnlp_params_hist(&gmma_rate, &low_alpha, &hgh_alpha,
				  &mtdbld_rate, luma_avg, luma_avg4);
	if (dnlp_printk)
		pr_dnlp_dbg("+++ dnlp_params_hist()-3: mtdbld_rate = %d\n",
			mtdbld_rate);


	/* -------- Step D: Curves Blendings ------  */
	/*old S curve, no use anymaore. 180227*/
	dnlp_gmma_cuvs(gmma_rate, low_alpha, hgh_alpha, lsft_avg);

	/*Step 3.0: clsh_scvbld = clash_curve + GmScurve */
	dnlp_clsh_sbld(mtdbld_rate);
	/*for read on tools*/
	ro_mtdbld_rate = mtdbld_rate;
	ro_blk_wht_ext0 = blk_wht_ext[0];
	ro_blk_wht_ext1 = blk_wht_ext[1];
	ro_dnlp_brightness = dnlp_brightness;

	/*step 3.1: blkwht_ebld = blk_gma_bld + extension */
	dnlp_blkwht_bld(blkwht_ebld, clsh_scvbld,
			blk_wht_ext, dnlp_brightness,
				luma_avg4, luma_avg,
				dnlp_alg_param.dnlp_iRgnBgn,
				dnlp_alg_param.dnlp_iRgnEnd);


	/*-- Step E: final curve selection, boosting and sorting Blendings---*/
	/*Step 4.0 selection of curve and apply the dnlp_final_gain*/
	curv_selection();
	/*Step 4.1: sort the curve to avoid luma invert, 0~255 sort */
	dnlp_tgt_sort();
	/*Step 4.2: do the saturation compensation based on the dnlp_curve */
	sat_compens = dnlp_sat_compensation();

	if (dnlp_printk) {
		pr_dnlp_dbg("#Dbg:[iRgnBgn < luma_avg(luma_avg4) < iRgnEnd] sat_compens=\n");
		pr_dnlp_dbg("#Dbg:[%02d	< %02d   (%03d)  < %02d]  sat_compens=%d\n",
			dnlp_alg_param.dnlp_iRgnBgn, luma_avg,
			luma_avg4, dnlp_alg_param.dnlp_iRgnEnd,
			sat_compens);
	}

	if (dnlp_alg_param.dnlp_dbg_map && ((dnlp_printk >> 12) & 0x1))
		for (i = 0; i < 64; i++)
			pr_dnlp_dbg("[index %02d] %5d(pre_0_gamma)=>%5d(ve_dnlp_tgt[])\n",
				i, pre_0_gamma[i], ve_dnlp_tgt[i]);

	/* print debug log once */
	if (ve_dnlp_ponce == 1 && dnlp_printk)
		dnlp_printk = 0;

	return;
}


 /* lpf[0] is always 0 & no need calculation */
void ve_dnlp_calculate_lpf(void)
{
	ulong i = 0;

	for (i = 0; i < 64; i++)
		ve_dnlp_lpf[i] = ve_dnlp_lpf[i] -
		(ve_dnlp_lpf[i] >> ve_dnlp_rt) + ve_dnlp_tgt[i];
}

void ve_dnlp_calculate_reg(void)
{
	ulong i = 0, j = 0, cur = 0, data = 0,
			offset = ve_dnlp_rt ? (1 << (ve_dnlp_rt - 1)) : 0;
	for (i = 0; i < 16; i++) {
		ve_dnlp_reg[i] = 0;
		cur = i << 2;
		for (j = 0; j < 4; j++) {
			data = (ve_dnlp_lpf[cur + j] + offset) >> ve_dnlp_rt;
			if (data > 255)
				data = 255;
			ve_dnlp_reg[i] |= data << (j << 3);
		}
	}
}

void ve_set_v3_dnlp(struct ve_dnlp_curve_param_s *p)
{
	ulong i = 0;
	/* get command parameters */
	/* general settings */
	if ((ve_en != p->param[ve_dnlp_enable]) ||
		(dnlp_sel != p->param[ve_dnlp_sel]) ||
		(dnlp_alg_param.dnlp_lowrange !=
			p->param[ve_dnlp_lowrange]) ||
		(dnlp_alg_param.dnlp_hghrange !=
			p->param[ve_dnlp_hghrange]) ||
		(dnlp_alg_param.dnlp_auto_rng !=
			p->param[ve_dnlp_auto_rng]) ||
		(dnlp_alg_param.dnlp_bbd_ratio_low !=
			p->param[ve_dnlp_bbd_ratio_low]) ||
		(dnlp_alg_param.dnlp_bbd_ratio_hig !=
			p->param[ve_dnlp_bbd_ratio_hig]) ||
		(dnlp_alg_param.dnlp_mvreflsh !=
			p->param[ve_dnlp_mvreflsh]) ||
		(dnlp_alg_param.dnlp_smhist_ck !=
			p->param[ve_dnlp_smhist_ck]) ||
		(dnlp_alg_param.dnlp_final_gain !=
			p->param[ve_dnlp_final_gain]) ||
		(dnlp_alg_param.dnlp_clahe_gain_neg !=
			p->param[ve_dnlp_clahe_gain_neg]) ||
		(dnlp_alg_param.dnlp_clahe_gain_pos !=
			p->param[ve_dnlp_clahe_gain_pos]) ||
		(dnlp_alg_param.dnlp_mtdbld_rate !=
			p->param[ve_dnlp_mtdbld_rate]) ||
		(dnlp_alg_param.dnlp_adpmtd_lbnd !=
			p->param[ve_dnlp_adpmtd_lbnd]) ||
		(dnlp_alg_param.dnlp_adpmtd_hbnd !=
			p->param[ve_dnlp_adpmtd_hbnd]) ||
		(dnlp_alg_param.dnlp_sbgnbnd !=
			p->param[ve_dnlp_sbgnbnd]) ||
		(dnlp_alg_param.dnlp_sendbnd !=
			p->param[ve_dnlp_sendbnd]) ||
		(dnlp_alg_param.dnlp_cliprate_v3 !=
			p->param[ve_dnlp_cliprate_v3]) ||
		(dnlp_alg_param.dnlp_cliprate_min !=
			p->param[ve_dnlp_cliprate_min]) ||
		(dnlp_alg_param.dnlp_adpcrat_lbnd !=
			p->param[ve_dnlp_adpcrat_lbnd]) ||
		(dnlp_alg_param.dnlp_adpcrat_hbnd !=
			p->param[ve_dnlp_adpcrat_hbnd]) ||
		(dnlp_alg_param.dnlp_clashBgn !=
			p->param[ve_dnlp_clashBgn]) ||
		(dnlp_alg_param.dnlp_clashEnd !=
			p->param[ve_dnlp_clashEnd]) ||
		(dnlp_alg_param.dnlp_blkext_rate !=
			p->param[ve_dnlp_blkext_rate]) ||
		(dnlp_alg_param.dnlp_whtext_rate !=
			p->param[ve_dnlp_whtext_rate]) ||
		(dnlp_alg_param.dnlp_blkext_ofst !=
			p->param[ve_dnlp_blkext_ofst]) ||
		(dnlp_alg_param.dnlp_whtext_ofst !=
			p->param[ve_dnlp_whtext_ofst]) ||
		(dnlp_alg_param.dnlp_bwext_div4x_min !=
			p->param[ve_dnlp_bwext_div4x_min]) ||
		(dnlp_alg_param.dnlp_blk_cctr !=
			p->param[ve_dnlp_blk_cctr]) ||
		(dnlp_alg_param.dnlp_brgt_ctrl !=
			p->param[ve_dnlp_brgt_ctrl]) ||
		(dnlp_alg_param.dnlp_brgt_range !=
			p->param[ve_dnlp_brgt_range]) ||
		(dnlp_alg_param.dnlp_brght_add !=
			p->param[ve_dnlp_brght_add]) ||
		(dnlp_alg_param.dnlp_brght_max !=
			p->param[ve_dnlp_brght_max]) ||
		(dnlp_alg_param.dnlp_satur_rat !=
			p->param[ve_dnlp_satur_rat]) ||
		(dnlp_alg_param.dnlp_satur_max !=
			p->param[ve_dnlp_satur_max]) ||
		(dnlp_alg_param.dnlp_scurv_low_th !=
			p->param[ve_dnlp_scurv_low_th]) ||
		(dnlp_alg_param.dnlp_scurv_mid1_th !=
			p->param[ve_dnlp_scurv_mid1_th]) ||
		(dnlp_alg_param.dnlp_scurv_mid2_th !=
			p->param[ve_dnlp_scurv_mid2_th]) ||
		(dnlp_alg_param.dnlp_scurv_hgh1_th !=
			p->param[ve_dnlp_scurv_hgh1_th]) ||
		(dnlp_alg_param.dnlp_scurv_hgh2_th !=
			p->param[ve_dnlp_scurv_hgh2_th]) ||
		(dnlp_alg_param.dnlp_mtdrate_adp_en !=
			p->param[ve_dnlp_mtdrate_adp_en]))
		menu_chg_en = 1;
	else
		return;

	ve_en = p->param[ve_dnlp_enable];
	dnlp_sel = p->param[ve_dnlp_sel];

	/* hist auto range parms */
	dnlp_alg_param.dnlp_lowrange = p->param[ve_dnlp_lowrange];
	dnlp_alg_param.dnlp_hghrange = p->param[ve_dnlp_hghrange];
	dnlp_alg_param.dnlp_auto_rng = p->param[ve_dnlp_auto_rng];

	/* histogram refine parms (remove bb affects) */
	dnlp_alg_param.dnlp_bbd_ratio_low = p->param[ve_dnlp_bbd_ratio_low];
	dnlp_alg_param.dnlp_bbd_ratio_hig = p->param[ve_dnlp_bbd_ratio_hig];

	/* calc iir-coef params */
	dnlp_alg_param.dnlp_mvreflsh = p->param[ve_dnlp_mvreflsh];
	dnlp_alg_param.dnlp_smhist_ck = p->param[ve_dnlp_smhist_ck];

	/* gains to delta of curves (for strength of the DNLP) */
	dnlp_alg_param.dnlp_final_gain = p->param[ve_dnlp_final_gain];
	dnlp_alg_param.dnlp_clahe_gain_neg = p->param[ve_dnlp_clahe_gain_neg];
	dnlp_alg_param.dnlp_clahe_gain_pos = p->param[ve_dnlp_clahe_gain_pos];

	/* coef of blending between gma_scurv and clahe curves */
	dnlp_alg_param.dnlp_mtdbld_rate =  p->param[ve_dnlp_mtdbld_rate];
	dnlp_alg_param.dnlp_adpmtd_lbnd = p->param[ve_dnlp_adpmtd_lbnd];
	dnlp_alg_param.dnlp_adpmtd_hbnd = p->param[ve_dnlp_adpmtd_hbnd];

	/* for gma_scurvs processing range */
	dnlp_alg_param.dnlp_sbgnbnd = p->param[ve_dnlp_sbgnbnd];
	dnlp_alg_param.dnlp_sendbnd = p->param[ve_dnlp_sendbnd];

	/* curve- clahe */
	dnlp_alg_param.dnlp_cliprate_v3 = p->param[ve_dnlp_cliprate_v3];
	dnlp_alg_param.dnlp_cliprate_min = p->param[ve_dnlp_cliprate_min];
	dnlp_alg_param.dnlp_adpcrat_lbnd = p->param[ve_dnlp_adpcrat_lbnd];
	dnlp_alg_param.dnlp_adpcrat_hbnd = p->param[ve_dnlp_adpcrat_hbnd];

	/* for clahe_curvs processing range */
	dnlp_alg_param.dnlp_clashBgn = p->param[ve_dnlp_clashBgn];
	dnlp_alg_param.dnlp_clashEnd = p->param[ve_dnlp_clashEnd];

	/* black white extension control params */
	dnlp_alg_param.dnlp_blkext_rate = p->param[ve_dnlp_blkext_rate];
	dnlp_alg_param.dnlp_whtext_rate = p->param[ve_dnlp_whtext_rate];
	dnlp_alg_param.dnlp_blkext_ofst = p->param[ve_dnlp_blkext_ofst];
	dnlp_alg_param.dnlp_whtext_ofst = p->param[ve_dnlp_whtext_ofst];
	dnlp_alg_param.dnlp_bwext_div4x_min = p->param[ve_dnlp_bwext_div4x_min];

	/* brightness_plus */
	dnlp_alg_param.dnlp_blk_cctr = p->param[ve_dnlp_blk_cctr];
	dnlp_alg_param.dnlp_brgt_ctrl = p->param[ve_dnlp_brgt_ctrl];
	dnlp_alg_param.dnlp_brgt_range = p->param[ve_dnlp_brgt_range];
	dnlp_alg_param.dnlp_brght_add = p->param[ve_dnlp_brght_add];
	dnlp_alg_param.dnlp_brght_max = p->param[ve_dnlp_brght_max];

	/* adaptive saturation compensations */
	dnlp_alg_param.dnlp_satur_rat = p->param[ve_dnlp_satur_rat];
	dnlp_alg_param.dnlp_satur_max = p->param[ve_dnlp_satur_max];
	dnlp_alg_param.dnlp_scurv_low_th = p->param[ve_dnlp_scurv_low_th];
	dnlp_alg_param.dnlp_scurv_mid1_th = p->param[ve_dnlp_scurv_mid1_th];
	dnlp_alg_param.dnlp_scurv_mid2_th = p->param[ve_dnlp_scurv_mid2_th];
	dnlp_alg_param.dnlp_scurv_hgh1_th = p->param[ve_dnlp_scurv_hgh1_th];
	dnlp_alg_param.dnlp_scurv_hgh2_th = p->param[ve_dnlp_scurv_hgh2_th];
	dnlp_alg_param.dnlp_mtdrate_adp_en =
		p->param[ve_dnlp_mtdrate_adp_en];
	/* TODO: ve_dnlp_set_saturtn = p->dnlp_set_saturtn; */

	/*load static curve*/
	for (i = 0; i < 65; i++) {
		dnlp_scurv_low[i] = p->ve_dnlp_scurv_low[i];
		dnlp_scurv_mid1[i] = p->ve_dnlp_scurv_mid1[i];
		dnlp_scurv_mid2[i] = p->ve_dnlp_scurv_mid2[i];
		dnlp_scurv_hgh1[i] = p->ve_dnlp_scurv_hgh1[i];
		dnlp_scurv_hgh2[i] = p->ve_dnlp_scurv_hgh2[i];
	}
	/*load gain var*/
	for (i = 0; i < 49; i++)
		gain_var_lut49[i] = p->ve_gain_var_lut49[i];
	/*load wext gain*/
	for (i = 0; i < 48; i++)
		wext_gain[i] = p->ve_wext_gain[i];

	if (ve_en) {
		/* clear historic luma sum */
		ve_dnlp_luma_sum = 0;
		/* init tgt & lpf */
		for (i = 0; i < 64; i++) {
			ve_dnlp_tgt[i] = i << 2;
			ve_dnlp_lpf[i] = (ulong)ve_dnlp_tgt[i] << ve_dnlp_rt;
		}
		/* calculate dnlp reg data */
		ve_dnlp_calculate_reg();
		/* load dnlp reg data */
		ve_dnlp_load_reg();
		/* enable dnlp */
		ve_enable_dnlp();
	} else {
		/* disable dnlp */
		ve_disable_dnlp();
	}

}

