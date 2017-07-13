/*
 * FB driver for the ST7789V LCD Controller
 *
 * Copyright (C) 2015 Dennis Menschel
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

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME "fb_st7789v"

#define DEFAULT_GAMMA \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25\n" \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25"

/**
 * enum st7789v_command - ST7789V display controller commands
 *
 * @PORCTRL: porch setting
 * @GCTRL: gate control
 * @VCOMS: VCOM setting
 * @VDVVRHEN: VDV and VRH command enable
 * @VRHS: VRH set
 * @VDVS: VDV set
 * @VCMOFSET: VCOM offset set
 * @PWCTRL1: power control 1
 * @PVGAMCTRL: positive voltage gamma control
 * @NVGAMCTRL: negative voltage gamma control
 *
 * The command names are the same as those found in the datasheet to ease
 * looking up their semantics and usage.
 *
 * Note that the ST7789V display controller offers quite a few more commands
 * which have been omitted from this list as they are not used at the moment.
 * Furthermore, commands that are compliant with the MIPI DCS have been left
 * out as well to avoid duplicate entries.
 */
enum st7789v_command {
	TEOFF = 0x34,
	TEON = 0x35,
	STE = 0x44,
	RGBCTRL = 0xB1,
	PORCTRL = 0xB2,
	FRCTRL1 = 0xB3,
	GCTRL = 0xB7,
	VCOMS = 0xBB,
	VDVVRHEN = 0xC2,
	VRHS = 0xC3,
	VDVS = 0xC4,
	VCMOFSET = 0xC5,
	FRCTRL2 = 0xC6,
	PWCTRL1 = 0xD0,
	PVGAMCTRL = 0xE0,
	NVGAMCTRL = 0xE1,
};

#define MADCTL_BGR BIT(3) /* bitmask for RGB/BGR order */
#define MADCTL_MV BIT(5) /* bitmask for page/column order */
#define MADCTL_MX BIT(6) /* bitmask for column address order */
#define MADCTL_MY BIT(7) /* bitmask for page address order */

#define FMT16 (MIPI_DCS_PIXEL_FMT_16BIT | (MIPI_DCS_PIXEL_FMT_16BIT << 4))
#define FMT18 (MIPI_DCS_PIXEL_FMT_18BIT | (MIPI_DCS_PIXEL_FMT_18BIT << 4))


/**
 * init_display() - initialize the display controller
 *
 * @par: FBTFT parameter object
 *
 * Most of the commands in this init function set their parameters to the
 * same default values which are already in place after the display has been
 * powered up. (The main exception to this rule is the pixel format which
 * would default to 18 instead of 16 bit per pixel.)
 * Nonetheless, this sequence can be used as a template for concrete
 * displays which usually need some adjustments.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int init_display(struct fbtft_par *par)
{
	struct fbtft_platform_data *pdata = par->pdata;
	unsigned fps = par->display_fps;
	unsigned rtna;
	unsigned te_line = par->te_line;

	/* turn off sleep mode */
	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	/* set pixel format to RGB-565 */
	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT,
		(pdata->display.bpp == 16) ? FMT16 : FMT18);

	/* vertical Back porch=8, vertical Front porch = 8 */
	write_reg(par, PORCTRL, 0x08, 0x08, 0x00, 0x22, 0x22);

	/*
	 * VGH = 13.26V
	 * VGL = -10.43V
	 */
	write_reg(par, GCTRL, 0x35);

	/*
	 * VDV and VRH register values come from command write
	 * (instead of NVM)
	 */
	write_reg(par, VDVVRHEN, 0x01, 0xFF);

	/*
	 * VAP =  4.1V + (VCOM + VCOM offset + 0.5 * VDV)
	 * VAN = -4.1V + (VCOM + VCOM offset + 0.5 * VDV)
	 */
	write_reg(par, VRHS, 0x0B);

	/* VDV = 0V */
	write_reg(par, VDVS, 0x20);

	/* VCOM = 0.9V */
	write_reg(par, VCOMS, 0x20);

	/* VCOM offset = 0V */
	write_reg(par, VCMOFSET, 0x20);

	/*
	 * AVDD = 6.8V
	 * AVCL = -4.8V
	 * VDS = 2.3V
	 */
	write_reg(par, PWCTRL1, 0xA4, 0xA1);
	write_reg(par, FRCTRL1, 0x00, 0x0F, 0x0F);
	/*
	 * fps = 10 MHz / ((320 + VFPA + VBPA + VSYNC) * (240 + 2 + 4 + 2 + RTNA * 16))
	 * fps = 10 MHz / ((320 + 8 + 8 + 1) * (240 + 2 + 4 + 2 + RTNA * 16))
	 * fps = 10 MHz / 337 / (248 + RTNA * 16)
	 * fps = 10000000000 / 337 / (248 + RTNA * 16) / 1000
	 * fps = 29673591 / (248 + RTNA * 16) / 1000
	 * 248 + RTNA * 16 = 10 MHz / (337 * fps)
	 * RTNA * 16 = (10 MHz / (337 * fps)) - 248
	 * RTNA = ((10 MHz / (337 * fps)) - 248) / 16
	 * RTNA = ((10 MHz / (16 * 337 * fps)) - 15.5)
	 * RTNA = 1854.6 / fps - 15.5
	 * RTNA = (474778 / fps - 3968) >> 8
	 *
	 * fps == 61, rtna = 14.9
	 * fps == 60, rtna = 15.4
	 * fps == 40, rtna = 30.8
	 * rtna == 15, fps = 10000000/337/(248 + 240) = 10000000/337/488 = 60.8 (measured 61.7 FPS)
	 * rtna == 16, fps = 10000000/337/(248 + 256) = 10000000/337/504 = 58.8
	 * rtna == 21, fps = 10000000/337/(248 + 336) = 10000000/337/584 = 50.81 (measured 51.5 FPS)
	 * rtna == 31, fps = 10000000/337/(248 + 496) = 10000000/337/744 = 39.88 (measured 40.4 FPS)
	 */
	rtna = (474778 / fps - 3968 + 255) >> 8;
	if (rtna > 0x1f)
		rtna = 0x1f;
	fps = 29673591 / (248 + (rtna << 4));
	pr_info("%s: rtna=%d fps=%d.%.3d, te-line=%d irq=%d\n", __func__, rtna,
			fps / 1000, fps % 1000, te_line, par->irq);
	write_reg(par, FRCTRL2, 0x1f);
	if (par->irq) {
		write_reg(par, TEON, 0x00);		/* Turn on TE output */
		write_reg(par, STE, (te_line >> 8), (te_line & 0xff));	/* Trial & error to get best starting  */
	}
	write_reg(par, MIPI_DCS_ENTER_INVERT_MODE);
	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	return 0;
}

/**
 * set_var() - apply LCD properties like rotation and BGR mode
 *
 * @par: FBTFT parameter object
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_var(struct fbtft_par *par)
{
	u8 madctl_par = 0;

	if (par->bgr)
		madctl_par |= MADCTL_BGR;
	switch (par->info->var.rotate) {
	case 0:
		break;
	case 90:
		madctl_par |= (MADCTL_MV | MADCTL_MY);
		break;
	case 180:
		madctl_par |= (MADCTL_MX | MADCTL_MY);
		break;
	case 270:
		madctl_par |= (MADCTL_MV | MADCTL_MX);
		break;
	default:
		return -EINVAL;
	}
	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, madctl_par);
	return 0;
}

/**
 * set_gamma() - set gamma curves
 *
 * @par: FBTFT parameter object
 * @curves: gamma curves
 *
 * Before the gamma curves are applied, they are preprocessed with a bitmask
 * to ensure syntactically correct input for the display controller.
 * This implies that the curves input parameter might be changed by this
 * function and that illegal gamma values are auto-corrected and not
 * reported as errors.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_gamma(struct fbtft_par *par, unsigned long *curves)
{
	int i;
	int j;
	int c; /* curve index offset */

	/*
	 * Bitmasks for gamma curve command parameters.
	 * The masks are the same for both positive and negative voltage
	 * gamma curves.
	 */
	const u8 gamma_par_mask[] = {
		0xFF, /* V63[3:0], V0[3:0]*/
		0x3F, /* V1[5:0] */
		0x3F, /* V2[5:0] */
		0x1F, /* V4[4:0] */
		0x1F, /* V6[4:0] */
		0x3F, /* J0[1:0], V13[3:0] */
		0x7F, /* V20[6:0] */
		0x77, /* V36[2:0], V27[2:0] */
		0x7F, /* V43[6:0] */
		0x3F, /* J1[1:0], V50[3:0] */
		0x1F, /* V57[4:0] */
		0x1F, /* V59[4:0] */
		0x3F, /* V61[5:0] */
		0x3F, /* V62[5:0] */
	};

	for (i = 0; i < par->gamma.num_curves; i++) {
		c = i * par->gamma.num_values;
		for (j = 0; j < par->gamma.num_values; j++)
			curves[c + j] &= gamma_par_mask[j];
		write_reg(
			par, PVGAMCTRL + i,
			curves[c + 0], curves[c + 1], curves[c + 2],
			curves[c + 3], curves[c + 4], curves[c + 5],
			curves[c + 6], curves[c + 7], curves[c + 8],
			curves[c + 9], curves[c + 10], curves[c + 11],
			curves[c + 12], curves[c + 13]);
	}
	return 0;
}

/**
 * blank() - blank the display
 *
 * @par: FBTFT parameter object
 * @on: whether to enable or disable blanking the display
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int blank(struct fbtft_par *par, bool on)
{
	if (on)
		write_reg(par, MIPI_DCS_SET_DISPLAY_OFF);
	else
		write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	return 0;
}

static int read_scanline(struct fbtft_par *par)
{
	u32 *p = (u32 *)par->scanline_cmd;
	u32 *r = (u32 *)par->scanline_result;
	int ret;

#if 0
	/* RDID1 0x85, RDID2 0x85, RDID3 0x52 */
	p[0] = 0x04;
	r[0] = 0xffffffff;
	fbtft_read_reg_n(par, p, 2, r, 25);
	pr_info("%s:%x\n", __func__, r[0]);

	p[0] = 0xda;
	r[0] = 0xffffffff;
	fbtft_read_reg_n(par, p, 2, r, 8);
	pr_info("%s:%x\n", __func__, r[0]);

	p[0] = 0xdb;
	r[0] = 0xffffffff;
	fbtft_read_reg_n(par, p, 2, r, 8);
	pr_info("%s:%x\n", __func__, r[0]);

	p[0] = 0xdc;
	r[0] = 0xffffffff;
	fbtft_read_reg_n(par, p, 2, r, 8);
	pr_info("%s:%x\n", __func__, r[0]);
#endif

	p[0] = 0x45;
	r[0] = 0xffffffff;
	ret = fbtft_read_reg_n(par, p, 2, r, 17);
	if (ret)
		return ret;
	pr_debug("%s:%x\n", __func__, r[0]);
	return r[0] & 0xffff;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = 240,
	.height = 320,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_var = set_var,
		.set_gamma = set_gamma,
		.blank = blank,
		.read_scanline = read_scanline,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7789v", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7789v");
MODULE_ALIAS("platform:st7789v");

MODULE_DESCRIPTION("FB driver for the ST7789V LCD Controller");
MODULE_AUTHOR("Dennis Menschel");
MODULE_LICENSE("GPL");
