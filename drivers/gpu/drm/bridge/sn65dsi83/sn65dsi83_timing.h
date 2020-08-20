#ifndef __SN65DSI83_TIMING_H__
#define __SN65DSI83_TIMING_H__

/* Default Video Parameters */
#define PIXCLK_INIT 62500000

#define HACTIVE_INIT 1280
#define HPW_INIT 2
#define HBP_INIT 6
#define HFP_INIT 5

#define VACTIVE_INIT 800
#define VPW_INIT 1
#define VBP_INIT 2
#define VFP_INIT 3

static const struct display_timing panel_default_timing = {
	.pixelclock = {PIXCLK_INIT, PIXCLK_INIT, PIXCLK_INIT},
	.hactive = {HACTIVE_INIT, HACTIVE_INIT, HACTIVE_INIT},
	.hfront_porch = {HFP_INIT, HFP_INIT, HFP_INIT},
	.hsync_len = {HPW_INIT, HPW_INIT, HPW_INIT},
	.hback_porch = {HBP_INIT, HBP_INIT, HBP_INIT},
	.vactive = {VACTIVE_INIT, VACTIVE_INIT, VACTIVE_INIT},
	.vfront_porch = {VFP_INIT, VFP_INIT, VFP_INIT},
	.vsync_len = {VPW_INIT, VPW_INIT, VPW_INIT},
	.vback_porch = {VBP_INIT, VBP_INIT, VBP_INIT},
	.flags = DISPLAY_FLAGS_HSYNC_LOW |
	    DISPLAY_FLAGS_VSYNC_LOW |
	    DISPLAY_FLAGS_DE_LOW | DISPLAY_FLAGS_PIXDATA_NEGEDGE,
};

#endif /* __SN65DSI83_TIMING_H__ */
