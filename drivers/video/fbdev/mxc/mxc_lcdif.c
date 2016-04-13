/*
 * Copyright (C) 2011-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/init.h>
#include <linux/ipu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mxcfb.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>

#include "mxc_dispdrv.h"

struct mxc_lcd_platform_data {
	u32 default_ifmt;
	u32 ipu_id;
	u32 disp_id;
};

struct mxc_lcdif_data {
	struct platform_device *pdev;
	struct mxc_dispdrv_handle *disp_lcdif;
};

#define DISPDRV_LCD	"lcd"

static struct fb_videomode lcdif_modedb[] = {
	{
	/* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	"CLAA-WVGA", 57, 800, 480, 37037, 40, 60, 10, 10, 20, 10,
	FB_SYNC_CLK_LAT_FALL,
	FB_VMODE_NONINTERLACED,
	0,},
	{
	/* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	"INNOLUX-WVGA", 57, 800, 480, 26162,
	.left_margin = 45, .right_margin = 210,
	.upper_margin = 22, .lower_margin = 132,
	.hsync_len = 1, .vsync_len = 1,
	.sync = FB_SYNC_CLK_LAT_FALL,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
	{
	/* 800x480 @ 60 Hz , pixel clk @ 32MHz */
	"SEIKO-WVGA", 60, 800, 480, 29850, 89, 164, 23, 10, 10, 10,
	FB_SYNC_CLK_LAT_FALL,
	FB_VMODE_NONINTERLACED,
	0,},
	{
	/* sharp wvga 800x480 */
	"sharp-wvga", 60, 800, 480, 1000000000/928 * 1000 /525/60,
	.left_margin = 40, .right_margin = 40,
	.upper_margin = 31, .lower_margin = 11,
	.hsync_len = 48, .vsync_len = 3,
	.sync = FB_SYNC_CLK_LAT_FALL,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
	{
	/* ASIT500MA6F5D 800x480 */
	"ASIT500MA6F5D", 60, 800, 480, 1000000000000ULL / (800+88+40+48) / (480+32+13+3) / 60,
	.left_margin = 88, .right_margin = 40,
	.upper_margin = 32, .lower_margin = 13,
	.hsync_len = 48, .vsync_len = 3,
	.sync = FB_SYNC_CLK_LAT_FALL,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
	{
	/* Cevians wvga 800x480 */
	"DC050WX", 60, 800, 480, 1000000000/960 * 1000 /525/60,
	.left_margin = 96, .right_margin = 24,
	.upper_margin = 3, .lower_margin = 10,
	.hsync_len = 40, .vsync_len = 32,
	.sync = FB_SYNC_CLK_LAT_FALL,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
	{
	 /* Okaya 480x272 */
	 "okaya_480x272", 60, 480, 272, 97786,
	 .left_margin = 2, .right_margin = 1,
	 .upper_margin = 3, .lower_margin = 2,
	 .hsync_len = 41, .vsync_len = 10,
	 .sync = FB_SYNC_CLK_LAT_FALL,
	 .vmode = FB_VMODE_NONINTERLACED,
	 .flag = 0,},
	{
		/* 1280x120 @ 60 Hz , pixel clk @ 32MHz */
		"TRULY-1U",
		60, /* refresh rate in Hz */
		1280, /* xres in pixels */
		120, /* yres in pixels */
		45000, /* pixel clock in picoseconds (dot clock or just clock) */
		54, /* left_margin (Horizontal Back Porch) in pixel clock units */
		54, /* right_margin (Horizontal Front Porch) in pixel clock units */
		3, /* upper_margin (Vertical Back Porch) in pixel clock units */
		3, /* lower_margin (Vertical Front Porch) in pixel clock units */
		3, /* hsync_len (Hsync pulse width) */
		3, /* vsync_len (Vsync pulse width) */
		0, /* sync (Polarity on the Data Enable) */
		FB_VMODE_NONINTERLACED, /* vmode (Video Mode) */
		0, /* flags */
	},
	{
	/* 800x600 @ 60 Hz , pixel clk @ 40MHz */
	"LSA40AT9001", 60, 800, 600, 1000000000 / (800+10+46+210) * 1000 / (600+1+23+12) / 60,
	.left_margin = 46, .right_margin = 210,
	.upper_margin = 23, .lower_margin = 12,
	.hsync_len = 10, .vsync_len = 1,
	.sync = FB_SYNC_CLK_LAT_FALL,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
	{
	/* 480x800 @ 57 Hz , pixel clk @ 27MHz */
	"LB043", 57, 480, 800, 25000,
	.left_margin = 40, .right_margin = 60,
	.upper_margin = 10, .lower_margin = 10,
	.hsync_len = 20, .vsync_len = 10,
	.sync = FB_SYNC_CLK_LAT_FALL,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
	{
	/* 480x800 @ 60 Hz , pixel clk @ 27MHz */
	"AUO_G050", 60, 480, 800, 1000000000/516 * 1000 /836/60,
	.left_margin = 18, .right_margin = 16,
	.upper_margin = 18, .lower_margin = 16,
	.hsync_len = 2, .vsync_len = 2,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
	{
	/* 720x480 @ 60 Hz YUYV, pixel clk @ 27.11MHz */
	"A030JN01_YUV720", 60, 720, 480, 1000000000/859 * 1000 /526/60,
	.left_margin = 40, .right_margin = 98,
	.upper_margin = 27, .lower_margin = 18,
	.hsync_len = 1, .vsync_len = 1,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
	{
	/* 640x480 @ 60 Hz , pixel clk @ 33.7MHz, 1.5 clks/pixel, 640*3/2 = 960 */
	"A030JN01_UPS051", 60, 640, 480, 1000000000/1068 * 1000 /526/60,
	.left_margin = 40, .right_margin = 48,
	.upper_margin = 27, .lower_margin = 18,
	.hsync_len = 20, .vsync_len = 1,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
	{
	 /*
	  * hitachi 640x240
	  * vsync = 60
	  * hsync = 260 * vsync = 15.6 Khz
	  * pixclk = 800 * hsync = 12.48 MHz
	  */
	 "hitachi_hvga", 60, 640, 240, 1000000000 / (640+34+1+125) * 1000 / (240+8+3+9) / 60,	//80128, (12.48 MHz)
	 .left_margin = 34, .right_margin = 1,
	 .upper_margin = 8, .lower_margin = 3,
	 .hsync_len = 125, .vsync_len = 9,
	 .sync = FB_SYNC_CLK_LAT_FALL,
	 .vmode = FB_VMODE_NONINTERLACED,
	 .flag = 0,},
	{
	 /*
	  * AT035GT-07ET3 320x240
	  * vsync = 60
	  * hsync = 260 * vsync = 15.6 Khz
	  * pixclk = 800 * hsync = 12.48 MHz
	  */
	 "AT035GT-07ET3", 60, 320, 240, 1000000000000ULL / (320+40+18+30) / (240+10+9+3) / 60,	//80128, (12.48 MHz)
	 .left_margin = 40, .right_margin = 18,
	 .upper_margin = 10, .lower_margin = 9,
	 .hsync_len = 30, .vsync_len = 3,
	 .sync = 0,
	 .vmode = FB_VMODE_NONINTERLACED,
	 .flag = 0,},

	/*
	 * BT656/BT1120 mode
	 *
	 * left_margin: used for field0 vStart width in lines
	 *
	 * right_margin: used for field0 vEnd width in lines
	 *
	 * up_margin: used for field1 vStart width in lines
	 *
	 * down_margin: used for field1 vEnd width in lines
	 *
	 * hsync_len: EAV Code + Blanking Video + SAV Code (in pixel clock count)
	 *		   For BT656 NTSC, it is 4 + 67*4 + 4 = 276.
	 *		   For BT1120 NTSC, it is 4 + 67*2 + 4 = 142.
	 *		   For BT656 PAL, it is 4 + 70*4 + 4 = 288.
	 *		   For BT1120 PAL, it is 4 + 70*2 + 4 = 148.
	 *
	 * vsync_len: not used, set to 1
	 */
	{
	 /* NTSC Interlaced output */
	 "BT656-NTSC", 60, 720, 480, 37037,
	 19, 3,
	 20, 3,
	 276, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED,
	 FB_MODE_IS_DETAILED,},
	{
	 /* PAL Interlaced output */
	 "BT656-PAL", 50, 720, 576, 37037,
	 22, 2,
	 23, 2,
	 288, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED,
	 FB_MODE_IS_DETAILED,},
	{
	 /* NTSC Interlaced output */
	 "BT1120-NTSC", 30, 720, 480, 74074,
	 19, 3,
	 20, 3,
	 142, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED,
	 FB_MODE_IS_DETAILED,},
	{
	 /* PAL Interlaced output */
	 "BT1120-PAL", 25, 720, 576, 74074,
	 22, 2,
	 23, 2,
	 148, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED,
	 FB_MODE_IS_DETAILED,},
	{
	 /* 1080I60 Interlaced output */
	  "BT1120-1080I60", 60, 1920, 1080, 13468,
	  20, 3,
	  20, 2,
	  280, 1,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_INTERLACED,
	  FB_MODE_IS_DETAILED,},
	{
	  /* 1080I50 Interlaced output */
	  "BT1120-1080I50", 50, 1920, 1080, 13468,
	  20, 3,
	  20, 2,
	  720, 1,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_INTERLACED,
	  FB_MODE_IS_DETAILED,},
};
static int lcdif_modedb_sz = ARRAY_SIZE(lcdif_modedb);

static int lcdif_init(struct mxc_dispdrv_handle *disp,
	struct mxc_dispdrv_setting *setting)
{
	int ret, i;
	struct mxc_lcdif_data *lcdif = mxc_dispdrv_getdata(disp);
	struct device *dev = &lcdif->pdev->dev;
	struct mxc_lcd_platform_data *plat_data = dev->platform_data;
	struct fb_videomode *modedb = lcdif_modedb;
	int modedb_sz = lcdif_modedb_sz;

	/* use platform defined ipu/di */
	ret = ipu_di_to_crtc(dev, plat_data->ipu_id,
			     plat_data->disp_id, &setting->crtc);
	if (ret < 0)
		return ret;

	ret = fb_find_mode(&setting->fbi->var, setting->fbi, setting->dft_mode_str,
				modedb, modedb_sz, NULL, setting->default_bpp);
	if (!ret) {
		fb_videomode_to_var(&setting->fbi->var, &modedb[0]);
		setting->if_fmt = plat_data->default_ifmt;
	}

	INIT_LIST_HEAD(&setting->fbi->modelist);
	for (i = 0; i < modedb_sz; i++) {
		fb_add_videomode(&modedb[i],
				&setting->fbi->modelist);
	}

	return ret;
}

void lcdif_deinit(struct mxc_dispdrv_handle *disp)
{
	/*TODO*/
}

static struct mxc_dispdrv_driver lcdif_drv = {
	.name 	= DISPDRV_LCD,
	.init 	= lcdif_init,
	.deinit	= lcdif_deinit,
};

static int lcd_get_of_property(struct platform_device *pdev,
				struct mxc_lcd_platform_data *plat_data)
{
	struct device_node *np = pdev->dev.of_node;
	int err;
	u32 ipu_id, disp_id;
	const char *default_ifmt;

	err = of_property_read_string(np, "default_ifmt", &default_ifmt);
	if (err) {
		dev_dbg(&pdev->dev, "get of property default_ifmt fail\n");
		return err;
	}
	err = of_property_read_u32(np, "ipu_id", &ipu_id);
	if (err) {
		dev_dbg(&pdev->dev, "get of property ipu_id fail\n");
		return err;
	}
	err = of_property_read_u32(np, "disp_id", &disp_id);
	if (err) {
		dev_dbg(&pdev->dev, "get of property disp_id fail\n");
		return err;
	}

	plat_data->ipu_id = ipu_id;
	plat_data->disp_id = disp_id;
	if (!strncmp(default_ifmt, "RGB24", 5))
		plat_data->default_ifmt = IPU_PIX_FMT_RGB24;
	else if (!strncmp(default_ifmt, "BGR24", 5))
		plat_data->default_ifmt = IPU_PIX_FMT_BGR24;
	else if (!strncmp(default_ifmt, "GBR24", 5))
		plat_data->default_ifmt = IPU_PIX_FMT_GBR24;
	else if (!strncmp(default_ifmt, "RGB565", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_RGB565;
	else if (!strncmp(default_ifmt, "RGB666", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_RGB666;
	else if (!strncmp(default_ifmt, "YUV444", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_YUV444;
	else if (!strncmp(default_ifmt, "LVDS666", 7))
		plat_data->default_ifmt = IPU_PIX_FMT_LVDS666;
	else if (!strncmp(default_ifmt, "YUYV16", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_YUYV;
	else if (!strncmp(default_ifmt, "UYVY16", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_UYVY;
	else if (!strncmp(default_ifmt, "YVYU16", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_YVYU;
	else if (!strncmp(default_ifmt, "VYUY16", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_VYUY;
	else if (!strncmp(default_ifmt, "BT656", 5))
		plat_data->default_ifmt = IPU_PIX_FMT_BT656;
	else if (!strncmp(default_ifmt, "BT1120", 6))
		plat_data->default_ifmt = IPU_PIX_FMT_BT1120;
	else {
		dev_err(&pdev->dev, "err default_ifmt!\n");
		return -ENOENT;
	}

	return err;
}

static int mxc_lcdif_probe(struct platform_device *pdev)
{
	int ret;
	struct pinctrl *pinctrl;
	struct mxc_lcdif_data *lcdif;
	struct mxc_lcd_platform_data *plat_data;

	dev_dbg(&pdev->dev, "%s enter\n", __func__);
	lcdif = devm_kzalloc(&pdev->dev, sizeof(struct mxc_lcdif_data),
				GFP_KERNEL);
	if (!lcdif)
		return -ENOMEM;
	plat_data = devm_kzalloc(&pdev->dev,
				sizeof(struct mxc_lcd_platform_data),
				GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;
	pdev->dev.platform_data = plat_data;

	ret = lcd_get_of_property(pdev, plat_data);
	if (ret < 0) {
		dev_err(&pdev->dev, "get lcd of property fail\n");
		return ret;
	}

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(&pdev->dev, "can't get/select pinctrl\n");
		return PTR_ERR(pinctrl);
	}

	lcdif->pdev = pdev;
	lcdif->disp_lcdif = mxc_dispdrv_register(&lcdif_drv);
	mxc_dispdrv_setdata(lcdif->disp_lcdif, lcdif);

	dev_set_drvdata(&pdev->dev, lcdif);
	dev_dbg(&pdev->dev, "%s exit\n", __func__);

	return ret;
}

static int mxc_lcdif_remove(struct platform_device *pdev)
{
	struct mxc_lcdif_data *lcdif = dev_get_drvdata(&pdev->dev);

	mxc_dispdrv_puthandle(lcdif->disp_lcdif);
	mxc_dispdrv_unregister(lcdif->disp_lcdif);
	kfree(lcdif);
	return 0;
}

static const struct of_device_id imx_lcd_dt_ids[] = {
	{ .compatible = "fsl,lcd"},
	{ /* sentinel */ }
};
static struct platform_driver mxc_lcdif_driver = {
	.driver = {
		.name = "mxc_lcdif",
		.of_match_table	= imx_lcd_dt_ids,
	},
	.probe = mxc_lcdif_probe,
	.remove = mxc_lcdif_remove,
};

static int __init mxc_lcdif_init(void)
{
	return platform_driver_register(&mxc_lcdif_driver);
}

static void __exit mxc_lcdif_exit(void)
{
	platform_driver_unregister(&mxc_lcdif_driver);
}

module_init(mxc_lcdif_init);
module_exit(mxc_lcdif_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("i.MX ipuv3 LCD extern port driver");
MODULE_LICENSE("GPL");
