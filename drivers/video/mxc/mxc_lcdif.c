/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mxcfb.h>
#include <linux/fsl_devices.h>
#include "mxc_dispdrv.h"

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
	/* 800x480 @ 60 Hz , pixel clk @ 32MHz */
	"SEIKO-WVGA", 60, 800, 480, 29850, 89, 164, 23, 10, 10, 10,
	FB_SYNC_CLK_LAT_FALL,
	FB_VMODE_NONINTERLACED,
	0,},
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
	/* 800x480M@60 with falling-edge pixel clock */
	"fusion7", 57, 800, 480, 33898, 96, 24, 3, 10, 72, 7,
	FB_SYNC_CLK_LAT_FALL,
	FB_VMODE_NONINTERLACED,
	0,},
	{
	/* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	"INNOLUX-WVGA", 57, 800, 480, 25000,
	.left_margin = 45, .right_margin = 1056 - 1 - 45 - 800,
	.upper_margin = 22, .lower_margin = 635 - 1 - 22 - 480,
	.hsync_len = 1, .vsync_len = 1,
	.sync = FB_SYNC_CLK_LAT_FALL,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,},
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
	 /*
	  * hitachi 640x240
	  * vsync = 60
	  * hsync = 260 * vsync = 15.6 Khz
	  * pixclk = 800 * hsync = 12.48 MHz
	  */
	 "hitachi_hvga", 60, 640, 240, 1000000000 / 800 * 1000 / 260 / 60,	//80128, (12.48 MHz)
	 .left_margin = 34, .right_margin = 1,
	 .upper_margin = 8, .lower_margin = 3,
	 .hsync_len = 125, .vsync_len = 9,
	 .sync = FB_SYNC_CLK_LAT_FALL,
	 .vmode = FB_VMODE_NONINTERLACED,
	 .flag = 0,},
};
static int lcdif_modedb_sz = ARRAY_SIZE(lcdif_modedb);

static int lcdif_init(struct mxc_dispdrv_handle *disp,
	struct mxc_dispdrv_setting *setting)
{
	int ret, i;
	struct mxc_lcdif_data *lcdif = mxc_dispdrv_getdata(disp);
	struct fsl_mxc_lcd_platform_data *plat_data
			= lcdif->pdev->dev.platform_data;
	struct fb_videomode *modedb = lcdif_modedb;
	int modedb_sz = lcdif_modedb_sz;

	/* use platform defined ipu/di */
	setting->dev_id = plat_data->ipu_id;
	setting->disp_id = plat_data->disp_id;

	ret = fb_find_mode(&setting->fbi->var, setting->fbi, setting->dft_mode_str,
				modedb, modedb_sz, NULL, setting->default_bpp);
	if (!ret) {
		fb_videomode_to_var(&setting->fbi->var, &modedb[0]);
		setting->if_fmt = plat_data->default_ifmt;
	}

	INIT_LIST_HEAD(&setting->fbi->modelist);
	for (i = 0; i < modedb_sz; i++) {
		struct fb_videomode m;
		fb_var_to_videomode(&m, &setting->fbi->var);
		if (fb_mode_is_equal(&m, &modedb[i])) {
			fb_add_videomode(&modedb[i],
					&setting->fbi->modelist);
			break;
		}
	}
	if (plat_data->enable_pins)
		plat_data->enable_pins();
	return ret;
}

void lcdif_deinit(struct mxc_dispdrv_handle *disp)
{
	struct mxc_lcdif_data *lcdif = mxc_dispdrv_getdata(disp);
	struct fsl_mxc_lcd_platform_data *plat_data
			= lcdif->pdev->dev.platform_data;

	if (plat_data->disable_pins)
		plat_data->disable_pins();
}

static struct mxc_dispdrv_driver lcdif_drv = {
	.name 	= DISPDRV_LCD,
	.init 	= lcdif_init,
	.deinit	= lcdif_deinit,
};

static int mxc_lcdif_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mxc_lcdif_data *lcdif;

	lcdif = kzalloc(sizeof(struct mxc_lcdif_data), GFP_KERNEL);
	if (!lcdif) {
		ret = -ENOMEM;
		goto alloc_failed;
	}

	lcdif->pdev = pdev;
	lcdif->disp_lcdif = mxc_dispdrv_register(&lcdif_drv);
	mxc_dispdrv_setdata(lcdif->disp_lcdif, lcdif);

	dev_set_drvdata(&pdev->dev, lcdif);

alloc_failed:
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

static struct platform_driver mxc_lcdif_driver = {
	.driver = {
		   .name = "mxc_lcdif",
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
