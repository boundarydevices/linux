


/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "set_hdr2_v0.h"
#include "arch/vpp_hdr_regs.h"
#include "arch/vpp_regs.h"

//#define HDR2_MODULE
//#define HDR2_PRINT

#ifndef HDR2_MODULE

// sdr to hdr table  12bit
int eotf_lut0[143] = {
	0xfc000, 0x20000, 0x29470, 0x2efb8,
	0x32f70, 0x35f2e, 0x389b7, 0x3aab8, 0x3c984,
	0x3e188, 0x3fd98, 0x40ef0, 0x42145,
	0x435de, 0x44668, 0x45316, 0x46101, 0x4c02c,
	0x50000, 0x52d56, 0x554ad, 0x57a96,
	0x59472, 0x5b009, 0x5c822, 0x5daac, 0x5efbb,
	0x603b2, 0x610e0, 0x61f70, 0x62f6e,
	0x64070, 0x649ea, 0x65428, 0x65f2e, 0x66b01,
	0x677a5, 0x6828e, 0x689b7, 0x6914d,
	0x69954, 0x6a1cc, 0x6aab7, 0x6b418, 0x6bdef,
	0x6c41f, 0x6c984, 0x6cf26, 0x6d507,
	0x6db28, 0x6e188, 0x6e829, 0x6ef0c, 0x6f630,
	0x6fd98, 0x702a1, 0x70699, 0x70ab3,
	0x70eef, 0x7134f, 0x717d3, 0x71c79, 0x72144,
	0x72634, 0x72b48, 0x73080, 0x735de,
	0x73b62, 0x74085, 0x7436d, 0x74668, 0x74976,
	0x74c97, 0x74fcd, 0x75316, 0x75672,
	0x759e3, 0x75d68, 0x76101, 0x764ae, 0x76870,
	0x76c47, 0x77032, 0x77433, 0x77848,
	0x77c72, 0x78059, 0x78283, 0x784b8, 0x786f8,
	0x78943, 0x78b99, 0x78df9, 0x79065,
	0x792dc, 0x7955e, 0x797eb, 0x79a83, 0x79d27,
	0x79fd6, 0x7a291, 0x7a557, 0x7a828,
	0x7ab05, 0x7adee, 0x7b0e2, 0x7b3e2, 0x7b6ee,
	0x7ba05, 0x7bd29, 0x7c02c, 0x7c1ca,
	0x7c36d, 0x7c517, 0x7c6c7, 0x7c87d, 0x7ca39,
	0x7cbfb, 0x7cdc3, 0x7cf92, 0x7d167,
	0x7d342, 0x7d523, 0x7d70a, 0x7d8f8, 0x7daec,
	0x7dce7, 0x7dee7, 0x7e0ef, 0x7e2fc,
	0x7e510, 0x7e72b, 0x7e94c, 0x7eb73, 0x7eda2,
	0x7efd6, 0x7f211, 0x7f453, 0x7f69b,
	0x7f8ea, 0x7fb40, 0x7fd9c, 0x7ffff
};
int oetf_lut0[149] = {
	0x000, 0x004, 0x006, 0x009, 0x00d, 0x013, 0x01c, 0x02a, 0x03c,
	0x044, 0x04a, 0x050, 0x056, 0x060, 0x069, 0x071, 0x079, 0x086,
	0x092, 0x09d, 0x0a7, 0x0b9, 0x0c9, 0x0d7, 0x0e3, 0x0fa, 0x10e,
	0x120, 0x130, 0x14d, 0x166, 0x17c, 0x190, 0x1b3, 0x1d2, 0x1ed,
	0x205, 0x22f, 0x254, 0x274, 0x291, 0x2c3, 0x2ee, 0x313, 0x334,
	0x353, 0x36e, 0x388, 0x39f, 0x3b6, 0x3ca, 0x3de, 0x3f0, 0x413,
	0x432, 0x44f, 0x469, 0x482, 0x499, 0x4af, 0x4c4, 0x4ea, 0x50d,
	0x52d, 0x54a, 0x566, 0x57f, 0x597, 0x5ae, 0x5d8, 0x5fe, 0x620,
	0x640, 0x65e, 0x67a, 0x694, 0x6ac, 0x6d9, 0x702, 0x727, 0x74a,
	0x769, 0x787, 0x7a2, 0x7bc, 0x7ec, 0x817, 0x83f, 0x863, 0x884,
	0x8a3, 0x8c0, 0x8db, 0x90d, 0x93a, 0x963, 0x988, 0x9ab, 0x9cb,
	0x9e9, 0xa05, 0xa38, 0xa67, 0xa90, 0xab7, 0xada, 0xafb, 0xb1a,
	0xb36, 0xb6a, 0xb99, 0xbc4, 0xbeb, 0xc0e, 0xc30, 0xc4e, 0xc6b,
	0xca0, 0xccf, 0xcf9, 0xd20, 0xd44, 0xd65, 0xd84, 0xda1, 0xdd5,
	0xe04, 0xe2e, 0xe54, 0xe78, 0xe98, 0xeb7, 0xed3, 0xeee, 0xf07,
	0xf1e, 0xf35, 0xf4a, 0xf5e, 0xf72, 0xf84, 0xf96, 0xfa7, 0xfb7,
	0xfc7, 0xfd6, 0xfe4, 0xff2, 0xfff
};

#define OGAIN_NUM 149
static int num_ogain_lut0 = OGAIN_NUM;
int ogain_lut0[OGAIN_NUM] = {
	0x400, 0x000, 0x001, 0x004, 0x010, 0x018, 0x020, 0x030, 0x050,
	0x070, 0x080, 0x090, 0x0a0, 0x0b0, 0x0c0, 0x0d0, 0x0e0, 0x0f0,
	0x100, 0x110, 0x120, 0x130, 0x140, 0x150, 0x160, 0x170, 0x180,
	0x200, 0x210, 0x220, 0x230, 0x240, 0x250, 0x260, 0x270, 0x280,
	0x290, 0x2a0, 0x2b0, 0x2c0, 0x2d0, 0x2e0, 0x2f0, 0x300, 0x300,
	0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,
	0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,
	0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,
	0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,
	0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,
	0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,
	0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,
	0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,
	0x300, 0x304, 0x33e, 0x374, 0x3a8, 0x3d8, 0x406, 0x430, 0x47c,
	0x4be, 0x4f5, 0x522, 0x546, 0x561, 0x574, 0x580, 0x585, 0x583,
	0x57b, 0x56e, 0x55c, 0x546, 0x52d, 0x510, 0x4f1, 0x4d0, 0x4ad,
	0x48a, 0x466, 0x443, 0x421, 0x400
};

module_param_array(ogain_lut0, int, &num_ogain_lut0, 0664);
MODULE_PARM_DESC(ogain_lut0, "\n knee_setting, 256=1.0\n");

int cgain_lut0[65] = {
	0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400,
	0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400,
	0x4c0, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400,
	0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x40e,
	0x429, 0x444, 0x45f, 0x479, 0x492, 0x4ab, 0x4c3, 0x4db, 0x4f2,
	0x509, 0x520, 0x536, 0x54c, 0x561, 0x576, 0x58b, 0x59f, 0x5b3,
	0x5c0, 0x5d0, 0x5f2, 0x609, 0x620, 0x636, 0x64c, 0x661, 0x676,
	0x68b, 0x69f
};

// hdr10 to gamma lut 12bit (hdr to sdr)
int eotf_lut1[143] = {
	0xfc000, 0x10400, 0x17000, 0x1b000,
	0x1dc80, 0x20440, 0x21e40, 0x23c40, 0x24f00,
	0x261e0, 0x276c0, 0x286d0, 0x29340,
	0x2a0d0, 0x2af60, 0x2bf00, 0x2c7e0, 0x30fe8,
	0x3483e, 0x37460, 0x39707, 0x3bb91,
	0x3d468, 0x3efee, 0x40878, 0x41c0b, 0x43308,
	0x446e7, 0x45664, 0x46831, 0x47c8c,
	0x489da, 0x496fa, 0x4a5ca, 0x4b673, 0x4c491,
	0x4cf03, 0x4daa8, 0x4e79d, 0x4f5ff,
	0x502f7, 0x50bc5, 0x5157d, 0x52032, 0x52bfa,
	0x538eb, 0x5438e, 0x54b55, 0x553d7,
	0x55d24, 0x5674c, 0x57260, 0x57e72, 0x585cb,
	0x58cf2, 0x594b7, 0x59d28, 0x5a652,
	0x5b042, 0x5bb07, 0x5c359, 0x5c9aa, 0x5d080,
	0x5d7e4, 0x5dfe2, 0x5e884, 0x5f1d7,
	0x5fbe7, 0x60361, 0x6093d, 0x60f8d, 0x6165c,
	0x61db2, 0x62598, 0x62e1a, 0x63743,
	0x6408f, 0x645dc, 0x64b91, 0x651b4, 0x6584c,
	0x65f64, 0x66704, 0x66f35, 0x67803,
	0x680bc, 0x685d0, 0x68b45, 0x69122, 0x6976c,
	0x69e2e, 0x6a56f, 0x6ad39, 0x6b595,
	0x6be8f, 0x6c418, 0x6c944, 0x6ced0, 0x6d4c5,
	0x6db29, 0x6e206, 0x6e962, 0x6f14a,
	0x6f9c5, 0x70170, 0x70652, 0x70b91, 0x71132,
	0x7173d, 0x71dbb, 0x724b2, 0x72c2d,
	0x73436, 0x73cd6, 0x7430d, 0x74807, 0x74d60,
	0x7531f, 0x7594b, 0x75fed, 0x7670f,
	0x76eba, 0x776f8, 0x77fd5, 0x784af, 0x789d0,
	0x78f54, 0x79545, 0x79ba9, 0x7a28b,
	0x7a9f5, 0x7b1f0, 0x7ba8a, 0x7c1e7, 0x7c6e6,
	0x7cc49, 0x7d218, 0x7d85c, 0x7df1e,
	0x7e66b, 0x7ee4c, 0x7f6ce, 0x7ffff
};
int oetf_lut1[149] = {
	0x000, 0x001, 0x002, 0x002, 0x003, 0x004, 0x005, 0x007, 0x00a,
	0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f, 0x010, 0x011, 0x013,
	0x014, 0x015, 0x017, 0x019, 0x01b, 0x01d, 0x01e, 0x021, 0x024,
	0x026, 0x028, 0x02c, 0x030, 0x033, 0x036, 0x03b, 0x040, 0x044,
	0x048, 0x04f, 0x055, 0x05b, 0x060, 0x069, 0x072, 0x079, 0x080,
	0x086, 0x08c, 0x092, 0x098, 0x09d, 0x0a2, 0x0a6, 0x0ab, 0x0b3,
	0x0bc, 0x0c3, 0x0ca, 0x0d1, 0x0d8, 0x0de, 0x0e4, 0x0f0, 0x0fa,
	0x104, 0x10e, 0x117, 0x120, 0x128, 0x130, 0x140, 0x14e, 0x15c,
	0x168, 0x175, 0x180, 0x18c, 0x196, 0x1ab, 0x1be, 0x1d0, 0x1e1,
	0x1f1, 0x201, 0x210, 0x21e, 0x23a, 0x253, 0x26b, 0x282, 0x298,
	0x2ad, 0x2c1, 0x2d4, 0x2f8, 0x31b, 0x33b, 0x359, 0x376, 0x392,
	0x3ad, 0x3c7, 0x3f7, 0x425, 0x450, 0x478, 0x49f, 0x4c4, 0x4e8,
	0x50a, 0x54b, 0x588, 0x5c1, 0x5f8, 0x62b, 0x65d, 0x68c, 0x6ba,
	0x711, 0x762, 0x7af, 0x7f7, 0x83c, 0x87e, 0x8be, 0x8fb, 0x96e,
	0x9db, 0xa41, 0xaa2, 0xafe, 0xb56, 0xbab, 0xbfd, 0xc4b, 0xc97,
	0xce0, 0xd28, 0xd6d, 0xdb0, 0xdf1, 0xe31, 0xe70, 0xead, 0xee8,
	0xf22, 0xf5b, 0xf93, 0xfca, 0xfff
};
static int num_ogain_lut1 = OGAIN_NUM;
int ogain_lut1[OGAIN_NUM] = {
	0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800,
	0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800,
	0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800,
	0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800,
	0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800, 0x800,
	0x7fe, 0x7fe, 0x7fe, 0x7fe, 0x7fe, 0x7fd, 0x7fd, 0x7fd, 0x7fd,
	0x7fc, 0x7fc, 0x7fc, 0x7fb, 0x7fb, 0x7fa, 0x7fa, 0x7f9, 0x7f9,
	0x7f8, 0x7f7, 0x7f6, 0x7f6, 0x7f5, 0x7f4, 0x7f3, 0x7f1, 0x7f0,
	0x7ee, 0x7ed, 0x7eb, 0x7ea, 0x7e8, 0x7e5, 0x7e2, 0x7df, 0x7dc,
	0x7d9, 0x7d6, 0x7d3, 0x7d0, 0x7ca, 0x7c4, 0x7be, 0x7b8, 0x7b2,
	0x7ac, 0x7a6, 0x7a0, 0x794, 0x788, 0x77c, 0x770, 0x764, 0x758,
	0x74c, 0x740, 0x728, 0x710, 0x6f8, 0x6e0, 0x6c8, 0x6b1, 0x699,
	0x681, 0x651, 0x621, 0x5f2, 0x5c3, 0x593, 0x564, 0x535, 0x506,
	0x4a9, 0x44c, 0x4f0, 0x494, 0x43a, 0x400, 0x400, 0x400, 0x400,
	0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400,
	0x400, 0x3ee, 0x3a4, 0x396, 0x385, 0x375, 0x355, 0x345, 0x335,
	0x355, 0x375, 0x395, 0x3a9, 0x400
};
module_param_array(ogain_lut1, int, &num_ogain_lut1, 0664);
MODULE_PARM_DESC(ogain_lut1, "\n ogain_lut1\n");

int cgain_lut1[65] = {
	0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400,
	0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400,
	0x4c0, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400,
	0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x40e,
	0x429, 0x444, 0x45f, 0x479, 0x492, 0x4ab, 0x4c3, 0x4db, 0x4f2,
	0x509, 0x520, 0x536, 0x54c, 0x561, 0x576, 0x58b, 0x59f, 0x5b3,
	0x5c0, 0x5d0, 0x5f2, 0x609, 0x620, 0x636, 0x64c, 0x661, 0x676,
	0x68b, 0x69f
};

// sdr to hdr 10bit (gamma to peak)
int eotf_lut2[143] = {
	0xfc000, 0x20000, 0x29470, 0x2efb8,
	0x32f70, 0x35f2e, 0x389b7, 0x3aab8, 0x3c984,
	0x3e188, 0x3fd98, 0x40ef0, 0x42145,
	0x435de, 0x44668, 0x45316, 0x46101, 0x4c02c,
	0x50000, 0x52d56, 0x554ad, 0x57a96,
	0x59472, 0x5b009, 0x5c822, 0x5daac, 0x5efbb,
	0x603b2, 0x610e0, 0x61f70, 0x62f6e,
	0x64070, 0x649ea, 0x65428, 0x65f2e, 0x66b01,
	0x677a5, 0x6828e, 0x689b7, 0x6914d,
	0x69954, 0x6a1cc, 0x6aab7, 0x6b418, 0x6bdef,
	0x6c41f, 0x6c984, 0x6cf26, 0x6d507,
	0x6db28, 0x6e188, 0x6e829, 0x6ef0c, 0x6f630,
	0x6fd98, 0x702a1, 0x70699, 0x70ab3,
	0x70eef, 0x7134f, 0x717d3, 0x71c79, 0x72144,
	0x72634, 0x72b48, 0x73080, 0x735de,
	0x73b62, 0x74085, 0x7436d, 0x74668, 0x74976,
	0x74c97, 0x74fcd, 0x75316, 0x75672,
	0x759e3, 0x75d68, 0x76101, 0x764ae, 0x76870,
	0x76c47, 0x77032, 0x77433, 0x77848,
	0x77c72, 0x78059, 0x78283, 0x784b8, 0x786f8,
	0x78943, 0x78b99, 0x78df9, 0x79065,
	0x792dc, 0x7955e, 0x797eb, 0x79a83, 0x79d27,
	0x79fd6, 0x7a291, 0x7a557, 0x7a828,
	0x7ab05, 0x7adee, 0x7b0e2, 0x7b3e2, 0x7b6ee,
	0x7ba05, 0x7bd29, 0x7c02c, 0x7c1ca,
	0x7c36d, 0x7c517, 0x7c6c7, 0x7c87d, 0x7ca39,
	0x7cbfb, 0x7cdc3, 0x7cf92, 0x7d167,
	0x7d342, 0x7d523, 0x7d70a, 0x7d8f8, 0x7daec,
	0x7dce7, 0x7dee7, 0x7e0ef, 0x7e2fc,
	0x7e510, 0x7e72b, 0x7e94c, 0x7eb73, 0x7eda2,
	0x7efd6, 0x7f211, 0x7f453, 0x7f69b,
	0x7f8ea, 0x7fb40, 0x7fd9c, 0x7ffff
};

int oetf_lut2[149] = {
	0x000, 0x001, 0x001, 0x002, 0x003, 0x005, 0x007, 0x00a, 0x00f,
	0x011, 0x013, 0x014, 0x015, 0x018, 0x01a, 0x01c, 0x01e, 0x022,
	0x025, 0x027, 0x02a, 0x02e, 0x032, 0x036, 0x039, 0x03f, 0x044,
	0x048, 0x04c, 0x053, 0x059, 0x05f, 0x064, 0x06d, 0x074, 0x07b,
	0x081, 0x08c, 0x095, 0x09d, 0x0a4, 0x0b1, 0x0bb, 0x0c5, 0x0cd,
	0x0d5, 0x0dc, 0x0e2, 0x0e8, 0x0ed, 0x0f3, 0x0f7, 0x0fc, 0x105,
	0x10c, 0x114, 0x11a, 0x121, 0x126, 0x12c, 0x131, 0x13b, 0x143,
	0x14b, 0x153, 0x159, 0x160, 0x166, 0x16b, 0x176, 0x17f, 0x188,
	0x190, 0x198, 0x19e, 0x1a5, 0x1ab, 0x1b6, 0x1c1, 0x1ca, 0x1d2,
	0x1da, 0x1e2, 0x1e9, 0x1ef, 0x1fb, 0x206, 0x210, 0x219, 0x221,
	0x229, 0x230, 0x237, 0x243, 0x24e, 0x259, 0x262, 0x26b, 0x273,
	0x27a, 0x281, 0x28e, 0x29a, 0x2a4, 0x2ae, 0x2b7, 0x2bf, 0x2c6,
	0x2ce, 0x2db, 0x2e6, 0x2f1, 0x2fb, 0x304, 0x30c, 0x314, 0x31b,
	0x328, 0x334, 0x33e, 0x348, 0x351, 0x359, 0x361, 0x368, 0x375,
	0x381, 0x38b, 0x395, 0x39e, 0x3a6, 0x3ae, 0x3b5, 0x3bb, 0x3c2,
	0x3c8, 0x3cd, 0x3d2, 0x3d8, 0x3dc, 0x3e1, 0x3e5, 0x3ea, 0x3ee,
	0x3f2, 0x3f5, 0x3f9, 0x3fd, 0x3ff
};

int ogain_lut2[149] = {
	0x3ff, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x000, 0x000, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001,
	0x002, 0x002, 0x002, 0x002, 0x002, 0x003, 0x003, 0x003, 0x003,
	0x004, 0x004, 0x004, 0x005, 0x005, 0x006, 0x006, 0x007, 0x007,
	0x008, 0x009, 0x00a, 0x00a, 0x00b, 0x00c, 0x00d, 0x00f, 0x010,
	0x012, 0x013, 0x015, 0x016, 0x018, 0x01b, 0x01e, 0x021, 0x024,
	0x027, 0x02a, 0x02d, 0x030, 0x035, 0x03b, 0x041, 0x047, 0x04d,
	0x052, 0x058, 0x05e, 0x069, 0x075, 0x080, 0x08c, 0x097, 0x0a2,
	0x0ad, 0x0b8, 0x0ce, 0x0e4, 0x0f9, 0x10e, 0x123, 0x138, 0x14c,
	0x161, 0x189, 0x1af, 0x1d5, 0x1fb, 0x21f, 0x242, 0x264, 0x286,
	0x2c7, 0x304, 0x33e, 0x374, 0x3a8, 0x3d8, 0x3ff, 0x3ff, 0x3ff,
	0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff,
	0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff,
	0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff
};

int cgain_lut2[65] = {
	0xc00, 0xc00, 0xc00, 0xc00, 0xc00, 0xc00, 0xc00, 0xc00, 0xc00,
	0xc00, 0xc00, 0xc0e, 0xc79, 0xcdb, 0xd36, 0xd8b, 0xdda, 0xe25,
	0xe6b, 0xead, 0xeec, 0xf28, 0xf61, 0xf98, 0xfcc, 0xfff, 0x102f,
	0x105d, 0x108a, 0x10b5, 0x10df, 0x1107, 0x112e, 0x1154, 0x1178, 0x119c,
	0x11bf, 0x11e0, 0x1201, 0x1221, 0x1240, 0x125e, 0x127c, 0x1299, 0x12b5,
	0x12d1, 0x12ec, 0x1306, 0x1320, 0x1339, 0x1352, 0x136b, 0x1383, 0x139a,
	0x13b1, 0x13c7, 0x13de, 0x13f3, 0x1409, 0x141e, 0x1432, 0x1447, 0x145b,
	0x146e, 0x1482
};
#else //HDR2_MODULE
int64_t FloatRev(int64_t ia)
{
	int64_t tmp;
	int64_t iA_exp;
	int64_t iA_val;

	iA_exp = ia >> precision;
	iA_val = ia & 0x3fff;

	if (iA_exp == 0x3f)
		tmp = 0;
	else if (iA_exp >= precision)
		tmp = ((int64_t)(iA_val + (1ULL << precision)))
			<< (iA_exp - precision);
	else
		tmp = ((int64_t)(iA_val + (1ULL << precision) +
			(1ULL << (precision - iA_exp - 1)))) >>
			(precision - iA_exp);

	return tmp;
}



int64_t FloatCon(int64_t iA, int MOD)
{
	int64_t oxs;
	int64_t oXs_exp;
	int64_t oXs_val;
	int64_t exp;
	int64_t val;
	int64_t iA_tmp;
	int64_t diff;

	exp = LOG2(iA);
	/*exp = iA==0 ? 0 : LOG2(iA);*/

	oXs_exp = exp;
	if (exp >= precision)
		val = iA >> (exp - precision);
	else
		val = iA << (precision - exp);

	oXs_val = val & ((1 << precision) - 1);

	if (iA == 0) {
		oXs_exp = 0x3f;
		oXs_val = 0;
	}

	if (exp >= MOD) {
		oXs_exp = MOD - 1;
		oXs_val = 0x3fff;
	}

	oxs = (oXs_exp << 14) + oXs_val;
	iA_tmp = FloatRev(oxs);

	return oxs;
}

int64_t pq_eotf(int64_t e)
{
	double m1 = 2610.0/4096/4;
	double m2 = 2523.0/4096*128;
	double c3 = 2392.0/4096*32;
	double c2 = 2413.0/4096*32;
	double c1 = c3-c2+1;
	int64_t o;
	double e_v = ((double)e) / pow(2, IE_BW);

	o = (int64_t)(pow(((MAX((pow(e_v, (1 / m2)) - c1), 0)) /
		(c2 - c3 * pow(e_v, (1 / m2)))),
		(1 / m1)) * (pow(2, O_BW)) + 0.5);
	return o;
}

int64_t pq_oetf(int64_t o)
{
	double m1 = 2610.0/4096/4;
	double m2 = 2523.0/4096*128;
	double c3 = 2392.0/4096*32;
	double c2 = 2413.0/4096*32;
	double c1 = c3 - c2 + 1;
	int64_t e;
	double o_v = o / pow(2, O_BW);

	if (o == pow(2, O_BW))
		e = (int64_t)pow(2, OE_BW);
	else
		e = (int64_t)(pow(((c1 + c2 * pow(o_v, m1)) /
			(1 + c3 * pow(o_v, m1))),
			m2) * (pow(2, OE_BW)) + 0 .5);
	return e;
}

int64_t gm_eotf(int64_t e)
{
	int64_t o;
	double e_v = ((double)e) / pow(2, IE_BW);

	o = (int64_t)(pow(e_v, 2.4) * (pow(2, O_BW)) + 0.5);
	return o;
}

int64_t gm_oetf(int64_t o)
{
	int64_t e;
	double o_v = o / pow(2, O_BW);

	e = (int64_t)(pow(o_v, 1 / 2.4) * (pow(2, OE_BW)) + 0.5);
	return e;
}

int64_t sld_eotf(int64_t e)
{
	double m = 0.15;
	double p = 2.0;
	int64_t o;
	double e_v = ((double)e) / pow(2, 12);
	double tmp = pow((e_v), -(1.0 / m));

	o = (int64_t)(1.0 / (pow((e_v), -(1.0 / m)) * p - p + 1.0) *
			(pow(2, 32)) + 0.5);
	return o;
}

int64_t sld_oetf(int64_t o)
{
	double m = 0.15;
	double p = 2.0;
	int64_t e;
	double o_v = o / pow(2, 32);

	e = (int64_t)(pow((p*o_v) / ((p - 1)*o_v + 1.0), m) *
			(pow(2, OE_BW)) + 0.5);
	return e;
}

int64_t hlg_eotf(int64_t e)
{
	double a = 0.17883277;
	double b = 0.02372241;
	double c = 1.00429347;
	double e_v = ((double)e) / pow(2, IE_BW);
	double o_v;
	int64_t o;

	if (e_v < 0.5)
		o_v = pow(e_v, 2) / 3;
	else
		o_v = exp((e_v - c) / a) + b;
	o = (int64_t)(o_v * (pow(2, O_BW)) + 0.5);
	return o;
}


int64_t hlg_oetf(int64_t o)
{
	double a = 0.17883277;
	double b = 0.02372241;
	double c = 1.00429347;
	double tmp = 0.08333333;
	int64_t e;
	double e_v;
	double o_v = o / pow(2, O_BW);

	if (o_v < tmp)
		e_v = pow((3 * o_v), 0.5);
	else
		e_v = a * log(o_v - b) + c;

	e = (int64_t)(e_v * (pow(2, OE_BW)) + 0.5);
	return e;
}

int64_t ootf_gain(int64_t o)
{
	double p1 = fmt_io == 1;
	double p2 = 1;
	double p3 = fmt_io == 2;
	double p4 = 1;
	double p5 = 0;
	double o_v = o / pow(2, O_BW);

	double y = 4 * o_v * pow((1 - o_v), 3) * p1 +
			6 * pow(o_v, 2) * pow((1 - o_v), 2) * p2 +
			4 * pow(o_v, 3) * (1 - o_v) * p3 + pow(o_v, 4);

	double gain = o_v == 0 ?  1 : y / o_v;
	int64_t gain_t;

	gain_t = (int64_t)(gain * (pow(2, OGAIN_BW - 2)) + 0.5);

	return gain_t;
}

int64_t hlg_gain(int64_t o)
{
	double p = 1.2 - 1 + 0.42 * (LOG2(peak_out / 1000) / LOG2(10));
	int64_t gain;
	double o_v = o / pow(2, O_BW);

	gain = (int64_t)(pow(o_v, p) * (pow(2, OGAIN_BW)) + 0.5);
	return gain;
}

int64_t nolinear_cgain(int64_t i)
{
	int64_t out;
	double ColorSaturationWeight = 1.2;
	double fscsm = 3 + ColorSaturationWeight *
			MAX((log(((double)i) / pow(2, OE_BW-2)) - 1), 0);

	out = (int64_t)(pow(2, 10) * fscsm);
	return out;
}

/*146bins*/
void eotf_float_gen(int64_t *o_out, MenuFun eotf)
{
	int64_t tmp_o, tmp_e;
	int i;

	for (i = 0; i < 16; i++) {
		tmp_e = (int64_t)((1ULL << (IE_BW - 10)) * i);
		tmp_o = eotf(tmp_e);
		o_out[i] = FloatCon(tmp_o, maxbit);
	}

	for (i = 2; i <= 128; i++) {
		tmp_e = (int64_t)((1ULL << (IE_BW - 7)) * i);
		if (tmp_e == (1 << IE_BW))
			tmp_o = 0xffffffff;
		else
			tmp_o = eotf(tmp_e);
		o_out[i + 14] = FloatCon(tmp_o, maxbit);
	}
}

/*149 bins piece wise lut*/
void oetf_float_gen(int64_t *bin_e, MenuFun oetf)
{
	/*int64_t bin_e[1024]; = zeros(1024);*/
	int64_t bin_o[1025];/* = zeros(1024);*/

	int i = 0, j;
	int bin_num = 0;

	bin_o[i] = 0;
	i++;
	/*bin1~bin8*/
	for (; pow(2, i - 1) * pow(2, 4) < pow(2, 11); i++)
		bin_o[i] = POW(2, i - 1) * POW(2, 4);

	bin_num = i;

	/*bin9~bin44*/
	for (j = 11; j < 20; j++) {/* bin_o< 2^20*/
		for (; i < bin_num + 4; i++)
			bin_o[i] = (i - bin_num) * (POW(2, j - 2)) + POW(2, j);
		bin_num = i;
	}
	bin_num = i;

	/*bin45~bin132*/
	for (j = 20; j < 31; j++) {/*bin_o<2 ^31*/
		for (; i < bin_num + 8; i++)
			bin_o[i] = (i - bin_num) * (POW(2, j - 3)) + POW(2, j);
		bin_num = i;
	}
	/*bin133~bin148*/
	for (; i < bin_num + 16; i++)
		bin_o[i] = (i - bin_num) * (POW(2, 31 - 4)) + POW(2, 31);

	bin_o[i] = 0x100000000;

	for (j = 0; j <= i; j++) {
		bin_e[j] = oetf(bin_o[j]);
		if (bin_e[j] >= (1 << OE_BW))
			bin_e[j] = (1 << OE_BW) - 1;
	}
}

void nolinear_lut_gen(int64_t *bin_c, MenuFun cgain)
{
	/*int bin_c[65]; = zeros(1024);*/
	/*int max_in = 1 << 12;*/
	/*bin_c : 4.10*/
	/*c gain input :OE_BW*/
	int j;

	for (j = 0; j <= 64; j++)
		bin_c[j] = cgain(j * 64);
}

#endif /*HDR2_MODULE*/

static uint force_din_swap = 0xff;
module_param(force_din_swap, uint, 0664);
MODULE_PARM_DESC(force_din_swap, "\n force_din_swap\n");

static uint force_mtrxo_en = 0xff;
module_param(force_mtrxo_en, uint, 0664);
MODULE_PARM_DESC(force_mtrxo_en, "\n force_mtrxo_en\n");

static uint force_mtrxi_en = 0xff;
module_param(force_mtrxi_en, uint, 0664);
MODULE_PARM_DESC(force_mtrxi_en, "\n force_mtrxi_en\n");

static uint force_eo_enable = 0xff;
module_param(force_eo_enable, uint, 0664);
MODULE_PARM_DESC(force_eo_enable, "\n force_eo_enable\n");

static uint force_oe_enable = 0xff;
module_param(force_oe_enable, uint, 0664);
MODULE_PARM_DESC(force_oe_enable, "\n force_oe_enable\n");

static uint force_ogain_enable = 0xff;
module_param(force_ogain_enable, uint, 0664);
MODULE_PARM_DESC(force_ogain_enable, "\n force_ogain_enable\n");

static uint force_cgain_enable = 0xff;
module_param(force_cgain_enable, uint, 0664);
MODULE_PARM_DESC(force_cgain_enable, "\n force_cgain_enable\n");

static uint out_luma = 8;
module_param(out_luma, uint, 0664);
MODULE_PARM_DESC(out_luma, "\n out_luma\n");

static uint in_luma = 1;/*1 as 100luminance*/
module_param(in_luma, uint, 0664);
MODULE_PARM_DESC(in_luma, "\n in_luma\n");

static uint adp_scal_shift = 10;/*1 as 100luminance*/
module_param(adp_scal_shift, uint, 0664);
MODULE_PARM_DESC(adp_scal_shift, "\n adp_scal_shift\n");

static uint alpha_oe_a = 0x1;
module_param(alpha_oe_a, uint, 0664);
MODULE_PARM_DESC(alpha_oe_a, "\n alpha_oe_a\n");

static uint alpha_oe_b = 0x1;
module_param(alpha_oe_b, uint, 0664);
MODULE_PARM_DESC(alpha_oe_b, "\n alpha_oe_b\n");

static uint hdr2_debug;
module_param(hdr2_debug, uint, 0664);
MODULE_PARM_DESC(hdr2_debug, "\n hdr2_debug\n");

/* gamut 3x3 matrix*/
int ncl_2020_709[9] = {
	3401, -1204, -149, -255, 2320, -17, -37, -206, 2291};
/*int cl_2020_709[9] =*/
	/*{-1775, 3867, -44, 3422, -1154, -220 ,-304,	43, 2309}; */
int ncl_709_2020[9] = {1285, 674, 89, 142, 1883, 23, 34, 180, 1834};
/*int cl_709_2020[9] =*/
	/*{436, 1465, 148, 1285, 674, 89, 34, 180, 1834}; */
/*int yrb2ycbcr_cl2020[15] =*/
	/*{876, 0, 0, -566, 0, 566, -902, 902, 0, -462, 0, 462, -521, 521, 0};*/

/* matrix coef */
int rgb2yuvpre[3]	= {0, 0, 0};
int rgb2yuvpos[3]	= {64, 512, 512};
int yuv2rgbpre[3]	= {-64, -512, -512};
int yuv2rgbpos[3]	= {0, 0, 0};

/*matrix coef BT709*/
int yuv2rgbmat[15] = {
	1197, 0, 0, 1197, 1851, 0, 1197, 0, 1163, 1197, 2271, 0, 1197, 0, 2011};
int rgb2ycbcr[15] = {
	230, 594, 52, -125, -323, 448, 448, -412, -36, 0, 0, 0, 0, 0, 0};
int rgb2ycbcr_ncl2020[15] = {
	230, 594, 52, -125, -323, 448, 448, -412, -36, 0, 0, 0, 0, 0, 0};
int rgb2ycbcr_709[15] = {
	186, 627, 63, -103, -345, 448, 448, -407, -41, 0, 0, 0, 0, 0, 0};
int ycbcr2rgb_709[15]  = {
	1192, 0, 1836, 1192, -217, -546, 1192, 2166, 0, 0, 0, 0, 0, 0, 0};
/*int yrb2ycbcr_cl2020[15] =*/
	/*{876, 0, 0, -566, 0, 566, -902, 902, 0, -462, 0, 462, -521, 521, 0};*/
int ycbcr2rgb_ncl2020[15] = {
	1197, 0, 1726, 1197, -193, -669, 1197, 2202, 0, 0, 0, 0, 0, 0, 0};
/*int ycbcr2yrb_cl2020[15] =*/
	/*{1197,0,0,1197,0,1163,1197,1851,0, 1197, 0, 2011, 1197, 2271, 0};*/

static int bypass_coeff[15] = {
	1024, 0, 0,
	0, 1024, 0,
	0, 0, 1024,
	0, 0, 0,
	0, 0, 0,
};

/*in/out matrix*/
void set_hdr_matrix(
	enum hdr_module_sel module_sel,
	enum hdr_matrix_sel mtx_sel,
	struct hdr_proc_mtx_param_s *hdr_mtx_param)
{
	unsigned int MATRIXI_COEF00_01 = 0;
	unsigned int MATRIXI_COEF02_10 = 0;
	unsigned int MATRIXI_COEF11_12 = 0;
	unsigned int MATRIXI_COEF20_21 = 0;
	unsigned int MATRIXI_COEF22 = 0;
	unsigned int MATRIXI_COEF30_31 = 0;
	unsigned int MATRIXI_COEF32_40 = 0;
	unsigned int MATRIXI_COEF41_42 = 0;
	unsigned int MATRIXI_OFFSET0_1 = 0;
	unsigned int MATRIXI_OFFSET2 = 0;
	unsigned int MATRIXI_PRE_OFFSET0_1 = 0;
	unsigned int MATRIXI_PRE_OFFSET2 = 0;
	unsigned int MATRIXI_CLIP = 0;
	unsigned int MATRIXI_EN_CTRL = 0;

	unsigned int MATRIXO_COEF00_01 = 0;
	unsigned int MATRIXO_COEF02_10 = 0;
	unsigned int MATRIXO_COEF11_12 = 0;
	unsigned int MATRIXO_COEF20_21 = 0;
	unsigned int MATRIXO_COEF22 = 0;
	unsigned int MATRIXO_COEF30_31 = 0;
	unsigned int MATRIXO_COEF32_40 = 0;
	unsigned int MATRIXO_COEF41_42 = 0;
	unsigned int MATRIXO_OFFSET0_1 = 0;
	unsigned int MATRIXO_OFFSET2 = 0;
	unsigned int MATRIXO_PRE_OFFSET0_1 = 0;
	unsigned int MATRIXO_PRE_OFFSET2 = 0;
	unsigned int MATRIXO_CLIP = 0;
	unsigned int MATRIXO_EN_CTRL = 0;

	unsigned int CGAIN_OFFT = 0;
	unsigned int CGAIN_COEF0 = 0;
	unsigned int CGAIN_COEF1 = 0;
	unsigned int ADPS_CTRL = 0;
	unsigned int ADPS_ALPHA0 = 0;
	unsigned int ADPS_ALPHA1 = 0;
	unsigned int ADPS_BETA0 = 0;
	unsigned int ADPS_BETA1 = 0;
	unsigned int ADPS_BETA2 = 0;
	unsigned int ADPS_COEF0 = 0;
	unsigned int ADPS_COEF1 = 0;
	unsigned int GMUT_CTRL = 0;
	unsigned int GMUT_COEF0 = 0;
	unsigned int GMUT_COEF1 = 0;
	unsigned int GMUT_COEF2 = 0;
	unsigned int GMUT_COEF3 = 0;
	unsigned int GMUT_COEF4 = 0;

	unsigned int hdr_ctrl = 0;

	int adpscl_mode = 0;

	int c_gain_lim_coef[3];
	int gmut_coef[3][3];
	int gmut_shift;
	int adpscl_enable[3];
	int adpscl_alpha[3];
	int adpscl_shift[3];
	int adpscl_ys_coef[3];
	int adpscl_beta[3];
	int adpscl_beta_s[3];

	int i = 0;
	int mtx[15] = {
		1024, 0, 0,
		0, 1024, 0,
		0, 0, 1024,
		0, 0, 0,
		0, 0, 0,
	};

	if (module_sel & VD1_HDR) {
		MATRIXI_COEF00_01 = VD1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = VD1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = VD1_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = VD1_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = VD1_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = VD1_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = VD1_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = VD1_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = VD1_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = VD1_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = VD1_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = VD1_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = VD1_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = VD1_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = VD1_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = VD1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = VD1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = VD1_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = VD1_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = VD1_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = VD1_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = VD1_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = VD1_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = VD1_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = VD1_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = VD1_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = VD1_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = VD1_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = VD1_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = VD1_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = VD1_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = VD1_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = VD1_HDR2_CGAIN_COEF1;
		ADPS_CTRL = VD1_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = VD1_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = VD1_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = VD1_HDR2_ADPS_BETA0;
		ADPS_BETA1 = VD1_HDR2_ADPS_BETA1;
		ADPS_BETA2 = VD1_HDR2_ADPS_BETA2;
		ADPS_COEF0 = VD1_HDR2_ADPS_COEF0;
		ADPS_COEF1 = VD1_HDR2_ADPS_COEF1;
		GMUT_CTRL = VD1_HDR2_GMUT_CTRL;
		GMUT_COEF0 = VD1_HDR2_GMUT_COEF0;
		GMUT_COEF1 = VD1_HDR2_GMUT_COEF1;
		GMUT_COEF2 = VD1_HDR2_GMUT_COEF2;
		GMUT_COEF3 = VD1_HDR2_GMUT_COEF3;
		GMUT_COEF4 = VD1_HDR2_GMUT_COEF4;

		hdr_ctrl = VD1_HDR2_CTRL;
	} else if (module_sel & VD2_HDR) {
		MATRIXI_COEF00_01 = VDIN1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = VDIN1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = VDIN1_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = VDIN1_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = VDIN1_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = VDIN1_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = VDIN1_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = VDIN1_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = VDIN1_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = VDIN1_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = VDIN1_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = VDIN1_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = VDIN1_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = VDIN1_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = VDIN1_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = VDIN1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = VDIN1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = VDIN1_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = VDIN1_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = VDIN1_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = VDIN1_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = VDIN1_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = VDIN1_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = VDIN1_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = VDIN1_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = VDIN1_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = VDIN1_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = VDIN1_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = VDIN1_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = VDIN1_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = VDIN1_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = VDIN1_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = VDIN1_HDR2_CGAIN_COEF1;
		ADPS_CTRL = VDIN1_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = VDIN1_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = VDIN1_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = VDIN1_HDR2_ADPS_BETA0;
		ADPS_BETA1 = VDIN1_HDR2_ADPS_BETA1;
		ADPS_BETA2 = VDIN1_HDR2_ADPS_BETA2;
		ADPS_COEF0 = VDIN1_HDR2_ADPS_COEF0;
		ADPS_COEF1 = VDIN1_HDR2_ADPS_COEF1;
		GMUT_CTRL = VDIN1_HDR2_GMUT_CTRL;
		GMUT_COEF0 = VDIN1_HDR2_GMUT_COEF0;
		GMUT_COEF1 = VDIN1_HDR2_GMUT_COEF1;
		GMUT_COEF2 = VDIN1_HDR2_GMUT_COEF2;
		GMUT_COEF3 = VDIN1_HDR2_GMUT_COEF3;
		GMUT_COEF4 = VDIN1_HDR2_GMUT_COEF4;

		hdr_ctrl = VDIN1_HDR2_CTRL;
	} else if (module_sel & OSD1_HDR) {
		MATRIXI_COEF00_01 = OSD1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = OSD1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = OSD1_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = OSD1_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = OSD1_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = OSD1_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = OSD1_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = OSD1_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = OSD1_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = OSD1_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = OSD1_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = OSD1_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = OSD1_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = OSD1_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = OSD1_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = OSD1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = OSD1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = OSD1_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = OSD1_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = OSD1_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = OSD1_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = OSD1_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = OSD1_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = OSD1_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = OSD1_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = OSD1_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = OSD1_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = OSD1_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = OSD1_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = OSD1_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = OSD1_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = OSD1_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = OSD1_HDR2_CGAIN_COEF1;
		ADPS_CTRL = OSD1_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = OSD1_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = OSD1_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = OSD1_HDR2_ADPS_BETA0;
		ADPS_BETA1 = OSD1_HDR2_ADPS_BETA1;
		ADPS_BETA2 = OSD1_HDR2_ADPS_BETA2;
		ADPS_COEF0 = OSD1_HDR2_ADPS_COEF0;
		ADPS_COEF1 = OSD1_HDR2_ADPS_COEF1;
		GMUT_CTRL = OSD1_HDR2_GMUT_CTRL;
		GMUT_COEF0 = OSD1_HDR2_GMUT_COEF0;
		GMUT_COEF1 = OSD1_HDR2_GMUT_COEF1;
		GMUT_COEF2 = OSD1_HDR2_GMUT_COEF2;
		GMUT_COEF3 = OSD1_HDR2_GMUT_COEF3;
		GMUT_COEF4 = OSD1_HDR2_GMUT_COEF4;

		hdr_ctrl = OSD1_HDR2_CTRL;
	} else if (module_sel & DI_HDR) {
		MATRIXI_COEF00_01 = DI_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = DI_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = DI_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = DI_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = DI_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = DI_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = DI_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = DI_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = DI_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = DI_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = DI_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = DI_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = DI_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = DI_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = DI_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = DI_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = DI_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = DI_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = DI_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = DI_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = DI_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = DI_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = DI_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = DI_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = DI_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = DI_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = DI_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = DI_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = DI_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = DI_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = DI_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = DI_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = DI_HDR2_CGAIN_COEF1;
		ADPS_CTRL = DI_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = DI_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = DI_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = DI_HDR2_ADPS_BETA0;
		ADPS_BETA1 = DI_HDR2_ADPS_BETA1;
		ADPS_BETA2 = DI_HDR2_ADPS_BETA2;
		ADPS_COEF0 = DI_HDR2_ADPS_COEF0;
		ADPS_COEF1 = DI_HDR2_ADPS_COEF1;
		GMUT_CTRL = DI_HDR2_GMUT_CTRL;
		GMUT_COEF0 = DI_HDR2_GMUT_COEF0;
		GMUT_COEF1 = DI_HDR2_GMUT_COEF1;
		GMUT_COEF2 = DI_HDR2_GMUT_COEF2;
		GMUT_COEF3 = DI_HDR2_GMUT_COEF3;
		GMUT_COEF4 = DI_HDR2_GMUT_COEF4;

		hdr_ctrl = DI_HDR2_CTRL;
	} else if (module_sel & VDIN0_HDR) {
		MATRIXI_COEF00_01 = VDIN0_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = VDIN0_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = VDIN0_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = VDIN0_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = VDIN0_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = VDIN0_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = VDIN0_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = VDIN0_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = VDIN0_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = VDIN0_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = VDIN0_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = VDIN0_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = VDIN0_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = VDIN0_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = VDIN0_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = VDIN0_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = VDIN0_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = VDIN0_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = VDIN0_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = VDIN0_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = VDIN0_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = VDIN0_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = VDIN0_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = VDIN0_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = VDIN0_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = VDIN0_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = VDIN0_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = VDIN0_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = VDIN0_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = VDIN0_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = VDIN0_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = VDIN0_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = VDIN0_HDR2_CGAIN_COEF1;
		ADPS_CTRL = VDIN0_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = VDIN0_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = VDIN0_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = VDIN0_HDR2_ADPS_BETA0;
		ADPS_BETA1 = VDIN0_HDR2_ADPS_BETA1;
		ADPS_BETA2 = VDIN0_HDR2_ADPS_BETA2;
		ADPS_COEF0 = VDIN0_HDR2_ADPS_COEF0;
		ADPS_COEF1 = VDIN0_HDR2_ADPS_COEF1;
		GMUT_CTRL = VDIN0_HDR2_GMUT_CTRL;
		GMUT_COEF0 = VDIN0_HDR2_GMUT_COEF0;
		GMUT_COEF1 = VDIN0_HDR2_GMUT_COEF1;
		GMUT_COEF2 = VDIN0_HDR2_GMUT_COEF2;
		GMUT_COEF3 = VDIN0_HDR2_GMUT_COEF3;
		GMUT_COEF4 = VDIN0_HDR2_GMUT_COEF4;

		hdr_ctrl = VDIN0_HDR2_CTRL;
	} else if (module_sel & VDIN1_HDR) {
		MATRIXI_COEF00_01 = VDIN1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF00_01 = VDIN1_HDR2_MATRIXI_COEF00_01;
		MATRIXI_COEF02_10 = VDIN1_HDR2_MATRIXI_COEF02_10;
		MATRIXI_COEF11_12 = VDIN1_HDR2_MATRIXI_COEF11_12;
		MATRIXI_COEF20_21 = VDIN1_HDR2_MATRIXI_COEF20_21;
		MATRIXI_COEF22 = VDIN1_HDR2_MATRIXI_COEF22;
		MATRIXI_COEF30_31 = VDIN1_HDR2_MATRIXI_COEF30_31;
		MATRIXI_COEF32_40 = VDIN1_HDR2_MATRIXI_COEF32_40;
		MATRIXI_COEF41_42 = VDIN1_HDR2_MATRIXI_COEF41_42;
		MATRIXI_OFFSET0_1 = VDIN1_HDR2_MATRIXI_OFFSET0_1;
		MATRIXI_OFFSET2 = VDIN1_HDR2_MATRIXI_OFFSET2;
		MATRIXI_PRE_OFFSET0_1 = VDIN1_HDR2_MATRIXI_PRE_OFFSET0_1;
		MATRIXI_PRE_OFFSET2 = VDIN1_HDR2_MATRIXI_PRE_OFFSET2;
		MATRIXI_CLIP = VDIN1_HDR2_MATRIXI_CLIP;
		MATRIXI_EN_CTRL = VDIN1_HDR2_MATRIXI_EN_CTRL;

		MATRIXO_COEF00_01 = VDIN1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF00_01 = VDIN1_HDR2_MATRIXO_COEF00_01;
		MATRIXO_COEF02_10 = VDIN1_HDR2_MATRIXO_COEF02_10;
		MATRIXO_COEF11_12 = VDIN1_HDR2_MATRIXO_COEF11_12;
		MATRIXO_COEF20_21 = VDIN1_HDR2_MATRIXO_COEF20_21;
		MATRIXO_COEF22 = VDIN1_HDR2_MATRIXO_COEF22;
		MATRIXO_COEF30_31 = VDIN1_HDR2_MATRIXO_COEF30_31;
		MATRIXO_COEF32_40 = VDIN1_HDR2_MATRIXO_COEF32_40;
		MATRIXO_COEF41_42 = VDIN1_HDR2_MATRIXO_COEF41_42;
		MATRIXO_OFFSET0_1 = VDIN1_HDR2_MATRIXO_OFFSET0_1;
		MATRIXO_OFFSET2 = VDIN1_HDR2_MATRIXO_OFFSET2;
		MATRIXO_PRE_OFFSET0_1 = VDIN1_HDR2_MATRIXO_PRE_OFFSET0_1;
		MATRIXO_PRE_OFFSET2 = VDIN1_HDR2_MATRIXO_PRE_OFFSET2;
		MATRIXO_CLIP = VDIN1_HDR2_MATRIXO_CLIP;
		MATRIXO_EN_CTRL = VDIN1_HDR2_MATRIXO_EN_CTRL;

		CGAIN_OFFT = VDIN1_HDR2_CGAIN_OFFT;
		CGAIN_COEF0 = VDIN1_HDR2_CGAIN_COEF0;
		CGAIN_COEF1 = VDIN1_HDR2_CGAIN_COEF1;
		ADPS_CTRL = VDIN1_HDR2_ADPS_CTRL;
		ADPS_ALPHA0 = VDIN1_HDR2_ADPS_ALPHA0;
		ADPS_ALPHA1 = VDIN1_HDR2_ADPS_ALPHA1;
		ADPS_BETA0 = VDIN1_HDR2_ADPS_BETA0;
		ADPS_BETA1 = VDIN1_HDR2_ADPS_BETA1;
		ADPS_BETA2 = VDIN1_HDR2_ADPS_BETA2;
		ADPS_COEF0 = VDIN1_HDR2_ADPS_COEF0;
		ADPS_COEF1 = VDIN1_HDR2_ADPS_COEF1;
		GMUT_CTRL = VDIN1_HDR2_GMUT_CTRL;
		GMUT_COEF0 = VDIN1_HDR2_GMUT_COEF0;
		GMUT_COEF1 = VDIN1_HDR2_GMUT_COEF1;
		GMUT_COEF2 = VDIN1_HDR2_GMUT_COEF2;
		GMUT_COEF3 = VDIN1_HDR2_GMUT_COEF3;
		GMUT_COEF4 = VDIN1_HDR2_GMUT_COEF4;

		hdr_ctrl = VDIN1_HDR2_CTRL;
	}

	WRITE_VPP_REG_BITS(hdr_ctrl, hdr_mtx_param->mtx_on, 13, 1);

	if (mtx_sel & HDR_IN_MTX) {
		if (hdr_mtx_param->mtx_in) {
			for (i = 0; i < 15; i++)
				mtx[i] = hdr_mtx_param->mtx_in[i];
		}
		WRITE_VPP_REG(MATRIXI_EN_CTRL, hdr_mtx_param->mtx_on);
		/*yuv in*/
		WRITE_VPP_REG_BITS(hdr_ctrl, hdr_mtx_param->mtx_on, 4, 1);

		WRITE_VPP_REG_BITS(hdr_ctrl, hdr_mtx_param->mtx_only, 16, 1);
		WRITE_VPP_REG_BITS(hdr_ctrl, 0, 17, 1);
		/*mtx in en*/
		WRITE_VPP_REG_BITS(hdr_ctrl, 1, 14, 1);

		WRITE_VPP_REG(MATRIXI_COEF00_01,
			(mtx[0 * 3 + 0] << 16) | (mtx[0 * 3 + 1] & 0x1FFF));
		WRITE_VPP_REG(MATRIXI_COEF02_10,
			(mtx[0 * 3 + 2] << 16) | (mtx[1 * 3 + 0] & 0x1FFF));
		WRITE_VPP_REG(MATRIXI_COEF11_12,
			(mtx[1 * 3 + 1] << 16) | (mtx[1 * 3 + 2] & 0x1FFF));
		WRITE_VPP_REG(MATRIXI_COEF20_21,
			(mtx[2 * 3 + 0] << 16) | (mtx[2 * 3 + 1] & 0x1FFF));
		WRITE_VPP_REG(MATRIXI_COEF22,
			mtx[2 * 3 + 2]);
		WRITE_VPP_REG(MATRIXI_OFFSET0_1,
			(yuv2rgbpos[0] << 16) | (yuv2rgbpos[1] & 0xFFF));
		WRITE_VPP_REG(MATRIXI_OFFSET2,
			yuv2rgbpos[2]);
		WRITE_VPP_REG(MATRIXI_PRE_OFFSET0_1,
			(yuv2rgbpre[0] << 16)|(yuv2rgbpre[1] & 0xFFF));
		WRITE_VPP_REG(MATRIXI_PRE_OFFSET2,
			yuv2rgbpre[2]);

	} else if (mtx_sel & HDR_GAMUT_MTX) {
		if (hdr_mtx_param->mtx_gamut) {
			for (i = 0; i < 9; i++)
				gmut_coef[i/3][i%3] =
					hdr_mtx_param->mtx_gamut[i];
		}
		gmut_shift = 11;

		if (hdr_mtx_param->mtx_cgain) {
			for (i = 0; i < 3; i++)
				c_gain_lim_coef[i] =
					hdr_mtx_param->mtx_cgain[i] << 2;
		}

		adpscl_mode = 1;/*according to test code*/
		for (i = 0; i < 3; i++) {
			adpscl_enable[i] = 0;
			if (hdr_mtx_param->p_sel & HDR_SDR)
				adpscl_alpha[i] = out_luma *
					(1 << adp_scal_shift) / in_luma;
			else if (hdr_mtx_param->p_sel & SDR_HDR)
				adpscl_alpha[i] = in_luma *
					(1 << adp_scal_shift) / out_luma;
			else if (hdr_mtx_param->p_sel & HDR_BYPASS)
				adpscl_alpha[i] = out_luma *
					(1 << adp_scal_shift) / in_luma;
			adpscl_shift[i] = adp_scal_shift;
			if (hdr_mtx_param->mtx_ogain)
				adpscl_ys_coef[i] =
					hdr_mtx_param->mtx_ogain[i] << 1;
			adpscl_beta_s[i] = 0;
			adpscl_beta[i] = FLTZERO;
		}

		/*gamut mode: 1->gamut before ootf*/
					/*2->gamut after ootf*/
					/*other->disable gamut*/
		WRITE_VPP_REG_BITS(hdr_ctrl, 1, 6, 2);

	    WRITE_VPP_REG(GMUT_CTRL, gmut_shift);
	    WRITE_VPP_REG(GMUT_COEF0,
			gmut_coef[0][1] << 16 | gmut_coef[0][0]);
	    WRITE_VPP_REG(GMUT_COEF1,
			gmut_coef[1][0] << 16 | gmut_coef[0][2]);
	    WRITE_VPP_REG(GMUT_COEF2,
			gmut_coef[1][2] << 16 | gmut_coef[1][1]);
	    WRITE_VPP_REG(GMUT_COEF3,
			gmut_coef[2][1] << 16 | gmut_coef[2][0]);
	    WRITE_VPP_REG(GMUT_COEF4, gmut_coef[2][2]);

	    WRITE_VPP_REG(CGAIN_COEF0,
			c_gain_lim_coef[1] << 16 |
			c_gain_lim_coef[0]);
	    WRITE_VPP_REG(CGAIN_COEF1, c_gain_lim_coef[2]);

	    WRITE_VPP_REG(ADPS_CTRL, adpscl_enable[2] << 6 |
							adpscl_enable[1] << 5 |
							adpscl_enable[0] << 4 |
							adpscl_mode);
		WRITE_VPP_REG(ADPS_ALPHA0,
				adpscl_alpha[1]<<16 | adpscl_alpha[0]);
		WRITE_VPP_REG(ADPS_ALPHA1, adpscl_shift[0] << 24 |
							adpscl_shift[1] << 20 |
							adpscl_shift[2] << 16 |
							adpscl_alpha[2]);
	    WRITE_VPP_REG(ADPS_BETA0,
			adpscl_beta_s[0] << 20 | adpscl_beta[0]);
	    WRITE_VPP_REG(ADPS_BETA1,
			adpscl_beta_s[1] << 20 | adpscl_beta[1]);
	    WRITE_VPP_REG(ADPS_BETA2,
			adpscl_beta_s[2] << 20 | adpscl_beta[2]);
	    WRITE_VPP_REG(ADPS_COEF0,
			adpscl_ys_coef[1] << 16 | adpscl_ys_coef[0]);
	    WRITE_VPP_REG(ADPS_COEF1, adpscl_ys_coef[2]);

	} else if (mtx_sel & HDR_OUT_MTX) {
		if (hdr_mtx_param->mtx_out) {
			for (i = 0; i < 15; i++)
				mtx[i] = hdr_mtx_param->mtx_out[i];
		}

		WRITE_VPP_REG(CGAIN_OFFT,
			(rgb2yuvpos[2] << 16) | rgb2yuvpos[1]);
		WRITE_VPP_REG(MATRIXO_EN_CTRL, hdr_mtx_param->mtx_on);
		/*yuv in*/
		WRITE_VPP_REG_BITS(hdr_ctrl, hdr_mtx_param->mtx_on, 4, 1);

		WRITE_VPP_REG_BITS(hdr_ctrl, hdr_mtx_param->mtx_only, 16, 1);
		WRITE_VPP_REG_BITS(hdr_ctrl, 0, 17, 1);
		/*mtx out en*/
		WRITE_VPP_REG_BITS(hdr_ctrl, 1, 15, 1);

		WRITE_VPP_REG(MATRIXO_COEF00_01,
			(mtx[0 * 3 + 0] << 16) | (mtx[0 * 3 + 1] & 0x1FFF));
		WRITE_VPP_REG(MATRIXO_COEF02_10,
			(mtx[0 * 3 + 2] << 16) | (mtx[1 * 3 + 0] & 0x1FFF));
		WRITE_VPP_REG(MATRIXO_COEF11_12,
			(mtx[1 * 3 + 1] << 16) | (mtx[1 * 3 + 2] & 0x1FFF));
		WRITE_VPP_REG(MATRIXO_COEF20_21,
			(mtx[2 * 3 + 0] << 16) | (mtx[2 * 3 + 1] & 0x1FFF));
		WRITE_VPP_REG(MATRIXO_COEF22,
			mtx[2 * 3 + 2]);
		WRITE_VPP_REG(MATRIXO_OFFSET0_1,
			(rgb2yuvpos[0] << 16) | (rgb2yuvpos[1]&0xFFF));
		WRITE_VPP_REG(MATRIXO_OFFSET2,
			rgb2yuvpos[2]);
		WRITE_VPP_REG(MATRIXO_PRE_OFFSET0_1,
			(rgb2yuvpre[0] << 16)|(rgb2yuvpre[1]&0xFFF));
		WRITE_VPP_REG(MATRIXO_PRE_OFFSET2,
			rgb2yuvpre[2]);
	}

}

void set_eotf_lut(
	enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *hdr_lut_param)
{
	unsigned int lut[HDR2_EOTF_LUT_SIZE];
	unsigned int eotf_lut_addr_port = 0;
	unsigned int eotf_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;

	if (module_sel & VD1_HDR) {
		eotf_lut_addr_port = VD1_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = VD1_EOTF_LUT_DATA_PORT;
		hdr_ctrl = VD1_HDR2_CTRL;
	} else if (module_sel & VD2_HDR) {
		eotf_lut_addr_port = VD2_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = VD2_EOTF_LUT_DATA_PORT;
		hdr_ctrl = VD2_HDR2_CTRL;
	} else if (module_sel & OSD1_HDR) {
		eotf_lut_addr_port = OSD1_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = OSD1_EOTF_LUT_DATA_PORT;
		hdr_ctrl = OSD1_HDR2_CTRL;
	} else if (module_sel & DI_HDR) {
		eotf_lut_addr_port = DI_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = DI_EOTF_LUT_DATA_PORT;
		hdr_ctrl = DI_HDR2_CTRL;
	} else if (module_sel & VDIN0_HDR) {
		eotf_lut_addr_port = VDIN0_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = VDIN0_EOTF_LUT_DATA_PORT;
		hdr_ctrl = VDIN0_HDR2_CTRL;
	} else if (module_sel & VDIN1_HDR) {
		eotf_lut_addr_port = VDIN1_EOTF_LUT_ADDR_PORT;
		eotf_lut_data_port = VDIN1_EOTF_LUT_DATA_PORT;
		hdr_ctrl = VDIN1_HDR2_CTRL;
	}

	for (i = 0; i < HDR2_EOTF_LUT_SIZE; i++)
		lut[i] = hdr_lut_param->eotf_lut[i];

	WRITE_VPP_REG_BITS(hdr_ctrl, hdr_lut_param->lut_on, 3, 1);
	WRITE_VPP_REG(eotf_lut_addr_port, 0x0);
	for (i = 0; i < HDR2_EOTF_LUT_SIZE; i++)
		WRITE_VPP_REG(eotf_lut_data_port, lut[i]);
}

void set_ootf_lut(
	enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *hdr_lut_param)
{
	unsigned int lut[HDR2_OOTF_LUT_SIZE];
	unsigned int ootf_lut_addr_port = 0;
	unsigned int ootf_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;

	if (module_sel & VD1_HDR) {
		ootf_lut_addr_port = VD1_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = VD1_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = VD1_HDR2_CTRL;
	} else if (module_sel & VD2_HDR) {
		ootf_lut_addr_port = VD2_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = VD2_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = VD2_HDR2_CTRL;
	} else if (module_sel & OSD1_HDR) {
		ootf_lut_addr_port = OSD1_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = OSD1_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = OSD1_HDR2_CTRL;
	} else if (module_sel & DI_HDR) {
		ootf_lut_addr_port = DI_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = DI_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = DI_HDR2_CTRL;
	} else if (module_sel & VDIN0_HDR) {
		ootf_lut_addr_port = VDIN0_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = VDIN0_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = VDIN0_HDR2_CTRL;
	} else if (module_sel & VDIN1_HDR) {
		ootf_lut_addr_port = VDIN1_OGAIN_LUT_ADDR_PORT;
		ootf_lut_data_port = VDIN1_OGAIN_LUT_DATA_PORT;
		hdr_ctrl = VDIN1_HDR2_CTRL;
	}

	for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
		lut[i] = hdr_lut_param->ogain_lut[i];

	WRITE_VPP_REG_BITS(hdr_ctrl, hdr_lut_param->lut_on, 1, 1);
	WRITE_VPP_REG(ootf_lut_addr_port, 0x0);

	for (i = 0; i < HDR2_OOTF_LUT_SIZE / 2; i++)
		WRITE_VPP_REG(ootf_lut_data_port,
			(lut[i * 2 + 1] << 16) +
			lut[i * 2]);
	WRITE_VPP_REG(ootf_lut_data_port, lut[148]);
}

void set_oetf_lut(
	enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *hdr_lut_param)
{
	unsigned int lut[HDR2_OETF_LUT_SIZE];
	unsigned int oetf_lut_addr_port = 0;
	unsigned int oetf_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;

	if (module_sel & VD1_HDR) {
		oetf_lut_addr_port = VD1_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = VD1_OETF_LUT_DATA_PORT;
		hdr_ctrl = VD1_HDR2_CTRL;
	} else if (module_sel & VD2_HDR) {
		oetf_lut_addr_port = VD2_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = VD2_OETF_LUT_DATA_PORT;
		hdr_ctrl = VD2_HDR2_CTRL;
	} else if (module_sel & OSD1_HDR) {
		oetf_lut_addr_port = OSD1_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = OSD1_OETF_LUT_DATA_PORT;
		hdr_ctrl = OSD1_HDR2_CTRL;
	} else if (module_sel & DI_HDR) {
		oetf_lut_addr_port = DI_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = DI_OETF_LUT_DATA_PORT;
		hdr_ctrl = DI_HDR2_CTRL;
	} else if (module_sel & VDIN0_HDR) {
		oetf_lut_addr_port = VDIN0_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = VDIN0_OETF_LUT_DATA_PORT;
		hdr_ctrl = VDIN0_HDR2_CTRL;
	} else if (module_sel & VDIN1_HDR) {
		oetf_lut_addr_port = VDIN1_OETF_LUT_ADDR_PORT;
		oetf_lut_data_port = VDIN1_OETF_LUT_DATA_PORT;
		hdr_ctrl = VDIN1_HDR2_CTRL;
	}

	for (i = 0; i < HDR2_OETF_LUT_SIZE; i++)
		lut[i] = hdr_lut_param->oetf_lut[i];

	WRITE_VPP_REG_BITS(hdr_ctrl, hdr_lut_param->lut_on, 2, 1);
	WRITE_VPP_REG(oetf_lut_addr_port, 0x0);
	for (i = 0; i < HDR2_OETF_LUT_SIZE / 2; i++) {
		if (hdr_lut_param->bitdepth == 10)
			WRITE_VPP_REG(oetf_lut_data_port,
				((lut[i * 2 + 1] >> 2) << 16) +
				(lut[i * 2] >> 2));
		else
			WRITE_VPP_REG(oetf_lut_data_port,
				(lut[i * 2 + 1] << 16) +
				lut[i * 2]);
		}
		WRITE_VPP_REG(oetf_lut_data_port, lut[148]);
}

void set_c_gain(
	enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *hdr_lut_param)
{
	unsigned int lut[HDR2_CGAIN_LUT_SIZE];
	unsigned int cgain_lut_addr_port = 0;
	unsigned int cgain_lut_data_port = 0;
	unsigned int hdr_ctrl = 0;
	unsigned int i = 0;

	if (module_sel & VD1_HDR) {
		cgain_lut_addr_port = VD1_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = VD1_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = VD1_HDR2_CTRL;
	} else if (module_sel & VD2_HDR) {
		cgain_lut_addr_port = VD2_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = VD2_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = VD2_HDR2_CTRL;
	} else if (module_sel & OSD1_HDR) {
		cgain_lut_addr_port = OSD1_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = OSD1_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = OSD1_HDR2_CTRL;
	} else if (module_sel & DI_HDR) {
		cgain_lut_addr_port = DI_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = DI_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = DI_HDR2_CTRL;
	} else if (module_sel & VDIN0_HDR) {
		cgain_lut_addr_port = VDIN0_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = VDIN0_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = VDIN0_HDR2_CTRL;
	} else if (module_sel & VDIN1_HDR) {
		cgain_lut_addr_port = VDIN1_CGAIN_LUT_ADDR_PORT;
		cgain_lut_data_port = VDIN1_CGAIN_LUT_DATA_PORT;
		hdr_ctrl = VDIN1_HDR2_CTRL;
	}

	for (i = 0; i < HDR2_CGAIN_LUT_SIZE; i++)
		lut[i] = hdr_lut_param->cgain_lut[i];

	/*cgain mode: 0->y domin*/
	/*cgain mode: 1->rgb domin, use r/g/b max*/
	WRITE_VPP_REG_BITS(hdr_ctrl, hdr_lut_param->lut_on, 12, 1);
	WRITE_VPP_REG_BITS(hdr_ctrl, hdr_lut_param->lut_on, 0, 1);

	WRITE_VPP_REG(cgain_lut_addr_port, 0x0);
	for (i = 0; i < HDR2_CGAIN_LUT_SIZE / 2; i++)
		WRITE_VPP_REG(cgain_lut_data_port,
			(lut[i * 2 + 1] << 16) + lut[i * 2]);
	WRITE_VPP_REG(cgain_lut_data_port, lut[64]);
}

struct hdr_proc_lut_param_s hdr_lut_param;

void hdrbypass_func(enum hdr_module_sel module_sel)
{
	int bit_depth;
	unsigned int i = 0;
	struct hdr_proc_mtx_param_s hdr_mtx_param;

	memset(&hdr_mtx_param, 0, sizeof(struct hdr_proc_mtx_param_s));
	memset(&hdr_lut_param, 0, sizeof(struct hdr_proc_lut_param_s));

	if (module_sel & (VD1_HDR | VD2_HDR | OSD1_HDR))
		bit_depth = 12;
	else if (module_sel & (VDIN0_HDR | VDIN1_HDR | DI_HDR))
		bit_depth = 10;
	else
		return;

#ifdef HDR2_MODULE
	MenuFun fun[] = {pq_eotf, pq_oetf, gm_eotf, gm_oetf, sld_eotf, sld_oetf,
		hlg_eotf, hlg_oetf, ootf_gain, nolinear_cgain, hlg_gain};

	/*lut parameters*/
	eotf_float_gen(hdr_lut_param.eotf_lut, fun[2]);
	oetf_float_gen(hdr_lut_param.oetf_lut, fun[1]);
	oetf_float_gen(hdr_lut_param.ogain_lut, fun[8]);
	nolinear_lut_gen(hdr_lut_param.cgain_lut, fun[9]);
	hdr_lut_param.lut_on = LUT_OFF;
	hdr_lut_param.bitdepth = bit_depth;
#else
	/*lut parameters*/
	for (i = 0; i < HDR2_OETF_LUT_SIZE; i++) {
		hdr_lut_param.oetf_lut[i]  = oetf_lut1[i];
		hdr_lut_param.ogain_lut[i] = ogain_lut1[i];
		if (i < HDR2_EOTF_LUT_SIZE)
			hdr_lut_param.eotf_lut[i] = eotf_lut1[i];
		if (i < HDR2_CGAIN_LUT_SIZE)
			hdr_lut_param.cgain_lut[i] = cgain_lut1[i] - 1;
	}
	hdr_lut_param.lut_on = LUT_OFF;
	hdr_lut_param.bitdepth = bit_depth;
#endif

	/*mtx parameters*/
	hdr_mtx_param.mtx_only = HDR_ONLY;
	for (i = 0; i < 15; i++) {
		hdr_mtx_param.mtx_in[i] = bypass_coeff[i];
		hdr_mtx_param.mtx_cgain[i] = bypass_coeff[i];
		hdr_mtx_param.mtx_ogain[i] = bypass_coeff[i];
		hdr_mtx_param.mtx_out[i] = bypass_coeff[i];
		if (i < 9)
			hdr_mtx_param.mtx_gamut[i] = bypass_coeff[i];
	}
	hdr_mtx_param.mtx_on = MTX_OFF;
	hdr_mtx_param.p_sel = HDR_BYPASS;

	set_hdr_matrix(module_sel, HDR_IN_MTX, &hdr_mtx_param);

	set_eotf_lut(module_sel, &hdr_lut_param);

	set_hdr_matrix(module_sel, HDR_GAMUT_MTX, &hdr_mtx_param);

	set_ootf_lut(module_sel, &hdr_lut_param);

	set_oetf_lut(module_sel, &hdr_lut_param);

	set_hdr_matrix(module_sel, HDR_OUT_MTX, &hdr_mtx_param);

	set_c_gain(module_sel, &hdr_lut_param);
}

void hdr2sdr_func(enum hdr_module_sel module_sel)
{
	unsigned int bit_depth;
	unsigned int i = 0;
	struct hdr_proc_mtx_param_s hdr_mtx_param;

	memset(&hdr_mtx_param, 0, sizeof(struct hdr_proc_mtx_param_s));
	memset(&hdr_lut_param, 0, sizeof(struct hdr_proc_lut_param_s));

	if (module_sel & (VD1_HDR | VD2_HDR | OSD1_HDR))
		bit_depth = 12;
	else if (module_sel & (VDIN0_HDR | VDIN1_HDR | DI_HDR))
		bit_depth = 10;
	else
		return;

#ifdef HDR2_MODULE
	MenuFun fun[] = {pq_eotf, pq_oetf, gm_eotf, gm_oetf, sld_eotf, sld_oetf,
		hlg_eotf, hlg_oetf, ootf_gain, nolinear_cgain, hlg_gain};

	/*lut parameters*/
	eotf_float_gen(hdr_lut_param.eotf_lut, fun[2]);
	oetf_float_gen(hdr_lut_param.oetf_lut, fun[1]);
	oetf_float_gen(hdr_lut_param.ogain_lut, fun[8]);
	nolinear_lut_gen(hdr_lut_param.cgain_lut, fun[9]);
	hdr_lut_param.lut_on = LUT_ON;
	hdr_lut_param.bitdepth = bit_depth;
#else
	/*lut parameters*/
	for (i = 0; i < HDR2_OETF_LUT_SIZE; i++) {
		hdr_lut_param.oetf_lut[i]  = oetf_lut1[i];
		hdr_lut_param.ogain_lut[i] = ogain_lut1[i];
		if (i < HDR2_EOTF_LUT_SIZE)
			hdr_lut_param.eotf_lut[i] = eotf_lut1[i];
		if (i < HDR2_CGAIN_LUT_SIZE)
			hdr_lut_param.cgain_lut[i] = cgain_lut1[i] - 1;
	}
	hdr_lut_param.lut_on = LUT_ON;
	hdr_lut_param.bitdepth = bit_depth;
#endif

	/*mtx parameters*/
	hdr_mtx_param.mtx_only = HDR_ONLY;
	for (i = 0; i < 15; i++) {
		hdr_mtx_param.mtx_in[i] = ycbcr2rgb_ncl2020[i];
		hdr_mtx_param.mtx_cgain[i] = rgb2ycbcr_709[i];
		hdr_mtx_param.mtx_ogain[i] = rgb2ycbcr_ncl2020[i];
		hdr_mtx_param.mtx_out[i] = rgb2ycbcr_709[i];
		if (i < 9)
			hdr_mtx_param.mtx_gamut[i] = ncl_2020_709[i];
	}
	hdr_mtx_param.mtx_on = MTX_ON;
	hdr_mtx_param.p_sel = HDR_SDR;

	set_hdr_matrix(module_sel, HDR_IN_MTX, &hdr_mtx_param);

	set_eotf_lut(module_sel, &hdr_lut_param);

	set_hdr_matrix(module_sel, HDR_GAMUT_MTX, &hdr_mtx_param);

	set_ootf_lut(module_sel, &hdr_lut_param);

	set_oetf_lut(module_sel, &hdr_lut_param);

	set_hdr_matrix(module_sel, HDR_OUT_MTX, &hdr_mtx_param);

	set_c_gain(module_sel, &hdr_lut_param);
}

void sdr2hdr_func(enum hdr_module_sel module_sel)
{
	unsigned int bit_depth;
	unsigned int i = 0;
	struct hdr_proc_mtx_param_s hdr_mtx_param;

	memset(&hdr_mtx_param, 0, sizeof(struct hdr_proc_mtx_param_s));
	memset(&hdr_lut_param, 0, sizeof(struct hdr_proc_lut_param_s));

	if (module_sel & (VD1_HDR | VD2_HDR | OSD1_HDR))
		bit_depth = 12;
	else if (module_sel & (VDIN0_HDR | VDIN1_HDR | DI_HDR))
		bit_depth = 10;
	else
		return;

#ifdef HDR2_MODULE
	MenuFun fun[] = {pq_eotf, pq_oetf, gm_eotf, gm_oetf, sld_eotf, sld_oetf,
		hlg_eotf, hlg_oetf, ootf_gain, nolinear_cgain, hlg_gain};

	eotf_float_gen(hdr_lut_param.eotf_lut, fun[2]);
	oetf_float_gen(hdr_lut_param.oetf_lut, fun[1]);
	oetf_float_gen(hdr_lut_param.ogain_lut, fun[8]);
	nolinear_lut_gen(hdr_lut_param.cgain_lut, fun[9]);
#else
	for (i = 0; i < HDR2_OETF_LUT_SIZE; i++) {
		hdr_lut_param.oetf_lut[i]  = oetf_lut0[i];
		hdr_lut_param.ogain_lut[i] = ogain_lut0[i];
		if (i < HDR2_EOTF_LUT_SIZE)
			hdr_lut_param.eotf_lut[i] = eotf_lut0[i];
		if (i < HDR2_CGAIN_LUT_SIZE)
			hdr_lut_param.cgain_lut[i] = cgain_lut0[i] - 1;
	}
	hdr_lut_param.lut_on = LUT_ON;
	hdr_lut_param.bitdepth = bit_depth;
#endif

	/*mtx parameters*/
	hdr_mtx_param.mtx_only = HDR_ONLY;
	for (i = 0; i < 15; i++) {
		hdr_mtx_param.mtx_in[i] = ycbcr2rgb_709[i];
		hdr_mtx_param.mtx_cgain[i] = rgb2ycbcr_ncl2020[i];
		hdr_mtx_param.mtx_ogain[i] = rgb2ycbcr_709[i];
		hdr_mtx_param.mtx_out[i] = rgb2ycbcr_ncl2020[i];
		if (i < 9)
			hdr_mtx_param.mtx_gamut[i] = ncl_709_2020[i];
	}
	hdr_mtx_param.mtx_on = MTX_ON;
	hdr_mtx_param.p_sel = SDR_HDR;

	set_hdr_matrix(module_sel, HDR_IN_MTX, &hdr_mtx_param);

	set_eotf_lut(module_sel, &hdr_lut_param);

	set_hdr_matrix(module_sel, HDR_GAMUT_MTX, &hdr_mtx_param);

	set_ootf_lut(module_sel, &hdr_lut_param);

	set_oetf_lut(module_sel, &hdr_lut_param);

	set_hdr_matrix(module_sel, HDR_OUT_MTX, &hdr_mtx_param);

	set_c_gain(module_sel, &hdr_lut_param);
}
/*G12A matrix setting*/
void mtx_setting(enum vpp_matrix_e mtx_sel,
	enum mtx_csc_e mtx_csc,
	int mtx_on)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_coef13_14 = 0;
	unsigned int matrix_coef23_24 = 0;
	unsigned int matrix_coef15_25 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;

	if (mtx_sel & VD1_MTX) {
		matrix_coef00_01 = VPP_VD1_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_VD1_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_VD1_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_VD1_MATRIX_COEF20_21;
		matrix_coef22 = VPP_VD1_MATRIX_COEF22;
		matrix_coef13_14 = VPP_VD1_MATRIX_COEF13_14;
		matrix_coef23_24 = VPP_VD1_MATRIX_COEF23_24;
		matrix_coef15_25 = VPP_VD1_MATRIX_COEF15_25;
		matrix_clip = VPP_VD1_MATRIX_CLIP;
		matrix_offset0_1 = VPP_VD1_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_VD1_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_VD1_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_VD1_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_VD1_MATRIX_EN_CTRL;

		WRITE_VPP_REG_BITS(VPP_VD1_MATRIX_EN_CTRL, mtx_on, 0, 1);
	} else if (mtx_sel & POST2_MTX) {
		matrix_coef00_01 = VPP_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST2_MATRIX_COEF22;
		matrix_coef13_14 = VPP_POST2_MATRIX_COEF13_14;
		matrix_coef23_24 = VPP_POST2_MATRIX_COEF23_24;
		matrix_coef15_25 = VPP_POST2_MATRIX_COEF15_25;
		matrix_clip = VPP_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST2_MATRIX_EN_CTRL;

		WRITE_VPP_REG_BITS(VPP_POST2_MATRIX_EN_CTRL, mtx_on, 0, 1);
	} else if (mtx_sel & POST_MTX) {
		matrix_coef00_01 = VPP_POST_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST_MATRIX_COEF22;
		matrix_coef13_14 = VPP_POST_MATRIX_COEF13_14;
		matrix_coef23_24 = VPP_POST_MATRIX_COEF23_24;
		matrix_coef15_25 = VPP_POST_MATRIX_COEF15_25;
		matrix_clip = VPP_POST_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL;

		WRITE_VPP_REG_BITS(VPP_POST_MATRIX_EN_CTRL, mtx_on, 0, 1);
	}

	if (!mtx_on)
		return;

	switch (mtx_csc) {
	case MATRIX_RGB_YUV709:
		WRITE_VPP_REG(matrix_coef00_01, 0x00bb0275);
		WRITE_VPP_REG(matrix_coef02_10, 0x003f1f99);
		WRITE_VPP_REG(matrix_coef11_12, 0x1ea601c2);
		WRITE_VPP_REG(matrix_coef20_21, 0x01c21e67);
		WRITE_VPP_REG(matrix_coef22, 0x00001fd7);
		WRITE_VPP_REG(matrix_offset0_1, 0x00400200);
		WRITE_VPP_REG(matrix_offset2, 0x00000200);
		WRITE_VPP_REG(matrix_pre_offset0_1, 0x0);
		WRITE_VPP_REG(matrix_pre_offset2, 0x0);
		break;
	case MATRIX_YUV709_RGB:
		WRITE_VPP_REG(matrix_coef00_01, 0x04A80000);
		WRITE_VPP_REG(matrix_coef02_10, 0x072C04A8);
		WRITE_VPP_REG(matrix_coef11_12, 0x1F261DDD);
		WRITE_VPP_REG(matrix_coef20_21, 0x04A80876);
		WRITE_VPP_REG(matrix_coef22, 0x0);
		WRITE_VPP_REG(matrix_offset0_1, 0x0);
		WRITE_VPP_REG(matrix_offset2, 0x0);
		WRITE_VPP_REG(matrix_pre_offset0_1, 0x7c00600);
		WRITE_VPP_REG(matrix_pre_offset2, 0x00000600);
		break;
	default:
		break;
	}
}
