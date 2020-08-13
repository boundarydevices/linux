/*
 * Samsung MIPI DSIM Bridge
 *
 * Copyright 2018-2022 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm/unaligned.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/gcd.h>
#include <linux/log2.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <drm/drm_vblank.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/bridge/sec_mipi_dsim.h>
#include <video/videomode.h>
#include <video/mipi_display.h>

/* dsim registers */
#define DSIM_VERSION			0x00
#define DSIM_STATUS			0x04
#define DSIM_RGB_STATUS			0x08
#define DSIM_SWRST			0x0c
#define DSIM_CLKCTRL			0x10
#define DSIM_TIMEOUT			0x14
#define DSIM_CONFIG			0x18
#define DSIM_ESCMODE			0x1c
#define DSIM_MDRESOL			0x20
#define DSIM_MVPORCH			0x24
#define DSIM_MHPORCH			0x28
#define DSIM_MSYNC			0x2c
#define DSIM_SDRESOL			0x30
#define DSIM_INTSRC			0x34
#define DSIM_INTMSK			0x38

/* packet */
#define DSIM_PKTHDR			0x3c
#define DSIM_PAYLOAD			0x40
#define DSIM_RXFIFO			0x44
#define DSIM_FIFOTHLD			0x48
#define DSIM_FIFOCTRL			0x4c
#define DSIM_MEMACCHR			0x50
#define DSIM_MULTI_PKT			0x78

/* pll control */
#define DSIM_PLLCTRL_1G			0x90
#define DSIM_PLLCTRL			0x94
#define DSIM_PLLCTRL1			0x98
#define DSIM_PLLCTRL2			0x9c
#define DSIM_PLLTMR			0xa0

/* dphy */
#define DSIM_PHYTIMING			0xb4
#define DSIM_PHYTIMING1			0xb8
#define DSIM_PHYTIMING2			0xbc

/* reg bit manipulation */
#define REG_MASK(e, s) (((1 << ((e) - (s) + 1)) - 1) << (s))
#define REG_PUT(x, e, s) (((x) << (s)) & REG_MASK(e, s))
#define REG_GET(x, e, s) (((x) & REG_MASK(e, s)) >> (s))

/* register bit fields */
#define STATUS_PLLSTABLE		BIT(31)
#define STATUS_SWRSTRLS			BIT(20)
#define STATUS_TXREADYHSCLK		BIT(10)
#define STATUS_ULPSCLK			BIT(9)
#define STATUS_STOPSTATECLK		BIT(8)
#define STATUS_GET_ULPSDAT(x)		REG_GET(x,  7,  4)
#define STATUS_GET_STOPSTATEDAT(x)	REG_GET(x,  3,  0)

#define RGB_STATUS_CMDMODE_INSEL	BIT(31)
#define RGB_STATUS_GET_RGBSTATE(x)	REG_GET(x, 12,  0)

#define CLKCTRL_TXREQUESTHSCLK		BIT(31)
#define CLKCTRL_DPHY_SEL_1G		BIT(29)
#define CLKCTRL_DPHY_SEL_1P5G		(0x0 << 29)
#define CLKCTRL_ESCCLKEN		BIT(28)
#define CLKCTRL_PLLBYPASS		BIT(27)
#define CLKCTRL_BYTECLKSRC_DPHY_PLL	REG_PUT(0, 26, 25)
#define CLKCTRL_BYTECLKEN		BIT(24)
#define CLKCTRL_SET_LANEESCCLKEN(x)	REG_PUT(x, 23, 19)
#define CLKCTRL_SET_ESCPRESCALER(x)	REG_PUT(x, 15,  0)

#define TIMEOUT_SET_BTAOUT(x)		REG_PUT(x, 23, 16)
#define TIMEOUT_SET_LPDRTOUT(x)		REG_PUT(x, 15,  0)

#define CONFIG_NON_CONTINUOUS_CLOCK_LANE	BIT(31)
#define CONFIG_CLKLANE_STOP_START	BIT(30)
#define CONFIG_MFLUSH_VS		BIT(29)
#define CONFIG_EOT_R03			BIT(28)
#define CONFIG_SYNCINFORM		BIT(27)
#define CONFIG_BURSTMODE		BIT(26)
#define CONFIG_VIDEOMODE		BIT(25)
#define CONFIG_AUTOMODE			BIT(24)
#define CONFIG_HSEDISABLEMODE		BIT(23)
#define CONFIG_HFPDISABLEMODE		BIT(22)
#define CONFIG_HBPDISABLEMODE		BIT(21)
#define CONFIG_HSADISABLEMODE		BIT(20)
#define CONFIG_SET_MAINVC(x)		REG_PUT(x, 19, 18)
#define CONFIG_SET_SUBVC(x)		REG_PUT(x, 17, 16)
#define CONFIG_SET_MAINPIXFORMAT(x)	REG_PUT(x, 14, 12)
#define CONFIG_SET_SUBPIXFORMAT(x)	REG_PUT(x, 10,  8)
#define CONFIG_SET_NUMOFDATLANE(x)	REG_PUT(x,  6,  5)
#define CONFIG_SET_LANEEN(x)		REG_PUT(x,  4,  0)

#define ESCMODE_SET_STOPSTATE_CNT(X)	REG_PUT(x, 31, 21)
#define ESCMODE_FORCESTOPSTATE		BIT(20)
#define ESCMODE_FORCEBTA		BIT(16)
#define ESCMODE_CMDLPDT			BIT(7)
#define ESCMODE_TXLPDT			BIT(6)
#define ESCMODE_TXTRIGGERRST		BIT(5)

#define MDRESOL_MAINSTANDBY		BIT(31)
#define MDRESOL_SET_MAINVRESOL(x)	REG_PUT(x, 27, 16)
#define MDRESOL_SET_MAINHRESOL(x)	REG_PUT(x, 11,  0)

#define MVPORCH_SET_CMDALLOW(x)		REG_PUT(x, 31, 28)
#define MVPORCH_SET_STABLEVFP(x)	REG_PUT(x, 26, 16)
#define MVPORCH_SET_MAINVBP(x)		REG_PUT(x, 10,  0)

#define MHPORCH_SET_MAINHFP(x)		REG_PUT(x, 31, 16)
#define MHPORCH_SET_MAINHBP(x)		REG_PUT(x, 15,  0)

#define MSYNC_SET_MAINVSA(x)		REG_PUT(x, 31, 22)
#define MSYNC_SET_MAINHSA(x)		REG_PUT(x, 15,  0)

#define INTSRC_PLLSTABLE		BIT(31)
#define INTSRC_SWRSTRELEASE		BIT(30)
#define INTSRC_SFRPLFIFOEMPTY		BIT(29)
#define INTSRC_SFRPHFIFOEMPTY		BIT(28)
#define INTSRC_FRAMEDONE		BIT(24)
#define INTSRC_LPDRTOUT			BIT(21)
#define INTSRC_TATOUT			BIT(20)
#define INTSRC_RXDATDONE		BIT(18)
#define INTSRC_RXTE			BIT(17)
#define INTSRC_RXACK			BIT(16)
#define INTSRC_MASK			(INTSRC_PLLSTABLE	|	\
					 INTSRC_SWRSTRELEASE	|	\
					 INTSRC_SFRPLFIFOEMPTY	|	\
					 INTSRC_SFRPHFIFOEMPTY	|	\
					 INTSRC_FRAMEDONE	|	\
					 INTSRC_LPDRTOUT	|	\
					 INTSRC_TATOUT		|	\
					 INTSRC_RXDATDONE	|	\
					 INTSRC_RXTE		|	\
					 INTSRC_RXACK)

#define INTMSK_MSKPLLSTABLE		BIT(31)
#define INTMSK_MSKSWRELEASE		BIT(30)
#define INTMSK_MSKSFRPLFIFOEMPTY	BIT(29)
#define INTMSK_MSKSFRPHFIFOEMPTY	BIT(28)
#define INTMSK_MSKFRAMEDONE		BIT(24)
#define INTMSK_MSKLPDRTOUT		BIT(21)
#define INTMSK_MSKTATOUT		BIT(20)
#define INTMSK_MSKRXDATDONE		BIT(18)
#define INTMSK_MSKRXTE			BIT(17)
#define INTMSK_MSKRXACK			BIT(16)

#define PKTHDR_SET_DATA1(x)		REG_PUT(x, 23, 16)
#define PKTHDR_GET_DATA1(x)		REG_GET(x, 23, 16)
#define PKTHDR_SET_DATA0(x)		REG_PUT(x, 15,  8)
#define PKTHDR_GET_DATA0(x)		REG_GET(x, 15,  8)
#define PKTHDR_GET_WC(x)		REG_GET(x, 23,  8)
#define PKTHDR_SET_DI(x)		REG_PUT(x,  7,  0)
#define PKTHDR_GET_DI(x)		REG_GET(x,  7,  0)
#define PKTHDR_SET_DT(x)		REG_PUT(x,  5,  0)
#define PKTHDR_GET_DT(x)		REG_GET(x,  5,  0)
#define PKTHDR_SET_VC(x)		REG_PUT(x,  7,  6)
#define PKTHDR_GET_VC(x)		REG_GET(x,  7,  6)

#define FIFOCTRL_FULLRX			BIT(25)
#define FIFOCTRL_EMPTYRX		BIT(24)
#define FIFOCTRL_FULLHSFR		BIT(23)
#define FIFOCTRL_EMPTYHSFR		BIT(22)
#define FIFOCTRL_FULLLSFR		BIT(21)
#define FIFOCTRL_EMPTYLSFR		BIT(20)
#define FIFOCTRL_FULLHMAIN		BIT(11)
#define FIFOCTRL_EMPTYHMAIN		BIT(10)
#define FIFOCTRL_FULLLMAIN		BIT(9)
#define FIFOCTRL_EMPTYLMAIN		BIT(8)
#define FIFOCTRL_NINITRX		BIT(4)
#define FIFOCTRL_NINITSFR		BIT(3)
#define FIFOCTRL_NINITI80		BIT(2)
#define FIFOCTRL_NINITSUB		BIT(1)
#define FIFOCTRL_NINITMAIN		BIT(0)

#define PLLCTRL_DPDNSWAP_CLK		BIT(25)
#define PLLCTRL_DPDNSWAP_DAT		BIT(24)
#define PLLCTRL_PLLEN			BIT(23)
#define PLLCTRL_SET_PMS(x)		REG_PUT(x, 19,  1)
   #define PLLCTRL_SET_P(x)		REG_PUT(x, 18, 13)
   #define PLLCTRL_SET_M(x)		REG_PUT(x, 12,  3)
   #define PLLCTRL_SET_S(x)		REG_PUT(x,  2,  0)

#define PHYTIMING_SET_M_TLPXCTL(x)	REG_PUT(x, 15,  8)
#define PHYTIMING_SET_M_THSEXITCTL(x)	REG_PUT(x,  7,  0)

#define PHYTIMING1_SET_M_TCLKPRPRCTL(x)	 REG_PUT(x, 31, 24)
#define PHYTIMING1_SET_M_TCLKZEROCTL(x)	 REG_PUT(x, 23, 16)
#define PHYTIMING1_SET_M_TCLKPOSTCTL(x)	 REG_PUT(x, 15,  8)
#define PHYTIMING1_SET_M_TCLKTRAILCTL(x) REG_PUT(x,  7,  0)

#define PHYTIMING2_SET_M_THSPRPRCTL(x)	REG_PUT(x, 23, 16)
#define PHYTIMING2_SET_M_THSZEROCTL(x)	REG_PUT(x, 15,  8)
#define PHYTIMING2_SET_M_THSTRAILCTL(x)	REG_PUT(x,  7,  0)

#define dsim_read(dsim, reg)		readl(dsim->base + reg)
#define dsim_write(dsim, val, reg)	writel(val, dsim->base + reg)

#define MAX_MAIN_HRESOL		2047
#define MAX_MAIN_VRESOL		2047
#define MAX_SUB_HRESOL		1024
#define MAX_SUB_VRESOL		1024

/* in KHZ */
#define MAX_ESC_CLK_FREQ	20000

/* dsim all irqs index */
#define PLLSTABLE		1
#define SWRSTRELEASE		2
#define SFRPLFIFOEMPTY		3
#define SFRPHFIFOEMPTY		4
#define SYNCOVERRIDE		5
#define BUSTURNOVER		6
#define FRAMEDONE		7
#define LPDRTOUT		8
#define TATOUT			9
#define RXDATDONE		10
#define RXTE			11
#define RXACK			12
#define ERRRXECC		13
#define ERRRXCRC		14
#define ERRESC3			15
#define ERRESC2			16
#define ERRESC1			17
#define ERRESC0			18
#define ERRSYNC3		19
#define ERRSYNC2		20
#define ERRSYNC1		21
#define ERRSYNC0		22
#define ERRCONTROL3		23
#define ERRCONTROL2		24
#define ERRCONTROL1		25
#define ERRCONTROL0		26

#define MIPI_FIFO_TIMEOUT	msecs_to_jiffies(250)

#define MIPI_HFP_PKT_OVERHEAD	6
#define MIPI_HBP_PKT_OVERHEAD	6
#define MIPI_HSA_PKT_OVERHEAD	6

#define to_sec_mipi_dsim(dsi) container_of(dsi, struct sec_mipi_dsim, dsi_host)
#define conn_to_sec_mipi_dsim(conn)		\
	container_of(conn, struct sec_mipi_dsim, connector)

/* used for CEA standard modes */
struct dsim_hblank_par {
	char *name;		/* drm display mode name */
	int vrefresh;
	int hfp_wc;
	int hbp_wc;
	int hsa_wc;
	int lanes;
};

struct dsim_pll_pms {
	uint32_t bit_clk;	/* kHz */
	uint32_t p;
	uint32_t m;
	uint32_t s;
	uint32_t k;
};

struct sec_mipi_dsim {
	struct mipi_dsi_host dsi_host;
	struct drm_connector connector;
	struct drm_encoder *encoder;
	struct drm_bridge *bridge;
	struct drm_bridge *next;
	struct drm_panel *panel;
	struct device *dev;

	void __iomem *base;
	struct clk *clk_cfg;
	struct clk *clk_pllref;
	struct clk *clk_pixel;

	/* kHz clocks */
	uint32_t pix_clk;
	uint32_t bit_clk;
	uint32_t pref_clk;			/* phy ref clock rate in KHz */

	unsigned int lanes;
	unsigned int channel;			/* virtual channel */
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
	const struct dsim_hblank_par *hpar;
	unsigned int c_pms;
	struct dsim_pll_pms pms;
	unsigned long long lp_data_rate;
	unsigned long long hs_data_rate;
	unsigned long ref_clk;
	struct clk_hw	dsi_clk_hw;
	struct clk	*dsi_clk;
	unsigned long	frequency;
	struct videomode vmode;
	bool enabled;
	int clk_pllref_enable;
	int clk_cfg_enable;
	unsigned long def_pix_clk;
	unsigned long byte_clock;
	unsigned long pixelclock;
	uint32_t hsmult;

	struct completion pll_stable;
	struct completion ph_tx_done;
	struct completion pl_tx_done;
	struct completion rx_done;
	const struct sec_mipi_dsim_plat_data *pdata;
};

#define DSIM_HBLANK_PARAM(nm, vf, hfp, hbp, hsa, num)	\
	.name	  = (nm),				\
	.vrefresh = (vf),				\
	.hfp_wc   = (hfp),				\
	.hbp_wc   = (hbp),				\
	.hsa_wc   = (hsa),				\
	.lanes	  = (num)

#define DSIM_PLL_PMS(c, pp, mm, ss)			\
	.bit_clk = (c),					\
	.p = (pp),					\
	.m = (mm),					\
	.s = (ss)

static const struct dsim_hblank_par hblank_4lanes[] = {
	/* {  88, 148, 44 } */
	{ DSIM_HBLANK_PARAM("1920x1080", 60,  60, 105,  27, 4), },
	/* { 528, 148, 44 } */
	{ DSIM_HBLANK_PARAM("1920x1080", 50, 390, 105,  27, 4), },
	/* {  88, 148, 44 } */
	{ DSIM_HBLANK_PARAM("1920x1080", 30,  60, 105,  27, 4), },
	/* { 110, 220, 40 } */
	{ DSIM_HBLANK_PARAM("1280x720" , 60,  78, 159,  24, 4), },
	/* { 440, 220, 40 } */
	{ DSIM_HBLANK_PARAM("1280x720" , 50, 324, 159,  24, 4), },
	/* {  16,  60, 62 } */
	{ DSIM_HBLANK_PARAM("720x480"  , 60,   6,  39,  40, 4), },
	/* {  12,  68, 64 } */
	{ DSIM_HBLANK_PARAM("720x576"  , 50,   3,  45,  42, 4), },
	/* {  16,  48, 96 } */
	{ DSIM_HBLANK_PARAM("640x480"  , 60,   6,  30,  66, 4), },
	{ DSIM_HBLANK_PARAM("1280x800" , 34,  123*3, 5*3,  3, 4), },
	{ DSIM_HBLANK_PARAM("1080x1920", 60,  60, 105,  27, 4), },
};

static const struct dsim_hblank_par hblank_2lanes[] = {
	/* {  88, 148, 44 } */
	{ DSIM_HBLANK_PARAM("1920x1080", 30, 114, 210,  60, 2), },
	/* { 110, 220, 40 } */
	{ DSIM_HBLANK_PARAM("1280x720" , 60, 159, 320,  40, 2), },
	/* { 440, 220, 40 } */
	{ DSIM_HBLANK_PARAM("1280x720" , 50, 654, 320,  40, 2), },
	/* {  16,  60, 62 } */
	{ DSIM_HBLANK_PARAM("720x480"  , 60,  16,  66,  88, 2), },
	/* {  12,  68, 64 } */
	{ DSIM_HBLANK_PARAM("720x576"  , 50,  12,  96,  72, 2), },
	/* {  16,  48, 96 } */
	{ DSIM_HBLANK_PARAM("640x480"  , 60,  18,  66, 138, 2), },
};

/*
 * continued fraction
 *      2  1  2  1  2
 * 0 1  2  3  8 11 30
 * 1 0  1  1  3  4 11
 */
static void get_best_ratio_bigger(unsigned long *pnum, unsigned long *pdenom, unsigned max_n, unsigned max_d)
{
	unsigned long a = *pnum;
	unsigned long b = *pdenom;
	unsigned long c;
	unsigned n0 = 0;
	unsigned n1 = 1;
	unsigned d0 = 1;
	unsigned d1 = 0;
	unsigned _n = 0;
	unsigned _d = 1;
	unsigned whole;

	while (b) {
		whole = a / b;
		/* n0/d0 is the earlier term */
		n0 = n0 + (n1 * whole);
		d0 = d0 + (d1 * whole);

		c = a - (b * whole);
		a = b;
		b = c;

		if (b) {
			/* n1/d1 is the earlier term */
			whole = a / b;
			_n = n1 + (n0 * whole);
			_d = d1 + (d0 * whole);
		} else {
			_n = n0;
			_d = d0;
		}
		pr_debug("%s: cf=%i %d/%d, %d/%d\n", __func__, whole, n0, d0, _n, _d);
		if ((_n > max_n) || (_d > max_d)) {
			unsigned h;

			h = n0;
			if (h) {
				_n = max_n - n1;
				_n /= h;
				if (whole > _n)
					whole = _n;
			}
			h = d0;
			if (h) {
				_d = max_d - d1;
				_d /= h;
				if (whole > _d)
					whole = _d;
			}
			_n = n1 + (n0 * whole);
			_d = d1 + (d0 * whole);
			pr_debug("%s: b=%ld, n=%d of %d, d=%d of %d\n", __func__, b, _n, max_n, _d, max_d);
			if (!_d) {
				/* Don't choose infinite for a bigger ratio */
				_n = n0 + 1;
				_d = d0;
				pr_err("%s: %d/%d is too big\n", __func__, _n, _d);
			}
			break;
		}

		if (!b)
			break;
		n1 = _n;
		d1 = _d;
		c = a - (b * whole);
		a = b;
		b = c;
	}

	*pnum = _n;
	*pdenom = _d;
}

static const struct dsim_hblank_par *sec_mipi_dsim_get_hblank_par(const char *name,
								  int vrefresh,
								  int lanes)
{
	int i, size;
	const struct dsim_hblank_par *hpar, *hblank;

	pr_debug("%s: name=%s, vrefresh=%d, lanes=%d\n", __func__, name, vrefresh, lanes);
	if (unlikely(!name))
		return NULL;

	switch (lanes) {
	case 1:
		return NULL;
	case 2:
		hblank = hblank_2lanes;
		size   = ARRAY_SIZE(hblank_2lanes);
		break;
	case 4:
		hblank = hblank_4lanes;
		size   = ARRAY_SIZE(hblank_4lanes);
		break;
	default:
		pr_err("No hblank data for mode %s with %d lanes\n",
		       name, lanes);
		return NULL;
	}

	for (i = 0; i < size; i++) {
		hpar = &hblank[i];

		if (!strcmp(name, hpar->name)) {
			if (vrefresh != hpar->vrefresh)
				continue;

			/* found */
			return hpar;
		}
	}

	return NULL;
}

static int _sec_mipi_dsim_pll_enable(struct sec_mipi_dsim *dsim, int enable)
{
	if (dsim->clk_pllref_enable == enable)
		return 0;

	if (enable)
		clk_prepare_enable(dsim->clk_pllref);
	else
		clk_disable_unprepare(dsim->clk_pllref);
	dsim->clk_pllref_enable = enable;
	return 1;
}

#define MIN_FREQ	6000000

unsigned fixup_div(unsigned long clk, unsigned div)
{
	unsigned max = clk / MIN_FREQ;

	while ((div >= max * 7 / 2) && !(div % 7))
		div /= 7;
	while ((div >= max * 5 / 2) && !(div % 5))
		div /= 5;
	while ((div >= max * 3 / 2) && !(div % 3))
		div /= 3;
	while ((div >= max * 2 / 2) && !(div % 2))
		div >>= 1;

	while (div > max) {
		if (!(div % 2))
			div >>= 1;
		else if (!(div % 3))
			div /=  3;
		else if (!(div % 5))
			div /= 5;
		else if (!(div % 7))
			div /= 7;
		else
			div = max;
	}
	return div;
}

static int sec_mipi_choose_ref_clk(struct sec_mipi_dsim *dsim, unsigned long pix_clk)
{
	unsigned long ref_clk, bit_clk;
	struct clk *clk_ref_parent;
	unsigned bpp = mipi_dsi_pixel_format_to_bpp(dsim->format);
	unsigned div = 0;
	unsigned ref_parent_clk;

	if (bpp < 0)
		return -EINVAL;

	bit_clk = DIV_ROUND_UP_ULL((u64)pix_clk * bpp, dsim->lanes);
	if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_MBC) {
		unsigned n = (bpp + (dsim->lanes * 8) - 1) / (dsim->lanes * 8);
		unsigned bit_clkm = pix_clk * n * 8;

		if (bit_clk < bit_clkm) {
			bit_clk = bit_clkm;
			pr_info("%s: %ld = %ld * 8 * %d(MBC)\n", __func__, bit_clk, pix_clk, n);
		}
	}
	clk_ref_parent = clk_get_parent(dsim->clk_pllref);
	ref_parent_clk = ref_clk = bit_clk;
	if (clk_ref_parent) {
		ref_parent_clk = clk_get_rate(clk_ref_parent);

		if (ref_parent_clk)
			div = (ref_parent_clk + (pix_clk >> 1)) / pix_clk;

		pr_debug("%s: ref_clk=%ld, ref_parent_clk=%d, pix_clk=%ld, div=%d\n",
			__func__, ref_clk, ref_parent_clk, pix_clk, div);
	}
	pr_debug("%s: %ld = %d/%d\n", __func__, ref_clk, ref_parent_clk, div);
	if (div) {
		div = fixup_div(ref_parent_clk, div);
		ref_clk = (ref_parent_clk + (div >> 1)) / div;
		pr_debug("%s: %ld = %d/%d\n", __func__, ref_clk, ref_parent_clk, div);
	} else {
		while (ref_clk < MIN_FREQ)
			ref_clk <<= 1;
	}
	while (ref_clk > 6000000 * 33) {
		ref_clk >>= 1;
	}
	return ref_clk;
}

#define PHY_REF_CLK		12000

static int sec_mipi_dsim_set_pref_rate(struct sec_mipi_dsim *dsim, unsigned long pix_clk)
{
	int ret;
	uint32_t rate, diff;
	uint32_t ref_clk;
	struct device *dev = dsim->dev;
	const struct sec_mipi_dsim_plat_data *pdata = dsim->pdata;
	const struct sec_mipi_dsim_pll *dpll = pdata->dphy_pll;
	const struct sec_mipi_dsim_range *fin_range = &dpll->fin;

	ret = of_property_read_u32(dev->of_node, "pref-rate", &rate);
	if (ret < 0) {
		dev_dbg(dev, "no valid rate assigned for pref clock\n");
		ref_clk = sec_mipi_choose_ref_clk(dsim, pix_clk);
	} else {
		if (unlikely(rate < fin_range->min || rate > fin_range->max)) {
			dev_warn(dev, "pref-rate get is invalid: %uKHz\n",
				 rate);
			ref_clk = PHY_REF_CLK * 1000;
		} else
			ref_clk = rate * 1000;
	}

	while (1) {
		if (dsim->clk_pllref_enable) {
			_sec_mipi_dsim_pll_enable(dsim, 0);
			ret = clk_set_rate(dsim->clk_pllref, ref_clk);
			_sec_mipi_dsim_pll_enable(dsim, 1);
		} else {
			ret = clk_set_rate(dsim->clk_pllref, ref_clk);
		}
		if (ret)
			dev_err(dev, "failed to set pll ref clock rate %d\n", ret);

		rate = clk_get_rate(dsim->clk_pllref);
		pr_debug("%s: ref_clk=%d %d\n", __func__, ref_clk, rate);
		if (unlikely(!rate)) {
			dev_err(dev, "failed to get pll ref clock rate\n");
			return -EINVAL;
		}

		diff = (rate > ref_clk) ? rate - ref_clk : ref_clk - rate;
		if (diff <= 500) {
			dsim->pref_clk = rate / 1000;
			dsim->ref_clk = rate;
			return 0;

		}
		if (unlikely(ref_clk == PHY_REF_CLK * 1000))
			break;

		dev_warn(dev, "invalid assigned rate for pref: %uKHz\n",
			 dsim->pref_clk);
		dev_warn(dev, "use default pref rate instead: %uKHz\n",
			 PHY_REF_CLK);

		ref_clk = PHY_REF_CLK * 1000;
	}
	/* set default rate failed */
	dev_err(dev, "no valid pll ref clock rate\n");
	return -EINVAL;
}

static void sec_mipi_dsim_irq_init(struct sec_mipi_dsim *dsim);

/* For now, dsim only support one device attached */
static int sec_mipi_dsim_host_attach(struct mipi_dsi_host *host,
				     struct mipi_dsi_device *dsi)
{
	struct sec_mipi_dsim *dsim = to_sec_mipi_dsim(host);
	const struct sec_mipi_dsim_plat_data *pdata = dsim->pdata;
	struct device *dev = dsim->dev;
	struct drm_panel *panel;

	if (!dsi->lanes || dsi->lanes > pdata->max_data_lanes) {
		dev_err(dev, "invalid data lanes number\n");
		return -EINVAL;
	}

	if (dsim->channel)
		return -EINVAL;

	if (dsi->format != MIPI_DSI_FMT_RGB888 &&
	    dsi->format != MIPI_DSI_FMT_RGB565 &&
	    dsi->format != MIPI_DSI_FMT_RGB666 &&
	    dsi->format != MIPI_DSI_FMT_RGB666_PACKED) {
		dev_err(dev, "unsupported pixel format: %#x\n", dsi->format);
		return -EINVAL;
	}

	if (!dsim->next) {
		/* 'dsi' must be panel device */
		panel = of_drm_find_panel(dsi->dev.of_node);

		if (!panel) {
			dev_err(dev, "refuse unknown dsi device attach\n");
			WARN_ON(!panel);
			return -ENODEV;
		}

		/* Don't support multiple panels */
		if (dsim->panel && panel && dsim->panel != panel) {
			dev_err(dev, "don't support multiple panels\n");
			return -EBUSY;
		}

		dsim->panel = panel;
	}

	/* TODO: DSIM 3 lanes has some display issue, so
	 * avoid 3 lanes enable, and force data lanes to
	 * be 2.
	 */
	if (dsi->lanes == 3)
		dsi->lanes = 2;

	dsim->lanes	 = dsi->lanes;
	dsim->channel	 = dsi->channel;
	dsim->format	 = dsi->format;
	dsim->mode_flags = dsi->mode_flags;

	/* TODO: support later */
#if 0
	if (dsim->connector.dev)
		drm_helper_hpd_irq_event(dsim->connector.dev);
#endif

	return 0;
}

static int sec_mipi_dsim_host_detach(struct mipi_dsi_host *host,
				     struct mipi_dsi_device *dsi)
{
	struct sec_mipi_dsim *dsim = to_sec_mipi_dsim(host);

	if (WARN_ON(!dsim->next && !dsim->panel))
		return -ENODEV;

	/* clear the saved dsi parameters */
	dsim->lanes	 = 0;
	dsim->channel	 = 0;
	dsim->format	 = 0;
	dsim->mode_flags = 0;

	/* detached panel should be NULL */
	dsim->panel = NULL;

	return 0;
}

static void sec_mipi_dsim_config_cmd_lpm(struct sec_mipi_dsim *dsim,
					 bool enable)
{
	uint32_t escmode;

	escmode = dsim_read(dsim, DSIM_ESCMODE);

	if (enable)
		escmode |= ESCMODE_CMDLPDT;
	else
		escmode &= ~ESCMODE_CMDLPDT;

	dsim_write(dsim, escmode, DSIM_ESCMODE);
}

static void sec_mipi_dsim_write_pl_to_sfr_fifo(struct sec_mipi_dsim *dsim,
					       const void *payload,
					       size_t length)
{
	uint32_t pl_data;

	if (!length)
		return;

	while (length >= 4) {
		pl_data = get_unaligned_le32(payload);
		dsim_write(dsim, pl_data, DSIM_PAYLOAD);
		payload += 4;
		length -= 4;
	}

	pl_data = 0;
	switch (length) {
	case 3:
		pl_data |= ((u8 *)payload)[2] << 16;
		fallthrough;
	case 2:
		pl_data |= ((u8 *)payload)[1] << 8;
		fallthrough;
	case 1:
		pl_data |= ((u8 *)payload)[0];
		dsim_write(dsim, pl_data, DSIM_PAYLOAD);
		break;
	}
}

static void sec_mipi_dsim_write_ph_to_sfr_fifo(struct sec_mipi_dsim *dsim,
					       void *header,
					       bool use_lpm)
{
	uint32_t pkthdr;

	pkthdr = PKTHDR_SET_DATA1(((u8 *)header)[2])	| /* WC MSB  */
		 PKTHDR_SET_DATA0(((u8 *)header)[1])	| /* WC LSB  */
		 PKTHDR_SET_DI(((u8 *)header)[0]);	  /* Data ID */

	dsim_write(dsim, pkthdr, DSIM_PKTHDR);
}

static int sec_mipi_dsim_read_pl_from_sfr_fifo(struct sec_mipi_dsim *dsim,
					       void *payload,
					       size_t length)
{
	uint8_t data_type;
	uint16_t word_count = 0;
	uint32_t fifoctrl, ph, pl;
	int extra;
	unsigned char *dst = payload;

	fifoctrl = dsim_read(dsim, DSIM_FIFOCTRL);

	if (WARN_ON(fifoctrl & FIFOCTRL_EMPTYRX))
		return -EINVAL;

	ph = dsim_read(dsim, DSIM_RXFIFO);
	data_type = PKTHDR_GET_DT(ph);
	switch (data_type) {
	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		dev_err(dsim->dev, "peripheral report error: (0-7)%x, (8-15)%x 0x%08x\n",
			PKTHDR_GET_DATA0(ph), PKTHDR_GET_DATA1(ph), ph);
		return -EPROTO;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
		if (!WARN_ON(length < 2)) {
			dst[1] = PKTHDR_GET_DATA1(ph);
			word_count++;
		}
		fallthrough;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
		dst[0] = PKTHDR_GET_DATA0(ph);
		word_count++;
		length = word_count;
		break;
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
		word_count = PKTHDR_GET_WC(ph);
		extra = 0;
		if (word_count > length) {
			extra = ((word_count + 3) >> 2) - ((length + 3) >> 2);
			dev_err(dsim->dev, "invalid receive buffer length, %d vs %ld, 0x%08x\n", word_count, length, ph);
			word_count = length;
			length = -EINVAL;
		} else {
			length = word_count;
		}

		while (word_count >= 4) {
			pl = dsim_read(dsim, DSIM_RXFIFO);
			dst[0] = pl & 0xff;
			dst[1] = (pl >> 8)  & 0xff;
			dst[2] = (pl >> 16) & 0xff;
			dst[3] = (pl >> 24) & 0xff;
			dst += 4;
			word_count -= 4;
		}

		if (word_count > 0) {
			pl = dsim_read(dsim, DSIM_RXFIFO);

			switch (word_count) {
			case 3:
				dst[2] = (pl >> 16) & 0xff;
				fallthrough;
			case 2:
				dst[1] = (pl >> 8) & 0xff;
				fallthrough;
			case 1:
				dst[0] = pl & 0xff;
				break;
			}
		}
		while (extra) {
			pl = dsim_read(dsim, DSIM_RXFIFO);
			dev_err(dsim->dev, "extra, 0x%x\n", pl);
			extra--;
		}

		break;
	default:
		return -EINVAL;
	}

	return length;
}

static ssize_t sec_mipi_dsim_host_transfer(struct mipi_dsi_host *host,
					   const struct mipi_dsi_msg *msg)
{
	int ret;
	ssize_t bytes = 0;
	bool use_lpm;
	struct mipi_dsi_packet packet;
	struct sec_mipi_dsim *dsim = to_sec_mipi_dsim(host);

	if ((msg->rx_buf && !msg->rx_len) || (msg->rx_len && !msg->rx_buf))
		return -EINVAL;

	ret = mipi_dsi_create_packet(&packet, msg);
	if (ret) {
		dev_err(dsim->dev, "failed to create dsi packet: %d\n", ret);
		return ret;
	}

	/* need to read data from peripheral */
	if (unlikely(msg->rx_buf))
		reinit_completion(&dsim->rx_done);

	/* config LPM for CMD TX */
	use_lpm = msg->flags & MIPI_DSI_MSG_USE_LPM ? true : false;
	sec_mipi_dsim_config_cmd_lpm(dsim, use_lpm);

	if (packet.payload_length) {		/* Long Packet case */
		reinit_completion(&dsim->pl_tx_done);

		/* write packet payload */
		sec_mipi_dsim_write_pl_to_sfr_fifo(dsim,
						   packet.payload,
						   packet.payload_length);

		/* write packet header */
		sec_mipi_dsim_write_ph_to_sfr_fifo(dsim,
						   packet.header,
						   use_lpm);

		ret = wait_for_completion_timeout(&dsim->pl_tx_done,
						  MIPI_FIFO_TIMEOUT);
		if (!ret) {
			dev_err(dsim->dev, "wait payload tx done time out\n");
			return -EBUSY;
		}
	} else {
		reinit_completion(&dsim->ph_tx_done);

		/* write packet header */
		sec_mipi_dsim_write_ph_to_sfr_fifo(dsim,
						   packet.header,
						   use_lpm);

		ret = wait_for_completion_timeout(&dsim->ph_tx_done,
						  MIPI_FIFO_TIMEOUT);
		if (!ret) {
			dev_err(dsim->dev, "wait pkthdr tx done time out\n");
			return -EBUSY;
		}
	}

	/* read packet payload */
	if (unlikely(msg->rx_buf)) {
		ret = wait_for_completion_timeout(&dsim->rx_done,
						  MIPI_FIFO_TIMEOUT);
		if (!ret) {
			dev_err(dsim->dev, "wait rx done time out\n");
			return -EBUSY;
		}

		ret = sec_mipi_dsim_read_pl_from_sfr_fifo(dsim,
							  msg->rx_buf,
							  msg->rx_len);
		if (ret < 0)
			return ret;

		bytes = msg->rx_len;
	} else {
		bytes = packet.size;
	}

	return bytes;
}

static const struct mipi_dsi_host_ops sec_mipi_dsim_host_ops = {
	.attach   = sec_mipi_dsim_host_attach,
	.detach   = sec_mipi_dsim_host_detach,
	.transfer = sec_mipi_dsim_host_transfer,
};

static int sec_mipi_dsim_bridge_attach(struct drm_bridge *bridge,
					enum drm_bridge_attach_flags flags)
{
	int ret;
	bool attach_bridge = false;
	struct sec_mipi_dsim *dsim = bridge->driver_private;
	struct device *dev = dsim->dev;
	struct device_node *np = dev->of_node;
	struct device_node *endpoint, *remote = NULL;
	struct drm_bridge *next = ERR_PTR(-ENODEV);
	struct drm_encoder *encoder = dsim->encoder;

	/* TODO: All bridges and planes should have already been added */

	/* A panel has been found, ignore other dsi devices */
	if (dsim->panel)
		return 0;

	/* find next bridge */
	endpoint = of_graph_get_next_endpoint(np, NULL);
	/* At least one endpoint should be existed */
	if (!endpoint)
		return -ENODEV;

	while (endpoint) {
		/* check the endpoint can attach bridge or not */
		attach_bridge = of_property_read_bool(endpoint, "attach-bridge");
		if (!attach_bridge) {
			endpoint = of_graph_get_next_endpoint(np, endpoint);
			continue;
		}

		remote = of_graph_get_remote_port_parent(endpoint);

		if (!remote || !of_device_is_available(remote)) {
			of_node_put(remote);
			endpoint = of_graph_get_next_endpoint(np, endpoint);
			continue;
		}

		next = of_drm_find_bridge(remote);
		if (next) {
			/* Found */
			of_node_put(endpoint);
			break;
		}

		endpoint = of_graph_get_next_endpoint(np, endpoint);
	}

	/* No workable bridge exists */
	if (IS_ERR(next))
		return PTR_ERR(next);

	/* For the panel driver loading is after dsim bridge,
	 * defer bridge binding to wait for panel driver ready.
	 * The disadvantage of probe defer is endless probing
	 * in some cases.
	 */
	if (!next)
		return -EPROBE_DEFER;

	/* duplicate bridges or next bridge exists */
	WARN_ON(bridge == next || drm_bridge_get_next_bridge(bridge) || dsim->next);

	dsim->next = next;
	next->encoder = encoder;
	ret = drm_bridge_attach(encoder, next, bridge, flags);
	if (ret) {
		dev_err(dev, "Unable to attach bridge %s: %d\n",
			remote->name, ret);
		dsim->next = NULL;
		return ret;
	}

	return 0;
}

static int sec_mipi_dsim_config_pll(struct sec_mipi_dsim *dsim)
{
	int ret;
	uint32_t pllctrl = 0, status, data_lanes_en, stop;

	dsim_write(dsim, 0x8000, DSIM_PLLTMR);

	/* TODO: config dp/dn swap if requires */

	pllctrl |= PLLCTRL_SET_PMS(dsim->c_pms) | PLLCTRL_PLLEN;
	dsim_write(dsim, pllctrl, DSIM_PLLCTRL);

	ret = wait_for_completion_timeout(&dsim->pll_stable, HZ / 10);
	if (!ret) {
		dev_err(dsim->dev, "wait for pll stable time out\n");
		return -EBUSY;
	}

	/* wait for clk & data lanes to go to stop state */
	mdelay(1);

	data_lanes_en = (0x1 << dsim->lanes) - 1;
	status = dsim_read(dsim, DSIM_STATUS);
	if (!(status & STATUS_STOPSTATECLK)) {
		dev_err(dsim->dev, "clock is not in stop state\n");
		return -EBUSY;
	}

	stop = STATUS_GET_STOPSTATEDAT(status);
	if ((stop & data_lanes_en) != data_lanes_en) {
		dev_err(dsim->dev,
			"one or more data lanes is not in stop state\n");
		return -EBUSY;
	}

	return 0;
}

static unsigned pix_to_delay_byte_clocks(struct sec_mipi_dsim *dsim, unsigned pixels, int base, int min,
		unsigned *pix_cnt, unsigned *hs_clk_cnt)
{
	unsigned n;
	unsigned long a;

	*pix_cnt += pixels;
	a = *pix_cnt;
	a *= dsim->byte_clock;
	a += (dsim->pixelclock >> 1);
	a /= dsim->pixelclock;
	n = a;
	pr_info("%s:pix_cnt = %d, byte_clock = %ld, pixelclock = %ld, n=%d\n", __func__, *pix_cnt, dsim->byte_clock, dsim->pixelclock, n);

//	n *= dsim->lanes;
	n -= *hs_clk_cnt;

	if (n >= base + min)
		n -= base;
	else
		n = min;
	*hs_clk_cnt += n + base;

	return n;
}

static unsigned pix_to_delay_byte_clocks_burst(struct sec_mipi_dsim *dsim, unsigned pixels, unsigned bpp,
		unsigned *pix_cnt, unsigned *hs_clk_cnt)
{
	unsigned n;

	*pix_cnt += pixels;
	n = ((pixels * bpp) + (dsim->lanes * 8) - 1) / (dsim->lanes * 8);
//	n *= dsim->lanes;
	*hs_clk_cnt += n;

	return n;
}

static void sec_mipi_dsim_set_main_mode(struct sec_mipi_dsim *dsim)
{
	uint32_t bpp, hfp_wc, hbp_wc, hsa_wc;
	uint32_t mdresol = 0, mvporch = 0, mhporch = 0, msync = 0;
	struct videomode *vmode = &dsim->vmode;
	unsigned pix_cnt = 0;
	unsigned hs_clk_cnt = 0;

	pr_debug("%s: hfp=%d hbp=%d hsync=%d byte_clock=%ld pixelclock=%ld\n", __func__,
			vmode->hfront_porch, vmode->hback_porch, vmode->hsync_len,
			dsim->byte_clock, dsim->pixelclock);
	mdresol |= MDRESOL_SET_MAINVRESOL(vmode->vactive) |
		   MDRESOL_SET_MAINHRESOL(vmode->hactive);
	dsim_write(dsim, mdresol, DSIM_MDRESOL);

	mvporch |= MVPORCH_SET_MAINVBP(vmode->vback_porch)    |
		   MVPORCH_SET_STABLEVFP(vmode->vfront_porch) |
		   MVPORCH_SET_CMDALLOW(0xf);
	dsim_write(dsim, mvporch, DSIM_MVPORCH);

	bpp = mipi_dsi_pixel_format_to_bpp(dsim->format);

	/* calculate hfp & hbp word counts */
	if (dsim->panel || !dsim->hpar) {
		hbp_wc = pix_to_delay_byte_clocks(dsim, vmode->hback_porch, (dsim->lanes <= 1) ? 13 : 7, 1, &pix_cnt, &hs_clk_cnt);
		pix_to_delay_byte_clocks_burst(dsim, vmode->hactive, bpp, &pix_cnt, &hs_clk_cnt);
		hfp_wc = pix_to_delay_byte_clocks(dsim, vmode->hfront_porch, (dsim->lanes <= 1) ? 11 : 4, 0, &pix_cnt, &hs_clk_cnt);
		hsa_wc = pix_to_delay_byte_clocks(dsim, vmode->hsync_len, 10/dsim->lanes, 0, &pix_cnt, &hs_clk_cnt);
	} else {
		hbp_wc = dsim->hpar->hbp_wc;
		hfp_wc = dsim->hpar->hfp_wc;
		hsa_wc = dsim->hpar->hsa_wc;
	}

	mhporch |= MHPORCH_SET_MAINHFP(hfp_wc) |
		   MHPORCH_SET_MAINHBP(hbp_wc);

	dsim_write(dsim, mhporch, DSIM_MHPORCH);

	msync |= MSYNC_SET_MAINVSA(vmode->vsync_len) |
		 MSYNC_SET_MAINHSA(hsa_wc);

	dsim_write(dsim, msync, DSIM_MSYNC);
}

static void sec_mipi_dsim_config_dpi(struct sec_mipi_dsim *dsim)
{
	uint32_t config = 0, rgb_status = 0, data_lanes_en;

	if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO)
		rgb_status &= ~RGB_STATUS_CMDMODE_INSEL;
	else
		rgb_status |= RGB_STATUS_CMDMODE_INSEL;

	dsim_write(dsim, rgb_status, DSIM_RGB_STATUS);

	if (dsim->mode_flags & MIPI_DSI_CLOCK_NON_CONTINUOUS) {
		config |= CONFIG_NON_CONTINUOUS_CLOCK_LANE;
		if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO)
			config |= CONFIG_CLKLANE_STOP_START;
	}

	if (dsim->mode_flags & MIPI_DSI_MODE_VSYNC_FLUSH)
		config |= CONFIG_MFLUSH_VS;

	/* disable EoT packets in HS mode */
	if (dsim->mode_flags & MIPI_DSI_MODE_NO_EOT_PACKET)
		config |= CONFIG_EOT_R03;

	if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO) {
		config |= CONFIG_VIDEOMODE;

		if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
			config |= CONFIG_BURSTMODE;

		else if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			config |= CONFIG_SYNCINFORM;

		if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_AUTO_VERT)
			config |= CONFIG_AUTOMODE;

		if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_HSE)
			config |= CONFIG_HSEDISABLEMODE;

		if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_NO_HFP)
			config |= CONFIG_HFPDISABLEMODE;

		if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_NO_HBP)
			config |= CONFIG_HBPDISABLEMODE;

		if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_NO_HSA)
			config |= CONFIG_HSADISABLEMODE;
	}

	config |= CONFIG_SET_MAINVC(dsim->channel);

	switch (dsim->format) {
	case MIPI_DSI_FMT_RGB565:
		config |= dsim->mode_flags & MIPI_DSI_MODE_VIDEO ?
			  CONFIG_SET_MAINPIXFORMAT(0x4) :
			  CONFIG_SET_MAINPIXFORMAT(0x3);
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO)
			config |= CONFIG_SET_MAINPIXFORMAT(0x5);
		break;
	case MIPI_DSI_FMT_RGB666:
		config |= CONFIG_SET_MAINPIXFORMAT(0x6);
		break;
	case MIPI_DSI_FMT_RGB888:
		config |= CONFIG_SET_MAINPIXFORMAT(0x7);
		break;
	default:
		config |= CONFIG_SET_MAINPIXFORMAT(0x7);
		break;
	}

	/* config data lanes number and enable lanes */
	data_lanes_en = (0x1 << dsim->lanes) - 1;
	config |= CONFIG_SET_NUMOFDATLANE(dsim->lanes - 1);
	config |= CONFIG_SET_LANEEN(0x1 | data_lanes_en << 1);

	dsim_write(dsim, config, DSIM_CONFIG);
}

static void sec_mipi_dsim_config_dphy(struct sec_mipi_dsim *dsim)
{
	struct sec_mipi_dsim_dphy_timing key = { 0 };
	const struct sec_mipi_dsim_dphy_timing *match = NULL;
	const struct sec_mipi_dsim_plat_data *pdata = dsim->pdata;
	uint32_t phytiming = 0, phytiming1 = 0, phytiming2 = 0, timeout = 0;
	uint32_t hactive, vactive;
	struct videomode *vmode = &dsim->vmode;
	struct drm_display_mode mode;

	key.bit_clk = DIV_ROUND_CLOSEST_ULL(dsim->bit_clk, 1000);

	/* '1280x720@60Hz' mode with 2 data lanes
	 * requires special fine tuning for DPHY
	 * TIMING config according to the tests.
	 */
	if (dsim->lanes == 2) {
		hactive = vmode->hactive;
		vactive = vmode->vactive;

		if (hactive == 1280 && vactive == 720) {
			memset(&mode, 0x0, sizeof(mode));
			drm_display_mode_from_videomode(vmode, &mode);

			if (drm_mode_vrefresh(&mode) == 60)
				key.bit_clk >>= 1;
		}
	}

	match = bsearch(&key, pdata->dphy_timing, pdata->num_dphy_timing,
			sizeof(struct sec_mipi_dsim_dphy_timing),
			pdata->dphy_timing_cmp);
	if (WARN_ON(!match))
		return;

	pr_info("%s:bit_clk=%d, prepare=%d zero=%d post=%d trail=%d"
		" hs_prepare=%d hs_zero=%d hs_trail=%d lpx=%d hs_exit=%d\n",
		__func__, match->bit_clk, match->clk_prepare, match->clk_zero,
		match->clk_post, match->clk_trail,
		match->hs_prepare, match->hs_zero, match->hs_trail,
		match->lpx, match->hs_exit);
	phytiming  |= PHYTIMING_SET_M_TLPXCTL(match->lpx)	|
		      PHYTIMING_SET_M_THSEXITCTL(match->hs_exit);
	dsim_write(dsim, phytiming, DSIM_PHYTIMING);

	phytiming1 |= PHYTIMING1_SET_M_TCLKPRPRCTL(match->clk_prepare)	|
		      PHYTIMING1_SET_M_TCLKZEROCTL(match->clk_zero)	|
		      PHYTIMING1_SET_M_TCLKPOSTCTL(match->clk_post)	|
		      PHYTIMING1_SET_M_TCLKTRAILCTL(match->clk_trail);
	dsim_write(dsim, phytiming1, DSIM_PHYTIMING1);

	phytiming2 |= PHYTIMING2_SET_M_THSPRPRCTL(match->hs_prepare)	|
		      PHYTIMING2_SET_M_THSZEROCTL(match->hs_zero)	|
		      PHYTIMING2_SET_M_THSTRAILCTL(match->hs_trail);
	dsim_write(dsim, phytiming2, DSIM_PHYTIMING2);

	timeout |= TIMEOUT_SET_BTAOUT(0xff)	|
		   TIMEOUT_SET_LPDRTOUT(0xff);
	dsim_write(dsim, timeout, DSIM_TIMEOUT);
}

static void sec_mipi_dsim_init_fifo_pointers(struct sec_mipi_dsim *dsim)
{
	uint32_t fifoctrl, fifo_ptrs;

	fifoctrl = dsim_read(dsim, DSIM_FIFOCTRL);

	fifo_ptrs = FIFOCTRL_NINITRX	|
		    FIFOCTRL_NINITSFR	|
		    FIFOCTRL_NINITI80	|
		    FIFOCTRL_NINITSUB	|
		    FIFOCTRL_NINITMAIN;

	fifoctrl &= ~fifo_ptrs;
	dsim_write(dsim, fifoctrl, DSIM_FIFOCTRL);
	udelay(500);

	fifoctrl |= fifo_ptrs;
	dsim_write(dsim, fifoctrl, DSIM_FIFOCTRL);
	udelay(500);
}

static void sec_mipi_dsim_config_clkctrl(struct sec_mipi_dsim *dsim)
{
	uint32_t clkctrl = 0, data_lanes_en;
	uint32_t byte_clk, esc_prescaler;

	clkctrl |= CLKCTRL_TXREQUESTHSCLK;
#if 0
	/* 0 means using 1.5Gbps PHY */
	if (dsim->bit_clk <= 700000)
		clkctrl |= CLKCTRL_DPHY_SEL_1G;
	clkctrl &= ~CLKCTRL_PLLBYPASS;
#endif
	clkctrl |= CLKCTRL_ESCCLKEN;


	clkctrl |= CLKCTRL_BYTECLKSRC_DPHY_PLL;

	clkctrl |= CLKCTRL_BYTECLKEN;

	data_lanes_en = (0x1 << dsim->lanes) - 1;
	clkctrl |= CLKCTRL_SET_LANEESCCLKEN(0x1 | data_lanes_en << 1);

	/* calculate esc prescaler from byte clock:
	 * EscClk = ByteClk / EscPrescaler;
	 */
	byte_clk = dsim->bit_clk >> 3;
	pr_info("%s: bit_clk=%d, clkctrl=%x\n", __func__, dsim->bit_clk, clkctrl);
	esc_prescaler = DIV_ROUND_UP(byte_clk, MAX_ESC_CLK_FREQ);
	clkctrl |= CLKCTRL_SET_ESCPRESCALER(esc_prescaler);

	dsim_write(dsim, clkctrl, DSIM_CLKCTRL);
}

static void sec_mipi_dsim_set_standby(struct sec_mipi_dsim *dsim,
				      bool standby)
{
	uint32_t mdresol = 0;

	mdresol = dsim_read(dsim, DSIM_MDRESOL);

	if (standby)
		mdresol |= MDRESOL_MAINSTANDBY;
	else
		mdresol &= ~MDRESOL_MAINSTANDBY;

	dsim_write(dsim, mdresol, DSIM_MDRESOL);
}

static int sec_mipi_dsim_get_pms(struct sec_mipi_dsim *dsim, unsigned long bit_clk, unsigned long ref_clk)
{
	struct dsim_pll_pms *pms = &dsim->pms;
	unsigned long b = bit_clk;
	unsigned long numerator;
	unsigned long denominator;
	unsigned p,m, p_min, p_max;
	unsigned int c_pms;
	int shift = 0;
	int s;
	int i;

#if 0
	bit_clk = bit_clk * 4 / 3;
#endif
#define INPUT_MIN_FREQ	6000000
#define INPUT_MAX_FREQ	12000000
	/* Table 13-51 */
	/* Fin 6-200 MHz */
	/* Fin_pll = Fin /p = 6-12 MHz */
	/* VCO_out = Fin_pll * m = 350 MHz to 750 MHz */
	/* Fout = VCO_out >> s = 37.5 MHz to 750 MHz */
	/* p ranges between 1 and 33 */
	/* m ranges between 25 and 125 */
	/* s ranges between 1 and 7 */
	p_max = ref_clk / INPUT_MIN_FREQ;
	if (p_max > 33)
		p_max = 33;
	p_min = (ref_clk + INPUT_MAX_FREQ - 1) / INPUT_MAX_FREQ;
	if (!p_min)
		p_min = 1;
	do {
		numerator = bit_clk << shift;
		/*
		 * Increase ref clock so that finding bigger will also find
		 * very slightly smaller. Needed for rounding errors in
		 * (pixel clock * 24)
		 */
		denominator = ref_clk + 2;
		get_best_ratio_bigger(&numerator, &denominator, 125, p_max << (7 - shift));
		denominator <<= shift;
		s = __ffs(denominator);
		if (s > 7)
			s = 7;
		p = denominator >> s;
		m = numerator;
		while (s && !(m & 1)) {
			m >>= 1;
			s--;
		}
		if ((denominator >= p_min) && (p <= p_max))
			break;
		i = 2;
		while ((m * i <= 125) && ((p * i) <= p_max)) {
			if ((p << s) * i >= p_min) {
				p *= i;
				m *= i;
				break;
			}
			i++;
		}
		if (((p << s) >= p_min) && (p <= p_max))
			break;
		shift++;
	} while (shift < 8);

	pr_debug("%s: %ld/%ld = %ld/%ld p_min=%d p_max=%d p=%d m=%d s=%d \n", __func__, numerator, denominator, bit_clk, ref_clk, p_min, p_max, p, m, s);
	if (!p) {
		pr_info("%s: bit_clk=%ld ref_clk=%ld, numerator=%ld, denominator=%ld\n",
			__func__, bit_clk, ref_clk, numerator, denominator);
		return -EINVAL;
	}
	while (p < p_min) {
		if (s)
			s--;
		else if (m <= 125/2)
			m <<= 1;
		else {
			m = (m * p_min + p - 1) / p;
			p = p_min;
			break;
		}
		p <<= 1;
	}
	while (m > 125) {
		if (s) {
			s--;
		} else if ((p >> 1) >= p_min) {
			p >>= 1;
		} else {
			m = 125;
			break;
		}
		m = (m + 1) >> 1;
	}
#define OUTPUT_MIN_FREQ	350000000
#define OUTPUT_MAX_FREQ	750000000
	while ((ref_clk * m < OUTPUT_MIN_FREQ * p) || (m < 25)) {
		if (m <= 125/2)
			m <<= 1;
		else if (p >> 1)
			p >>= 1;
		else
			break;
		s++;
	}
	while (ref_clk * m > OUTPUT_MAX_FREQ * p) {
		if (!s)
			break;
		if ((p << 1) <= p_max)
			p <<= 1;
		else
			m = (m + 1) >> 1;
		s--;
	}

	if (p < 1  || p > 33 ||
	    m < 25 || m > 125 ||
	    s < 0  || s > 7) {
		pr_info("%s: bit_clk=%ld ref_clk=%ld, p=%d, m=%d, s=%d\n",
			__func__, bit_clk, ref_clk, p, m, s);
		return -EINVAL;
	}
	bit_clk = (ref_clk * m / p) >> s;
	c_pms =	PLLCTRL_SET_P(p) |
		    PLLCTRL_SET_M(m) |
		    PLLCTRL_SET_S(s);
	dsim->c_pms = c_pms;
	if ((pms->p != p) || (pms->m != m) || (pms->s != s) || (pms->bit_clk != bit_clk)) {
		pms->p = p;
		pms->m = m;
		pms->s = s;
		pms->bit_clk = bit_clk;
		pr_info("%s: bit_clk=%ld %ld ref_clk=%ld, p=%d, m=%d, s=%d, lanes=%d\n",
			__func__, b, bit_clk, ref_clk, p, m, s, dsim->lanes);
	}
	/* Divided by 2 because mipi output clock is DDR */
	dsim->frequency = bit_clk / 2;
	dsim->byte_clock = bit_clk >> 3;
	dsim->bit_clk = DIV_ROUND_UP_ULL(bit_clk, 1000);
	return 0;
}

static int _sec_mipi_dsim_check_pll_out(struct sec_mipi_dsim *dsim)
{
	int bpp;
	unsigned long pix_clk;
	uint32_t bit_clk;
	const struct sec_mipi_dsim_plat_data *pdata = dsim->pdata;
	int ret;

	bpp = mipi_dsi_pixel_format_to_bpp(dsim->format);
	if (bpp < 0)
		return -EINVAL;

	pix_clk = clk_get_rate(dsim->clk_pixel);
	if (!pix_clk)
		pix_clk = dsim->def_pix_clk;

	bit_clk = DIV_ROUND_UP_ULL((u64)pix_clk * bpp, dsim->lanes);
	pr_debug("%s: %d = %ld * %d / %d\n", __func__, bit_clk, pix_clk, bpp, dsim->lanes);

	if (bit_clk > pdata->max_data_rate) {
		pix_clk = dsim->def_pix_clk;
		bit_clk = DIV_ROUND_UP_ULL((u64)pix_clk * bpp, dsim->lanes);
		pr_debug("%s:aa %d = %ld * %d / %d\n", __func__, bit_clk, pix_clk, bpp, dsim->lanes);
		if (bit_clk > pdata->max_data_rate) {
			dev_err(dsim->dev,
					"requested bit clk freq exceeds lane's maximum value\n");
			return -EINVAL;
		}
	}
	/* set suitable rate for phy ref clock */
	ret = sec_mipi_dsim_set_pref_rate(dsim, pix_clk);

	dsim->pixelclock = pix_clk;
	dsim->pix_clk = DIV_ROUND_UP_ULL(pix_clk, 1000);
	dsim->bit_clk = DIV_ROUND_UP_ULL(bit_clk, 1000);
	dsim->hpar = NULL;

	pr_debug("%s: %d = %ld * %d / %d\n", __func__, bit_clk, pix_clk, bpp, dsim->lanes);
	if (dsim->hsmult) {
		unsigned bit_clkm = pix_clk * dsim->hsmult;

		if (bit_clk < bit_clkm) {
			bit_clk = bit_clkm;
			pr_info("%s: %d = %ld * %d (min)\n", __func__, bit_clk, pix_clk, dsim->hsmult);
		}
	}
	if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_MBC) {
		unsigned n = (bpp + (dsim->lanes * 8) - 1) / (dsim->lanes * 8);
		unsigned bit_clkm = pix_clk * n * 8;

		if (bit_clk < bit_clkm) {
			bit_clk = bit_clkm;
			pr_info("%s: %d = %ld * 8 * %d(MBC)\n", __func__, bit_clk, pix_clk, n);
		}
	}
	ret = sec_mipi_dsim_get_pms(dsim, bit_clk, dsim->ref_clk);
	if (ret < 0)
		return ret;
	if (dsim->dsi_clk_hw.clk)
		clk_set_rate(dsim->dsi_clk_hw.clk, dsim->frequency);

	return 0;
}

int sec_mipi_dsim_check_pll_out(struct drm_bridge *bridge,
				const struct drm_display_mode *mode)
{
	const struct dsim_hblank_par *hpar;
	struct sec_mipi_dsim *dsim = bridge->driver_private;

	dsim->def_pix_clk = mode->clock * 1000;
	dsim->hsmult = mode->min_hs_clock_multiple;

	if (dsim->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE) {
		hpar = sec_mipi_dsim_get_hblank_par(mode->name,
						    drm_mode_vrefresh(mode),
						    dsim->lanes);
		dsim->hpar = hpar;
		if (!hpar)
			dev_dbg(dsim->dev, "no pre-exist hpar can be used\n");
	}

	return 0;
}
EXPORT_SYMBOL(sec_mipi_dsim_check_pll_out);

static
struct drm_crtc *sec_mipi_dsim_get_new_crtc(struct sec_mipi_dsim *dsim,
					    struct drm_atomic_state *state)
{
	struct drm_encoder *encoder = dsim->encoder;
	struct drm_connector *connector;
	struct drm_connector_state *conn_state;

	connector = drm_atomic_get_new_connector_for_encoder(state, encoder);
	if (!connector)
		return NULL;

	conn_state = drm_atomic_get_new_connector_state(state, connector);
	if (!conn_state)
		return NULL;

	return conn_state->crtc;
}

static void
sec_mipi_dsim_bridge_atomic_enable(struct drm_bridge *bridge,
				   struct drm_bridge_state *old_bridge_state)
{
	int ret;
	struct sec_mipi_dsim *dsim = bridge->driver_private;
	struct drm_atomic_state *old_state = old_bridge_state->base.state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;

	/* At this moment, the dsim bridge's preceding encoder has
	 * already been enabled. So the dsim can be configed here
	 */

	crtc = sec_mipi_dsim_get_new_crtc(dsim, old_state);
	if (!crtc) {
		dev_err(dsim->dev, "bridge is enabling without CRTC\n");
		return;
	}

	old_crtc_state = drm_atomic_get_old_crtc_state(old_state, crtc);
	/* Don't do enablement operation if we're coming back from PSR. */
	if (old_crtc_state && old_crtc_state->self_refresh_active &&
	    dsim->enabled)
		return;

	if (dsim->enabled)
		return;

	/* config main display mode */
	sec_mipi_dsim_set_main_mode(dsim);

	/* config dsim dpi */
	sec_mipi_dsim_config_dpi(dsim);

	/* config dsim pll */
	ret = sec_mipi_dsim_config_pll(dsim);
	if (ret) {
		dev_err(dsim->dev, "dsim pll config failed: %d\n", ret);
		return;
	}

	/* config dphy timings */
	sec_mipi_dsim_config_dphy(dsim);

	/* initialize FIFO pointers */
	sec_mipi_dsim_init_fifo_pointers(dsim);

	/* prepare panel if exists */
	if (dsim->panel) {
		ret = drm_panel_prepare(dsim->panel);
		if (unlikely(ret)) {
			dev_err(dsim->dev, "panel prepare failed: %d\n", ret);
			return;
		}
	}

	/* config esc clock, byte clock and etc */
	sec_mipi_dsim_config_clkctrl(dsim);

	pr_debug("%s: status=%x\n", __func__, dsim_read(dsim, DSIM_STATUS));

	/* enable panel if exists */
	if (dsim->panel) {
		ret = drm_panel_enable(dsim->panel);
		if (unlikely(ret)) {
			dev_err(dsim->dev, "panel enable failed: %d\n", ret);
			goto panel_unprepare;
		}
	}

	/* enable data transfer of dsim */
	sec_mipi_dsim_set_standby(dsim, true);

	dsim->enabled = true;
	pr_debug("%s: b status=%x\n", __func__, dsim_read(dsim, DSIM_STATUS));

	return;

panel_unprepare:
	ret = drm_panel_unprepare(dsim->panel);
	if (unlikely(ret))
		dev_err(dsim->dev, "panel unprepare failed: %d\n", ret);
}

static void sec_mipi_dsim_disable_clkctrl(struct sec_mipi_dsim *dsim)
{
	uint32_t clkctrl;

	clkctrl = dsim_read(dsim, DSIM_CLKCTRL);

	clkctrl &= ~CLKCTRL_TXREQUESTHSCLK;

	clkctrl &= ~CLKCTRL_ESCCLKEN;

	clkctrl &= ~CLKCTRL_BYTECLKEN;

	dsim_write(dsim, clkctrl, DSIM_CLKCTRL);
}

static void sec_mipi_dsim_disable_pll(struct sec_mipi_dsim *dsim)
{
	uint32_t pllctrl;

	pllctrl  = dsim_read(dsim, DSIM_PLLCTRL);

	pllctrl &= ~PLLCTRL_PLLEN;

	dsim_write(dsim, pllctrl, DSIM_PLLCTRL);
}

static void
sec_mipi_dsim_bridge_atomic_disable(struct drm_bridge *bridge,
				    struct drm_bridge_state *old_bridge_state)
{
	int ret;
	struct sec_mipi_dsim *dsim = bridge->driver_private;
	struct drm_atomic_state *old_state = old_bridge_state->base.state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;

	crtc = sec_mipi_dsim_get_new_crtc(dsim, old_state);
	/* No CRTC means we're doing a full shutdown. */
	if (!crtc)
		goto disable;

	new_crtc_state = drm_atomic_get_new_crtc_state(old_state, crtc);
	/* Don't do disablement operation if we're entering PSR. */
	if (!new_crtc_state || new_crtc_state->self_refresh_active)
		return;

disable:
	if (!dsim->enabled)
		return;

	/* disable panel if exists */
	if (dsim->panel) {
		ret = drm_panel_disable(dsim->panel);
		if (unlikely(ret))
			dev_err(dsim->dev, "panel disable failed: %d\n", ret);
	}

	/* disable data transfer of dsim */
	sec_mipi_dsim_set_standby(dsim, false);

	/* disable esc clock & byte clock */
	sec_mipi_dsim_disable_clkctrl(dsim);

	/* disable dsim pll */
	sec_mipi_dsim_disable_pll(dsim);

	/* unprepare panel if exists */
	if (dsim->panel) {
		ret = drm_panel_unprepare(dsim->panel);
		if (unlikely(ret))
			dev_err(dsim->dev, "panel unprepare failed: %d\n", ret);
	}

	dsim->enabled = false;
}

static void sec_mipi_dsim_bridge_mode_set(struct drm_bridge *bridge,
					  const struct drm_display_mode *mode,
					  const struct drm_display_mode *adjusted_mode)
{
	struct sec_mipi_dsim *dsim = bridge->driver_private;

	/* This hook is called when the display pipe is completely
	 * off. And since the pm runtime is implemented, the dsim
	 * hardware cannot be accessed at this moment. So move all
	 * the mode_set config to ->enable() hook.
	 * And this hook is called only when 'mode_changed' is true,
	 * so it is called not every time atomic commit.
	 */

	drm_display_mode_to_videomode(adjusted_mode, &dsim->vmode);
}

static u32 *sec_mipi_dsim_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
						    struct drm_bridge_state *bridge_state,
						    struct drm_crtc_state *crtc_state,
						    struct drm_connector_state *conn_state,
						    u32 output_fmt,
						    unsigned int *num_input_fmts)
{
	u32 *input_fmts;
	struct sec_mipi_dsim *dsim = bridge->driver_private;

	/* use dsi format to determine output bus format
	 * if it is passed with MEDIA_BUS_FMT_FIXED
	 */
	if (output_fmt == MEDIA_BUS_FMT_FIXED) {
		switch (dsim->format) {
		case MIPI_DSI_FMT_RGB888:
			output_fmt = MEDIA_BUS_FMT_RGB888_1X24;
			break;
		case MIPI_DSI_FMT_RGB666:
			output_fmt = MEDIA_BUS_FMT_RGB666_1X24_CPADHI;
			break;
		case MIPI_DSI_FMT_RGB666_PACKED:
			output_fmt = MEDIA_BUS_FMT_RGB666_1X18;
			break;
		case MIPI_DSI_FMT_RGB565:
			output_fmt = MEDIA_BUS_FMT_RGB565_1X16;
			break;
		default:
			return NULL;
		}
	} else {
		/* check if the output format matches with
		 * DSI device requested format
		 */
		switch (dsim->format) {
		case MIPI_DSI_FMT_RGB888:
			if (output_fmt != MEDIA_BUS_FMT_RGB888_1X24)
				return NULL;
			break;
		case MIPI_DSI_FMT_RGB666:
			if (output_fmt != MEDIA_BUS_FMT_RGB666_1X24_CPADHI)
				return NULL;
			break;
		case MIPI_DSI_FMT_RGB666_PACKED:
			if (output_fmt != MEDIA_BUS_FMT_RGB666_1X18)
				return NULL;
			break;
		case MIPI_DSI_FMT_RGB565:
			if (output_fmt != MEDIA_BUS_FMT_RGB565_1X16)
				return NULL;
			break;
		default:
			return NULL;
		}
	}

	/* Since dsim cannot do any color conversion, so the
	 * bus format output by mipi dsi should just be
	 * propagated to the bus format recieved by mipi dsi
	 * directly.
	 */
	input_fmts = drm_atomic_helper_bridge_propagate_bus_fmt(bridge,
								bridge_state,
								crtc_state,
								conn_state,
								output_fmt,
								num_input_fmts);

	return input_fmts;
}

static int sec_mipi_dsim_bridge_atomic_check(struct drm_bridge *bridge,
					     struct drm_bridge_state *bridge_state,
					     struct drm_crtc_state *crtc_state,
					     struct drm_connector_state *conn_state)
{
	struct drm_display_mode *adjusted_mode = &crtc_state->adjusted_mode;
	struct drm_bus_cfg *input_bus_cfg = &bridge_state->input_bus_cfg;
	struct sec_mipi_dsim *dsim = bridge->driver_private;

	/*
	 * If the DSI mode is not video mode, it implies
	 * that the connector is self refresh aware.
	 */
	if (!(dsim->mode_flags & MIPI_DSI_MODE_VIDEO))
		conn_state->self_refresh_aware = true;

	/* seems unnecessary to check the input bus format
	 * again, since during the negotiation process, it
	 * has already been checked
	 */
	if (bridge_state->output_bus_cfg.format == MEDIA_BUS_FMT_FIXED)
		bridge_state->output_bus_cfg.format = bridge_state->input_bus_cfg.format;

	/* adjust Hsync and Vsync polarities for drm display mode,
	 * since DSIM can only accept active high Hsync and Vsync
	 * signals
	 */
	if (adjusted_mode->flags & DRM_MODE_FLAG_NHSYNC) {
		adjusted_mode->flags &= ~DRM_MODE_FLAG_NHSYNC;
		adjusted_mode->flags |= DRM_MODE_FLAG_PHSYNC;
	}

	if (adjusted_mode->flags & DRM_MODE_FLAG_NVSYNC) {
		adjusted_mode->flags &= ~DRM_MODE_FLAG_NVSYNC;
		adjusted_mode->flags |= DRM_MODE_FLAG_PVSYNC;
	}

	/* adjust input DE and pixel data sample polarities for
	 * input bus_flags, since DSIM can only accept active
	 * high DE and sample pixel data at clock postive edge
	 */
	if (input_bus_cfg->flags & DRM_BUS_FLAG_DE_LOW ||
	    !(input_bus_cfg->flags & DRM_BUS_FLAG_DE_HIGH)) {
		input_bus_cfg->flags &= ~DRM_BUS_FLAG_DE_LOW;
		input_bus_cfg->flags |= DRM_BUS_FLAG_DE_HIGH;
	}

	if (input_bus_cfg->flags & DRM_BUS_FLAG_PIXDATA_SAMPLE_NEGEDGE ||
	    !(input_bus_cfg->flags & DRM_BUS_FLAG_PIXDATA_SAMPLE_POSEDGE)) {
		input_bus_cfg->flags &= ~DRM_BUS_FLAG_PIXDATA_SAMPLE_NEGEDGE;
		input_bus_cfg->flags |= DRM_BUS_FLAG_PIXDATA_SAMPLE_POSEDGE;
	}

	/* workaround for CEA standard mode "1280x720@60" "1920x1080p24"
	 * display on 4 data lanes with Non-burst with sync
	 * pulse DSI mode, since use the standard horizontal
	 * timings cannot display correctly. And this code
	 * cannot be put into the dsim Bridge's mode_fixup,
	 * since the DSI device lane number change always
	 * happens after that.
	 */
	if (!strcmp(adjusted_mode->name, "1280x720") &&
	    drm_mode_vrefresh(adjusted_mode) == 60   &&
	    dsim->lanes == 4		    &&
	    dsim->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE) {
		adjusted_mode->hsync_start += 2;
		adjusted_mode->hsync_end   += 2;
		adjusted_mode->htotal      += 2;
	}

	if (!strcmp(adjusted_mode->name, "1920x1080") &&
	    drm_mode_vrefresh(adjusted_mode) == 24 &&
	    dsim->lanes == 4		    &&
	    dsim->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE) {
		adjusted_mode->hsync_start += 2;
		adjusted_mode->hsync_end   += 2;
		adjusted_mode->htotal      += 2;
	}

	return 0;
}

static const struct drm_bridge_funcs sec_mipi_dsim_bridge_funcs = {
	.atomic_check = sec_mipi_dsim_bridge_atomic_check,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_get_input_bus_fmts = sec_mipi_dsim_atomic_get_input_bus_fmts,
	.attach     = sec_mipi_dsim_bridge_attach,
	.atomic_enable	= sec_mipi_dsim_bridge_atomic_enable,
	.atomic_disable	= sec_mipi_dsim_bridge_atomic_disable,
	.mode_set   = sec_mipi_dsim_bridge_mode_set,
};

int sec_mipi_dsim_pll_enable(struct drm_bridge *bridge, int enable)
{
	struct sec_mipi_dsim *dsim = bridge->driver_private;
	int ret = _sec_mipi_dsim_check_pll_out(dsim);

	if (ret < 0)
		return ret;
	pr_debug("%s:enable=%d was %d\n", __func__, enable, dsim->clk_cfg_enable);
	dsim->clk_cfg_enable = enable;
	if (enable)
		clk_prepare_enable(dsim->clk_cfg);
	else
		clk_disable_unprepare(dsim->clk_cfg);
	_sec_mipi_dsim_pll_enable(dsim, enable);
	return 0;
}
EXPORT_SYMBOL(sec_mipi_dsim_pll_enable);

void sec_mipi_dsim_suspend(struct device *dev)
{
	struct sec_mipi_dsim *dsim = dev_get_drvdata(dev);
	/* TODO: add dsim reset */

	if (dsim->clk_cfg_enable) {
		clk_disable_unprepare(dsim->clk_cfg);
		_sec_mipi_dsim_pll_enable(dsim, 0);
	}
}
EXPORT_SYMBOL(sec_mipi_dsim_suspend);

void sec_mipi_dsim_resume(struct device *dev)
{
	struct sec_mipi_dsim *dsim = dev_get_drvdata(dev);

	if (dsim->clk_cfg_enable) {
		_sec_mipi_dsim_pll_enable(dsim, 1);
		clk_prepare_enable(dsim->clk_cfg);
	}

	sec_mipi_dsim_irq_init(dsim);

	/* TODO: add dsim de-reset */
}
EXPORT_SYMBOL(sec_mipi_dsim_resume);

static void __maybe_unused sec_mipi_dsim_irq_mask(struct sec_mipi_dsim *dsim,
						  int irq_idx)
{
	uint32_t intmsk;

	intmsk = dsim_read(dsim, DSIM_INTMSK);

	switch (irq_idx) {
	case PLLSTABLE:
		intmsk |= INTMSK_MSKPLLSTABLE;
		break;
	case SWRSTRELEASE:
		intmsk |= INTMSK_MSKSWRELEASE;
		break;
	case SFRPLFIFOEMPTY:
		intmsk |= INTMSK_MSKSFRPLFIFOEMPTY;
		break;
	case SFRPHFIFOEMPTY:
		intmsk |= INTMSK_MSKSFRPHFIFOEMPTY;
		break;
	case FRAMEDONE:
		intmsk |= INTMSK_MSKFRAMEDONE;
		break;
	case LPDRTOUT:
		intmsk |= INTMSK_MSKLPDRTOUT;
		break;
	case TATOUT:
		intmsk |= INTMSK_MSKTATOUT;
		break;
	case RXDATDONE:
		intmsk |= INTMSK_MSKRXDATDONE;
		break;
	case RXTE:
		intmsk |= INTMSK_MSKRXTE;
		break;
	case RXACK:
		intmsk |= INTMSK_MSKRXACK;
		break;
	default:
		/* unsupported irq */
		return;
	}

	writel(intmsk, dsim->base + DSIM_INTMSK);
}

static void sec_mipi_dsim_irq_unmask(struct sec_mipi_dsim *dsim,
				     int irq_idx)
{
	uint32_t intmsk;

	intmsk = dsim_read(dsim, DSIM_INTMSK);

	switch (irq_idx) {
	case PLLSTABLE:
		intmsk &= ~INTMSK_MSKPLLSTABLE;
		break;
	case SWRSTRELEASE:
		intmsk &= ~INTMSK_MSKSWRELEASE;
		break;
	case SFRPLFIFOEMPTY:
		intmsk &= ~INTMSK_MSKSFRPLFIFOEMPTY;
		break;
	case SFRPHFIFOEMPTY:
		intmsk &= ~INTMSK_MSKSFRPHFIFOEMPTY;
		break;
	case FRAMEDONE:
		intmsk &= ~INTMSK_MSKFRAMEDONE;
		break;
	case LPDRTOUT:
		intmsk &= ~INTMSK_MSKLPDRTOUT;
		break;
	case TATOUT:
		intmsk &= ~INTMSK_MSKTATOUT;
		break;
	case RXDATDONE:
		intmsk &= ~INTMSK_MSKRXDATDONE;
		break;
	case RXTE:
		intmsk &= ~INTMSK_MSKRXTE;
		break;
	case RXACK:
		intmsk &= ~INTMSK_MSKRXACK;
		break;
	default:
		/* unsupported irq */
		return;
	}

	dsim_write(dsim, intmsk, DSIM_INTMSK);
}

/* write 1 clear irq */
static void sec_mipi_dsim_irq_clear(struct sec_mipi_dsim *dsim,
				    int irq_idx)
{
	uint32_t intsrc = 0;

	switch (irq_idx) {
	case PLLSTABLE:
		intsrc |= INTSRC_PLLSTABLE;
		break;
	case SWRSTRELEASE:
		intsrc |= INTSRC_SWRSTRELEASE;
		break;
	case SFRPLFIFOEMPTY:
		intsrc |= INTSRC_SFRPLFIFOEMPTY;
		break;
	case SFRPHFIFOEMPTY:
		intsrc |= INTSRC_SFRPHFIFOEMPTY;
		break;
	case FRAMEDONE:
		intsrc |= INTSRC_FRAMEDONE;
		break;
	case LPDRTOUT:
		intsrc |= INTSRC_LPDRTOUT;
		break;
	case TATOUT:
		intsrc |= INTSRC_TATOUT;
		break;
	case RXDATDONE:
		intsrc |= INTSRC_RXDATDONE;
		break;
	case RXTE:
		intsrc |= INTSRC_RXTE;
		break;
	case RXACK:
		intsrc |= INTSRC_RXACK;
		break;
	default:
		/* unsupported irq */
		return;
	}

	dsim_write(dsim, intsrc, DSIM_INTSRC);
}

static void sec_mipi_dsim_irq_init(struct sec_mipi_dsim *dsim)
{
	sec_mipi_dsim_irq_unmask(dsim, PLLSTABLE);
	sec_mipi_dsim_irq_unmask(dsim, SWRSTRELEASE);

	if (dsim->panel) {
		sec_mipi_dsim_irq_unmask(dsim, SFRPLFIFOEMPTY);
		sec_mipi_dsim_irq_unmask(dsim, SFRPHFIFOEMPTY);
		sec_mipi_dsim_irq_unmask(dsim, LPDRTOUT);
		sec_mipi_dsim_irq_unmask(dsim, TATOUT);
		sec_mipi_dsim_irq_unmask(dsim, RXDATDONE);
		sec_mipi_dsim_irq_unmask(dsim, RXTE);
		sec_mipi_dsim_irq_unmask(dsim, RXACK);
	}
}

static irqreturn_t sec_mipi_dsim_irq_handler(int irq, void *data)
{
	uint32_t intsrc, status;
	struct sec_mipi_dsim *dsim = data;

	intsrc = dsim_read(dsim, DSIM_INTSRC);
	status = dsim_read(dsim, DSIM_STATUS);
	pr_debug("%s: intsrc=%x status=%x\n", __func__, intsrc, status);

	if (WARN_ON(!intsrc)) {
		dev_err(dsim->dev, "interrupt is not from dsim\n");
		return IRQ_NONE;
	}

	if (WARN_ON(!(intsrc & INTSRC_MASK))) {
		dev_warn(dsim->dev, "unenable irq happens: %#x\n", intsrc);
		/* just clear irqs */
		dsim_write(dsim, intsrc, DSIM_INTSRC);
		return IRQ_NONE;
	}

	if (intsrc & INTSRC_PLLSTABLE) {
		WARN_ON(!(status & STATUS_PLLSTABLE));
		sec_mipi_dsim_irq_clear(dsim, PLLSTABLE);
		complete(&dsim->pll_stable);
	}

	if (intsrc & INTSRC_SWRSTRELEASE)
		sec_mipi_dsim_irq_clear(dsim, SWRSTRELEASE);

	if (intsrc & INTSRC_SFRPLFIFOEMPTY) {
		sec_mipi_dsim_irq_clear(dsim, SFRPLFIFOEMPTY);
		complete(&dsim->pl_tx_done);
	}

	if (intsrc & INTSRC_SFRPHFIFOEMPTY) {
		sec_mipi_dsim_irq_clear(dsim, SFRPHFIFOEMPTY);
		complete(&dsim->ph_tx_done);
	}

	if (WARN_ON(intsrc & INTSRC_LPDRTOUT)) {
		sec_mipi_dsim_irq_clear(dsim, LPDRTOUT);
		dev_warn(dsim->dev, "LP RX timeout\n");
	}

	if (WARN_ON(intsrc & INTSRC_TATOUT)) {
		sec_mipi_dsim_irq_clear(dsim, TATOUT);
		dev_warn(dsim->dev, "Turns around Acknowledge timeout\n");
	}

	if (intsrc & INTSRC_RXDATDONE) {
		sec_mipi_dsim_irq_clear(dsim, RXDATDONE);
		complete(&dsim->rx_done);
	}

	if (intsrc & INTSRC_RXTE) {
		sec_mipi_dsim_irq_clear(dsim, RXTE);
		dev_dbg(dsim->dev, "TE Rx trigger received\n");
	}

	if (intsrc & INTSRC_RXACK) {
		sec_mipi_dsim_irq_clear(dsim, RXACK);
		dev_dbg(dsim->dev, "ACK Rx trigger received\n");
	}

	return IRQ_HANDLED;
}

static int sec_mipi_dsim_connector_get_modes(struct drm_connector *connector)
{
	struct sec_mipi_dsim *dsim = conn_to_sec_mipi_dsim(connector);

	if (WARN_ON(!dsim->panel))
		return -ENODEV;

	return drm_panel_get_modes(dsim->panel, connector);
}

static const struct drm_connector_helper_funcs
	sec_mipi_dsim_connector_helper_funcs = {
	.get_modes = sec_mipi_dsim_connector_get_modes,
};

static enum drm_connector_status
	sec_mipi_dsim_connector_detect(struct drm_connector *connector,
				       bool force)
{
	/* TODO: add support later */

	return connector_status_connected;
}

static const struct drm_connector_funcs sec_mipi_dsim_connector_funcs = {
	.detect     = sec_mipi_dsim_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy    = drm_connector_cleanup,
	.reset      = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state   = drm_atomic_helper_connector_destroy_state,
};

#ifdef CONFIG_COMMON_CLK
#define dsi_clk_to_data(_hw) container_of(_hw, struct sec_mipi_dsim, dsi_clk_hw)

static unsigned long dsim_clk_recalc_rate(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	return dsi_clk_to_data(hw)->frequency;
}

static long dsim_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long *prate)
{
	return dsi_clk_to_data(hw)->frequency;
}

static int dsim_clk_set_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long parent_rate)
{
	return 0;
}

static int dsim_clk_prepare(struct clk_hw *hw)
{
	return 0;
}

static void dsim_clk_unprepare(struct clk_hw *hw)
{
	return;
}

static int dsim_clk_is_prepared(struct clk_hw *hw)
{
	struct sec_mipi_dsim *dsim = dsi_clk_to_data(hw);

	return dsim->frequency ? 1 : 0;
}

static const struct clk_ops dsim_clk_ops = {
	.prepare = dsim_clk_prepare,
	.unprepare = dsim_clk_unprepare,
	.is_prepared = dsim_clk_is_prepared,
	.recalc_rate = dsim_clk_recalc_rate,
	.round_rate = dsim_clk_round_rate,
	.set_rate = dsim_clk_set_rate,
};

static const char refname[] = "dsi_phy_ref";
static const char *parents[] = { refname, };

static struct clk *sec_mipi_dsim_clk_register_clk(struct sec_mipi_dsim *dsim)
{
	struct clk *clk;
	struct clk_init_data init;

	init.name = "dsim-clk";
	init.ops = &dsim_clk_ops;
	init.flags = 0;
	init.parent_names = parents;
	init.num_parents = 1;
	dsim->dsi_clk_hw.init = &init;

	/* optional override of the clockname */
	of_property_read_string(dsim->dev->of_node, "clock-output-names", &init.name);
	of_property_read_string(dsim->dev->of_node, "clock-pllref-name", &parents[0]);

	/* register the clock */
	clk = clk_register(dsim->dev, &dsim->dsi_clk_hw);
	if (!IS_ERR(clk))
		of_clk_add_provider(dsim->dev->of_node, of_clk_src_simple_get, clk);
	dsim->dsi_clk = clk;
	return clk;
}

static void sec_mipi_dsim_clk_unregister_clk(struct sec_mipi_dsim *dsim)
{
	clk_unregister(dsim->dsi_clk);
}
#endif

int sec_mipi_dsim_bind(struct device *dev, struct device *master, void *data,
		       struct drm_encoder *encoder, void __iomem *base,
		       int irq, const struct sec_mipi_dsim_plat_data *pdata)
{
	int ret, version;
	struct drm_device *drm_dev = data;
	struct drm_bridge *bridge;
	struct drm_connector *connector;
	struct sec_mipi_dsim *dsim;
	struct device_node *node = NULL;

	dev_dbg(dev, "sec-dsim bridge bind begin\n");

	dsim = devm_kzalloc(dev, sizeof(*dsim), GFP_KERNEL);
	if (!dsim) {
		dev_err(dev, "Unable to allocate 'dsim'\n");
		return -ENOMEM;
	}

	dsim->dev = dev;
	dsim->base = base;
	dsim->pdata = pdata;
	dsim->encoder = encoder;

	dsim->dsi_host.ops = &sec_mipi_dsim_host_ops;
	dsim->dsi_host.dev = dev;

	dsim->clk_pllref = devm_clk_get(dev, "pll-ref");
	if (IS_ERR(dsim->clk_pllref)) {
		ret = PTR_ERR(dsim->clk_pllref);
		dev_err(dev, "Unable to get phy pll reference clock: %d\n", ret);
		return ret;
	}

	dsim->clk_cfg = devm_clk_get(dev, "cfg");
	if (IS_ERR(dsim->clk_cfg)) {
		ret = PTR_ERR(dsim->clk_cfg);
		dev_err(dev, "Unable to get configuration clock: %d\n", ret);
		return ret;
	}
	dsim->clk_pixel = devm_clk_get(dev, "pixel_clock");
	if (IS_ERR(dsim->clk_pixel)) {
		dev_warn(dev, "Unable to get pixel clock: %ld\n", PTR_ERR(dsim->clk_pixel));
		dsim->clk_pixel = NULL;
	}

	dev_set_drvdata(dev, dsim);

	pm_runtime_get_sync(dev);
	version = dsim_read(dsim, DSIM_VERSION);
	WARN_ON(version != pdata->version);
	pm_runtime_put_sync(dev);

	dev_info(dev, "version number is %#x\n", version);
	dsim->def_pix_clk = 118800000;

	ret = devm_request_irq(dev, irq, sec_mipi_dsim_irq_handler,
			       0, dev_name(dev), dsim);
	if (ret) {
		dev_err(dev, "failed to request dsim irq: %d\n", ret);
		return ret;
	}

	init_completion(&dsim->pll_stable);
	init_completion(&dsim->ph_tx_done);
	init_completion(&dsim->pl_tx_done);
	init_completion(&dsim->rx_done);

	/* Initialize and attach sec dsim bridge */
	bridge = devm_kzalloc(dev, sizeof(*bridge), GFP_KERNEL);
	if (!bridge) {
		dev_err(dev, "Unable to allocate 'bridge'\n");
		return -ENOMEM;
	}

	/* mipi dsi host needs to be registered before bridge attach, since:
	 * 1. Have Panel
	 *    The 'mipi_dsi_host_register' will allocate a mipi_dsi_device
	 *    if the dsi host node has a panel child node in DTB. And dsi
	 *    host ->attach() will be called in panel's probe().
	 *
	 * 2. Have Bridge
	 *    The dsi host ->attach() will be called through the below
	 *    'drm_bridge_attach()' which will attach next bridge in a
	 *    chain.
	 */
	ret = mipi_dsi_host_register(&dsim->dsi_host);
	if (ret) {
		dev_err(dev, "Unable to register mipi dsi host: %d\n", ret);
		return ret;
	}

	dsim->bridge = bridge;
	bridge->driver_private = dsim;
	bridge->funcs = &sec_mipi_dsim_bridge_funcs;
	bridge->of_node = dev->of_node;
	bridge->encoder = encoder;

	/* attach sec dsim bridge and its next bridge if exists */
	ret = drm_bridge_attach(encoder, bridge, NULL, 0);
	if (ret) {
		dev_err(dev, "Failed to attach bridge: %s\n", dev_name(dev));

		/* no bridge exists, so defer probe to wait
		 * panel driver loading
		 */
		if (ret != -EPROBE_DEFER) {
			for_each_available_child_of_node(dev->of_node, node) {
				/* skip nodes without reg property */
				if (!of_find_property(node, "reg", NULL))
					continue;

				/* error codes only ENODEV or EPROBE_DEFER */
				dsim->panel = of_drm_find_panel(node);
				if (!IS_ERR(dsim->panel))
					goto panel;

				ret = PTR_ERR(dsim->panel);
			}
		}

		mipi_dsi_host_unregister(&dsim->dsi_host);
		return ret;
	}

panel:
	if (dsim->panel) {
		/* A panel has been attached */
		connector = &dsim->connector;

		drm_connector_helper_add(connector,
					 &sec_mipi_dsim_connector_helper_funcs);
		ret = drm_connector_init(drm_dev, connector,
					 &sec_mipi_dsim_connector_funcs,
					 DRM_MODE_CONNECTOR_DSI);
		if (ret)
			goto host_unregister;

		/* TODO */
		connector->dpms = DRM_MODE_DPMS_OFF;

		ret = drm_connector_attach_encoder(connector, encoder);
		if (ret)
			goto cleanup_connector;
	}

#ifdef CONFIG_COMMON_CLK
	sec_mipi_dsim_clk_register_clk(dsim);
#endif
	dev_dbg(dev, "sec-dsim bridge bind end\n");

	return 0;

cleanup_connector:
	drm_connector_cleanup(connector);
host_unregister:
	mipi_dsi_host_unregister(&dsim->dsi_host);
	return ret;
}
EXPORT_SYMBOL(sec_mipi_dsim_bind);

void sec_mipi_dsim_unbind(struct device *dev, struct device *master, void *data)
{
	struct sec_mipi_dsim *dsim = dev_get_drvdata(dev);

#ifdef CONFIG_COMMON_CLK
	sec_mipi_dsim_clk_unregister_clk(dsim);
#endif
	if (dsim->panel)
		drm_connector_cleanup(&dsim->connector);

	mipi_dsi_host_unregister(&dsim->dsi_host);
}
EXPORT_SYMBOL(sec_mipi_dsim_unbind);

MODULE_DESCRIPTION("Samsung MIPI DSI Host Controller bridge driver");
MODULE_AUTHOR("Fancy Fang <chen.fang@nxp.com>");
MODULE_LICENSE("GPL");
