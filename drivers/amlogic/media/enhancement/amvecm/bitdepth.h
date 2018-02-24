#ifndef BITDEPTH_H_
#define BITDEPTH_H_

extern void vpp_data_path_test(void);

enum data_conv_mode_e {
	U_TO_S_NULL,
	U12_TO_S12,
	U10_TO_U12,
	U12_TO_U10,
	S12_TO_U12,
};

enum vd_if_bits_mode_e {
	BIT_MODE_8BIT = 0,
	BIT_MODE_10BIT_422,
	BIT_MODE_10BIT_444,
	BIT_MODE_10BIT_422_FULLPACK,
};

enum data_path_node_e {
	VD1_IF = 0,
	VD2_IF,
	CORE1_EXTMODE,
	PRE_BLEDN_SWITCH,
	DITHER,/*place after preblend*/
	PRE_U2S,/*5*/
	PRE_S2U,
	POST_BLEDN_SWITCH,
	WATER_MARK_SWITCH,
	GAIN_OFFSET_SWITCH,
	POST_U2S,/*10*/
	POST_S2U,
	CHROMA_CORING,
	BLACK_EXT,
	BLUESTRETCH,
	VADJ1,
	VADJ2,
	NODE_MAX,
};

/*extend 2bit 0 in high bits*/
#define EXTMODE_HIGH	1
/*extend 2bit 0 in low bits*/
#define EXTMODE_LOW	0

extern void vpp_bitdepth_config(unsigned int bitdepth);
extern void vpp_datapath_config(unsigned int node, unsigned int param1,
	unsigned int param2);
extern void vpp_datapath_status(void);
extern void vpp_set_12bit_datapath1(void);
extern void vpp_set_12bit_datapath2(void);
extern void vpp_set_pre_s2u(enum data_conv_mode_e conv_mode);
extern void vpp_set_10bit_datapath1(void);
extern void vpp_set_12bit_datapath_g12a(void);

#endif

