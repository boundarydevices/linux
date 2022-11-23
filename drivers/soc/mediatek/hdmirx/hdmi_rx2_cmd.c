// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>

#include <mtk_hdmirx.h>

#include "hdmi_rx2_hw.h"
#include "vga_table.h"
#include "hdmi_rx2_hal.h"
#include "hdmi_rx2_hdcp.h"
#include "hdmi_rx2_ctrl.h"
#include "hdmi_rx2_dvi.h"
#include "hdmi_rx2_aud_task.h"
#include "hdmirx_drv.h"
#include "hdmi_rx21_phy.h"

static char debug_buffer[128];

const u8 cts_4k_edid[] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x41, 0x0C,
	0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x0F, 0x1A, 0x01, 0x03, 0x80, 0x80,
	0x48, 0x78, 0x0A, 0xE6, 0x92, 0xA3, 0x54, 0x4A, 0x99, 0x26, 0x0F, 0x4A,
	0x4C, 0x21, 0x08, 0x00, 0x81, 0xC0, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x08, 0xE8, 0x00, 0x30,
	0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58, 0x8A, 0x00, 0x80, 0x68, 0x21, 0x00,
	0x00, 0x1E, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
	0x45, 0x00, 0x80, 0x68, 0x21, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC,
	0x00, 0x50, 0x68, 0x69, 0x6C, 0x69, 0x70, 0x73, 0x20, 0x46, 0x54, 0x56,
	0x0A, 0x20, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x30, 0x3E, 0x0F, 0x8C, 0x3C,
	0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x1E, 0x02, 0x03,
	0x4B, 0xF1, 0x59, 96, 97, 101, 102, 0x5D, 0x5F, 0x5E, 0x10, 0x1F, 0x20,
	0x22, 0x21, 0x05, 0x14, 0x04, 0x13, 0x12, 0x03, 0x11, 0x02, 0x16, 0x07,
	0x15, 0x06, 0x01, 0x2C, 0x09, 0x1F, 0x07, 0x11, 0x07, 0x50, 0x39, 0x07,
	0xFF, 0x51, 0x07, 0x00, 0x83, 0x01, 0x00, 0x00, 0x6F, 0x03, 0x0C, 0x00,
	0x10, 0x00, 0x78, 0x78, 0xA0, 0x4C, 0x4C, 0x00, 0x60, 0x01, 0x02, 0x03,
	0x67, 0xD8, 0x5D, 0xC4, 0x01, 0x78, 0xC8, 0x07, 0xE2, 0x00, 0x49, 0xE2,
	0x0F, 0x0F, 0x01, 0x1D, 0x80, 0x3E, 0x73, 0x38, 0x2D, 0x40, 0x7E, 0x2C,
	0x45, 0x80, 0x80, 0x68, 0x21, 0x00, 0x00, 0x1E, 0x01, 0x1D, 0x80, 0xD0,
	0x72, 0x1C, 0x16, 0x20, 0x10, 0x2C, 0x25, 0x80, 0x80, 0x68, 0x21, 0x00,
	0x00, 0x9E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x65};

const u8 edid_4k[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x41, 0x0c,
	0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x0f, 0x1a, 0x01, 0x03, 0x80, 0x80,
	0x48, 0x78, 0x0a, 0xe6, 0x92, 0xa3, 0x54, 0x4a, 0x99, 0x26, 0x0f, 0x4a,
	0x4c, 0x21, 0x08, 0x00, 0x81, 0xc0, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74, 0x00, 0x30,
	0xf2, 0x70, 0x5a, 0x80, 0xb0, 0x58, 0x8a, 0x00, 0x80, 0x68, 0x21, 0x00,
	0x00, 0x1e, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x45, 0x00, 0x80, 0x68, 0x21, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xfc,
	0x00, 0x50, 0x68, 0x69, 0x6c, 0x69, 0x70, 0x73, 0x20, 0x46, 0x54, 0x56,
	0x0a, 0x20, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x30, 0x3e, 0x0f, 0x8c, 0x1e,
	0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xb4, 0x02, 0x03,
	0x4a, 0xf1, 0x57, 0x60, 0x61, 0x5f, 0x5e, 0x5d, 0x10, 0x1f, 0x20, 0x22,
	0x21, 0x05, 0x14, 0x04, 0x13, 0x12, 0x03, 0x11, 0x02, 0x16, 0x07, 0x15,
	0x06, 0x01, 0x2c, 0x09, 0x7f, 0x07, 0x15, 0x07, 0x50, 0x39, 0x07, 0xff,
	0x51, 0x07, 0x00, 0x83, 0x01, 0x00, 0x00, 0x6d, 0x03, 0x0c, 0x00, 0x30,
	0x00, 0x30, 0x3c, 0x2f, 0x08, 0x60, 0x01, 0x02, 0x03, 0x67, 0xd8, 0x5d,
	0xc4, 0x01, 0x00, 0x00, 0x00, 0xe2, 0x00, 0x49, 0xe3, 0x0e, 0x60, 0x61,
	0x01, 0x1d, 0x80, 0x3e, 0x73, 0x38, 0x2d, 0x40, 0x7e, 0x2c, 0x45, 0x80,
	0x80, 0x68, 0x21, 0x00, 0x00, 0x1e, 0x01, 0x1d, 0x80, 0xd0, 0x72, 0x1c,
	0x16, 0x20, 0x10, 0x2c, 0x25, 0x80, 0x80, 0x68, 0x21, 0x00, 0x00, 0x9e,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xf2};

const u8 test_4_edid[] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x4D, 0x29,
	0x48, 0x44, 0x01, 0x00, 0x00, 0x00, 0x0A, 0x0D, 0x01, 0x03, 0x80, 0x50,
	0x2D, 0x78, 0x0A, 0x0D, 0xC9, 0xA0, 0x57, 0x47, 0x98, 0x27, 0x12, 0x48,
	0x4C, 0x20, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1D, 0x80, 0x18,
	0x71, 0x1C, 0x16, 0x20, 0x58, 0x2C, 0x25, 0x00, 0x20, 0xC2, 0x31, 0x00,
	0x00, 0x9E, 0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E,
	0x96, 0x00, 0x13, 0x8E, 0x21, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xFC,
	0x00, 0x48, 0x44, 0x4D, 0x49, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x3B, 0x3D, 0x0F, 0x2E, 0x08,
	0x02, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x03, 0xF1,

	0xF0, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C,

	0x02, 0x03, 0x1E, 0xF1, 0x4A, 0x85, 0x04, 0x10, 0x02, 0x01, 0x06, 0x14,
	0x12, 0x16, 0x13, 0x23, 0x09, 0x07, 0x07, 0x83, 0x01, 0x00, 0x00, 0x66,
	0x03, 0x0C, 0x00, 0x10, 0x00, 0x80, 0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0,
	0x1E, 0x20, 0x6E, 0x28, 0x55, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E,
	0xD6, 0x09, 0x80, 0xA0, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x60, 0x22, 0x00,
	0x12, 0x8E, 0x21, 0x08, 0x08, 0x18, 0x8C, 0x0A, 0xD0, 0x90, 0x20, 0x40,
	0x31, 0x20, 0x0C, 0x40, 0x55, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x18,
	0x01, 0x1D, 0x80, 0xD0, 0x72, 0x1C, 0x16, 0x20, 0x10, 0x2C, 0x25, 0x80,
	0xC4, 0x8E, 0x21, 0x00, 0x00, 0x9E, 0x8C, 0x0A, 0xA0, 0x14, 0x51, 0xF0,
	0x16, 0x00, 0x26, 0x7C, 0x43, 0x00, 0x13, 0x8E, 0x21, 0x00, 0x00, 0x98,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF5,

	0x02, 0x03, 0x04, 0xF1, 0xF3, 0x39, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
	0x58, 0x2C, 0x45, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E, 0x8C, 0x0A,
	0xA0, 0x20, 0x51, 0x20, 0x18, 0x10, 0x18, 0x7E, 0x23, 0x00, 0xC4, 0x8E,
	0x21, 0x00, 0x00, 0x98, 0x01, 0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20,
	0xB8, 0x28, 0x55, 0x40, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDF
};

const u8 pdYMH1800Edid[] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x4d, 0xd9, 0x01, 0x52, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x11, 0x01, 0x03, 0x80, 0xa0, 0x5a, 0x78,
	0x0a, 0x0d, 0xc9, 0xa0, 0x57, 0x47, 0x98, 0x27,
	0x12, 0x48, 0x4c, 0x21, 0x08, 0x00, 0x81, 0x80,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x8c, 0x0a,
	0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e,
	0x96, 0x00, 0xc4, 0x8e, 0x21, 0x00, 0x00, 0x18,
	0x01, 0x1d, 0x00, 0x72, 0x51, 0xd0, 0x1e, 0x20,
	0x6e, 0x28, 0x55, 0x00, 0x40, 0x84, 0x63, 0x00,
	0x00, 0x1e, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x53,
	0x4f, 0x4e, 0x59, 0x20, 0x54, 0x56, 0x20, 0x58,
	0x56, 0x0a, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd,
	0x00, 0x30, 0x3e, 0x0e, 0x46, 0x0f, 0x00, 0x0a,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x4b,
	0x02, 0x03, 0x4a, 0xf4, 0x5c, 0x1f, 0x03, 0x04,
	0x12, 0x13, 0x05, 0x14, 0x20, 0x07, 0x16, 0x10,
	0x15, 0x11, 0x02, 0x06, 0x01, 0x0e, 0x0f, 0x1d,
	0x1e, 0x0a, 0x0b, 0x19, 0x1a, 0x23, 0x24, 0x25,
	0x26, 0x3b, 0x09, 0x7f, 0x07, 0x0f, 0x7f, 0x07,
	0x15, 0x07, 0x50, 0x35, 0x07, 0x48, 0x3e, 0x1f,
	0xc0, 0x4d, 0x02, 0x00, 0x57, 0x06, 0x00, 0x67,
	0x54, 0x00, 0x5f, 0x54, 0x01, 0x83, 0x5f, 0x00,
	0x00, 0x68, 0x03, 0x0c, 0x00, 0x21, 0x00, 0xb8,
	0x2d, 0x00, 0x8c, 0x0a, 0xd0, 0x90, 0x20, 0x40,
	0x31, 0x20, 0x0c, 0x40, 0x55, 0x00, 0xc4, 0x8e,
	0x21, 0x00, 0x00, 0x18, 0x01, 0x1d, 0x00, 0xbc,
	0x52, 0xd0, 0x1e, 0x20, 0xb8, 0x28, 0x55, 0x40,
	0x40, 0x84, 0x63, 0x00, 0x00, 0x1e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa2
};

const u8 vrr30to60[] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	0x36, 0x8B, 0x03, 0xF9, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x19, 0x01, 0x03, 0x80, 0xA0, 0x5A, 0x78,
	0x0A, 0x0D, 0xC9, 0xA0, 0x57, 0x47, 0x98, 0x27,
	0x12, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x81, 0x80,
	0xA9, 0xC0, 0x71, 0x4F, 0xB3, 0x00, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A,
	0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
	0x45, 0x00, 0x40, 0x84, 0x63, 0x00, 0x00, 0x1E,
	0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20,
	0x6E, 0x28, 0x55, 0x00, 0x40, 0x84, 0x63, 0x00,
	0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x4D,
	0x54, 0x4B, 0x20, 0x4C, 0x43, 0x44, 0x54, 0x56,
	0x0A, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
	0x00, 0x30, 0x3E, 0x0E, 0x46, 0x3C, 0x00, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xD8,
	0x02, 0x03, 0x7F, 0xF1, 0x5B, 0x61, 0x60, 0x5D,
	0x5E, 0x5F, 0x62, 0x1F, 0x10, 0x3F, 0x76, 0x13,
	0x04, 0x20, 0x22, 0x3C, 0x3E, 0x12, 0x16, 0x03,
	0x07, 0x11, 0x15, 0x02, 0x06, 0x01, 0x65, 0x66,
	0x2F, 0x0D, 0x7F, 0x07, 0x15, 0x07, 0x50, 0x3D,
	0x07, 0xBC, 0x57, 0x04, 0x01, 0x67, 0x04, 0x03,
	0x83, 0x0F, 0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00,
	0x10, 0x00, 0xF8, 0x3C, 0x2F, 0x00, 0x80, 0x01,
	0x02, 0x03, 0x04, 0x6A, 0xD8, 0x5D, 0xC4, 0x01,
	0x78, 0x80, 0x07, 0x22, 0x1E, 0x3C, 0xE2, 0x00,
	0xF9, 0xE3, 0x05, 0xFF, 0x01, 0xE5, 0x0F, 0x03,
	0x02, 0x00, 0x06, 0xE3, 0x06, 0x05, 0x01, 0xE6,
	0x07, 0x04, 0x04, 0x00, 0x04, 0x04, 0xE6, 0x11,
	0x46, 0xD0, 0x00, 0x70, 0x00, 0xEB, 0x01, 0x46,
	0xD0, 0x00, 0x22, 0x17, 0x53, 0xB5, 0x8B, 0x58,
	0x65, 0xE5, 0x01, 0x8B, 0x84, 0x90, 0x01, 0xF1
};

const u8 vrr24to60[] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	0x36, 0x8B, 0x03, 0xF9, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x19, 0x01, 0x03, 0x80, 0xA0, 0x5A, 0x78,
	0x0A, 0x0D, 0xC9, 0xA0, 0x57, 0x47, 0x98, 0x27,
	0x12, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x81, 0x80,
	0xA9, 0xC0, 0x71, 0x4F, 0xB3, 0x00, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A,
	0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
	0x45, 0x00, 0x40, 0x84, 0x63, 0x00, 0x00, 0x1E,
	0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20,
	0x6E, 0x28, 0x55, 0x00, 0x40, 0x84, 0x63, 0x00,
	0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x4D,
	0x54, 0x4B, 0x20, 0x4C, 0x43, 0x44, 0x54, 0x56,
	0x0A, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
	0x00, 0x30, 0x3E, 0x0E, 0x46, 0x3C, 0x00, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xD8,
	0x02, 0x03, 0x7F, 0xF1, 0x5B, 0x61, 0x60, 0x5D,
	0x5E, 0x5F, 0x62, 0x1F, 0x10, 0x3F, 0x76, 0x13,
	0x04, 0x20, 0x22, 0x3C, 0x3E, 0x12, 0x16, 0x03,
	0x07, 0x11, 0x15, 0x02, 0x06, 0x01, 0x65, 0x66,
	0x2F, 0x0D, 0x7F, 0x07, 0x15, 0x07, 0x50, 0x3D,
	0x07, 0xBC, 0x57, 0x04, 0x01, 0x67, 0x04, 0x03,
	0x83, 0x0F, 0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00,
	0x10, 0x00, 0xF8, 0x3C, 0x2F, 0x00, 0x80, 0x01,
	0x02, 0x03, 0x04, 0x6A, 0xD8, 0x5D, 0xC4, 0x01,
	0x78, 0x80, 0x07, 0x22, 0x18, 0x3C, 0xE2, 0x00,
	0xF9, 0xE3, 0x05, 0xFF, 0x01, 0xE5, 0x0F, 0x03,
	0x02, 0x00, 0x06, 0xE3, 0x06, 0x05, 0x01, 0xE6,
	0x07, 0x04, 0x04, 0x00, 0x04, 0x04, 0xE6, 0x11,
	0x46, 0xD0, 0x00, 0x70, 0x00, 0xEB, 0x01, 0x46,
	0xD0, 0x00, 0x22, 0x17, 0x53, 0xB5, 0x8B, 0x58,
	0x65, 0xE5, 0x01, 0x8B, 0x84, 0x90, 0x01, 0xF7
};


#ifdef HDMIRX_ENG_BUILD
static u32
hdmi2cmd_status(struct MTK_HDMI *myhdmi)
{
	hdmi2_show_all_status(myhdmi);
	dvi2_show_status(myhdmi);

	return 0;
}

static u32
hdmi2cmd_init(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("HDMI Rx task Init\n");

	return 0;
}

static u32
hdmi2cmd_read_reg(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 u4Data;
	u32 u4Addr;
	void __iomem *va_addr = NULL;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("rg:0xPPPPPPPP\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x", &u4Addr);
	if (ret != 1)
		return 0;

	if ((u4Addr >= 0x1c400000) && (u4Addr < 0x1c400000 + 0x1000))
		va_addr = (RX_ADR + ((u4Addr - 0x1c400000)));
	else if ((u4Addr >= 0x1c300000) && (u4Addr < 0x1c300000 + 0x1000))
		va_addr = (TX_ADR + (u4Addr - 0x1c300000));
	else if ((u4Addr >= 0x11d60000) && (u4Addr < 0x11d60000 + 0x3000))
		va_addr = (A_ADR + (u4Addr - 0x11d60000));
	else if ((u4Addr >= 0x11d5f000) && (u4Addr < 0x11d5f000 + 0x1000))
		va_addr = (V_ADR + (u4Addr - 0x11d5f000));
	else if ((u4Addr >= 0x10014800) && (u4Addr < 0x10014800 + 0x400))
		va_addr = (S_ADR + (u4Addr - 0x10014800));
	else if ((u4Addr >= 0x10014400) && (u4Addr < 0x10014400 + 0x400))
		va_addr = (E_ADR + (u4Addr - 0x10014400));
	else if ((u4Addr >= 0x1000c000) && (u4Addr < 0x1000c000 + 0x1000))
		va_addr = (AP_ADR + (u4Addr - 0x1000c000));
	else if ((u4Addr >= 0x10007000) && (u4Addr < 0x10007000 + 0x1000))
		va_addr = (R_ADR + (u4Addr - 0x10007000));
	else if ((u4Addr >= 0x10000000) && (u4Addr < 0x10000000 + 0x1000))
		va_addr = (CK_ADR + (u4Addr - 0x10000000));
	else
		return 0;

	u4Data = r_reg(va_addr);

	RX_DEF_LOG("0x%08x = 0x%08x\n", u4Addr, u4Data);

	ret = sprintf(debug_buffer, "0x%08x", u4Data);
	if (ret < 0)
		return 0;

	return 0;
}

static u32
hdmi2cmd_read_reg_range(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 u4Data;
	u32 u4Addr;
	u32 u4Count;
	void __iomem *va_addr = NULL;
	u32 len = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("rg:0xPPPPPPPP/0xPPP\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x/0x%x", &u4Addr, &u4Count);
	if (ret != 2)
		return 0;

	if ((u4Addr >= 0x1c400000) && (u4Addr < 0x1c400000 + 0x1000)) {
		len = 0x1000 - (u4Addr - 0x1c400000);
		va_addr = (RX_ADR + ((u4Addr - 0x1c400000)));
	} else if ((u4Addr >= 0x1c300000) && (u4Addr < 0x1c300000 + 0x1000)) {
		len = 0x1000 - (u4Addr - 0x1c300000);
		va_addr = (TX_ADR + (u4Addr - 0x1c300000));
	} else if ((u4Addr >= 0x11d60000) && (u4Addr < 0x11d60000 + 0x3000)) {
		len = 0x3000 - (u4Addr - 0x11d60000);
		va_addr = (A_ADR + (u4Addr - 0x11d60000));
	} else if ((u4Addr >= 0x11d5f000) && (u4Addr < 0x11d5f000 + 0x1000)) {
		len = 0x1000 - (u4Addr - 0x11d5f000);
		va_addr = (V_ADR + (u4Addr - 0x11d5f000));
	} else if ((u4Addr >= 0x10014800) && (u4Addr < 0x10014800 + 0x400)) {
		len = 0x400 - (u4Addr - 0x10014800);
		va_addr = (S_ADR + (u4Addr - 0x10014800));
	} else if ((u4Addr >= 0x10014400) && (u4Addr < 0x10014400 + 0x400)) {
		len = 0x400 - (u4Addr - 0x10014400);
		va_addr = (E_ADR + (u4Addr - 0x10014400));
	} else if ((u4Addr >= 0x1000c000) && (u4Addr < 0x1000c000 + 0x1000)) {
		len = 0x1000 - (u4Addr - 0x1000c000);
		va_addr = (AP_ADR + (u4Addr - 0x1000c000));
	} else if ((u4Addr >= 0x10007000) && (u4Addr < 0x10007000 + 0x1000)) {
		len = 0x1000 - (u4Addr - 0x10007000);
		va_addr = (R_ADR + (u4Addr - 0x10007000));
	} else if ((u4Addr >= 0x10000000) && (u4Addr < 0x10000000 + 0x1000)) {
		len = 0x1000 - (u4Addr - 0x10000000);
		va_addr = (CK_ADR + (u4Addr - 0x10000000));
	} else
		return 0;

	if (u4Count > len)
		return 0;

	while (u4Count >= 4) {
		u4Data = r_reg(va_addr);
		RX_DEF_LOG("0x%08x = 0x%08x\n", u4Addr, u4Data);
		HAL_Delay_us(2);
		u4Addr = u4Addr + 4;
		va_addr = va_addr + 4;
		u4Count = u4Count - 4;
	}

	return 0;
}

static u32
hdmi2cmd_write_reg(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 u4Data;
	u32 u4Addr;
	void __iomem *va_addr = NULL;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("wg:0xPPPPPPPP=0xPPPPPPPP\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x=0x%x", &u4Addr, &u4Data);
	if (ret != 2)
		return 0;

	if ((u4Addr >= 0x1c400000) && (u4Addr < 0x1c400000 + 0x1000))
		va_addr = (RX_ADR + ((u4Addr - 0x1c400000)));
	else if ((u4Addr >= 0x1c300000) && (u4Addr < 0x1c300000 + 0x1000))
		va_addr = (TX_ADR + (u4Addr - 0x1c300000));
	else if ((u4Addr >= 0x11d60000) && (u4Addr < 0x11d60000 + 0x3000))
		va_addr = (A_ADR + (u4Addr - 0x11d60000));
	else if ((u4Addr >= 0x11d5f000) && (u4Addr < 0x11d5f000 + 0x1000))
		va_addr = (V_ADR + (u4Addr - 0x11d5f000));
	else if ((u4Addr >= 0x10014800) && (u4Addr < 0x10014800 + 0x400))
		va_addr = (S_ADR + (u4Addr - 0x10014800));
	else if ((u4Addr >= 0x10014400) && (u4Addr < 0x10014400 + 0x400))
		va_addr = (E_ADR + (u4Addr - 0x10014400));
	else if ((u4Addr >= 0x1000c000) && (u4Addr < 0x1000c000 + 0x1000))
		va_addr = (AP_ADR + (u4Addr - 0x1000c000));
	else if ((u4Addr >= 0x10007000) && (u4Addr < 0x10007000 + 0x1000))
		va_addr = (R_ADR + (u4Addr - 0x10007000));
	else if ((u4Addr >= 0x10000000) && (u4Addr < 0x10000000 + 0x1000))
		va_addr = (CK_ADR + (u4Addr - 0x10000000));
	else
		return 0;

	w_reg(va_addr, u4Data);
	u4Data = r_reg(va_addr);
	RX_DEF_LOG("0x%08x = 0x%08x\n", u4Addr, u4Data);

	return 0;
}

static u32
hdmi2cmd_read_flied(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 u4Data;
	u32 u4Addr;
	u32 u4Msk;
	void __iomem *va_addr;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("rf:0xPPPPPPPP,0xPPPP\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x,0x%x", &u4Addr,  &u4Msk);
	if (ret != 2)
		return 0;

	if ((u4Addr >= 0x1c400000) && (u4Addr < 0x1c400000 + 0x1000))
		va_addr = (RX_ADR + ((u4Addr - 0x1c400000)));
	else if ((u4Addr >= 0x1c300000) && (u4Addr < 0x1c300000 + 0x1000))
		va_addr = (TX_ADR + (u4Addr - 0x1c300000));
	else if ((u4Addr >= 0x11d60000) && (u4Addr < 0x11d60000 + 0x3000))
		va_addr = (A_ADR + (u4Addr - 0x11d60000));
	else if ((u4Addr >= 0x11d5f000) && (u4Addr < 0x11d5f000 + 0x1000))
		va_addr = (V_ADR + (u4Addr - 0x11d5f000));
	else if ((u4Addr >= 0x10014800) && (u4Addr < 0x10014800 + 0x400))
		va_addr = (S_ADR + (u4Addr - 0x10014800));
	else if ((u4Addr >= 0x10014400) && (u4Addr < 0x10014400 + 0x400))
		va_addr = (E_ADR + (u4Addr - 0x10014400));
	else if ((u4Addr >= 0x1000c000) && (u4Addr < 0x1000c000 + 0x1000))
		va_addr = (AP_ADR + (u4Addr - 0x1000c000));
	else if ((u4Addr >= 0x10000000) && (u4Addr < 0x10000000 + 0x1000))
		va_addr = (CK_ADR + (u4Addr - 0x10000000));
	else
		return 0;

	u4Data = Read16Msk(va_addr, u4Msk);

	RX_DEF_LOG("rf: 0x%08x = 0x%08x\n", u4Addr, u4Data);

	ret = sprintf(debug_buffer, "0x%x", u4Data);
	if (ret < 0)
		return 0;

	return 0;
}

static u32
hdmi2cmd_write_flied(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 u4Data;
	u32 u4Addr;
	u32 u4Msk;
	void __iomem *va_addr;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("wf:0xPPPPPPPP,0xPPPP,0xPPPP\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x,0x%x,0x%x", &u4Addr, &u4Msk, &u4Data);
	if (ret != 3)
		return 0;

	if ((u4Addr >= 0x1c400000) && (u4Addr < 0x1c400000 + 0x1000))
		va_addr = (RX_ADR + ((u4Addr - 0x1c400000)));
	else if ((u4Addr >= 0x1c300000) && (u4Addr < 0x1c300000 + 0x1000))
		va_addr = (TX_ADR + (u4Addr - 0x1c300000));
	else if ((u4Addr >= 0x11d60000) && (u4Addr < 0x11d60000 + 0x3000))
		va_addr = (A_ADR + (u4Addr - 0x11d60000));
	else if ((u4Addr >= 0x11d5f000) && (u4Addr < 0x11d5f000 + 0x1000))
		va_addr = (V_ADR + (u4Addr - 0x11d5f000));
	else if ((u4Addr >= 0x10014800) && (u4Addr < 0x10014800 + 0x400))
		va_addr = (S_ADR + (u4Addr - 0x10014800));
	else if ((u4Addr >= 0x10014400) && (u4Addr < 0x10014400 + 0x400))
		va_addr = (E_ADR + (u4Addr - 0x10014400));
	else if ((u4Addr >= 0x1000c000) && (u4Addr < 0x1000c000 + 0x1000))
		va_addr = (AP_ADR + (u4Addr - 0x1000c000));
	else if ((u4Addr >= 0x10000000) && (u4Addr < 0x10000000 + 0x1000))
		va_addr = (CK_ADR + (u4Addr - 0x10000000));
	else
		return 0;

	w_reg(va_addr, (u4Msk << 16) | u4Data);
	u4Data = r_reg(va_addr);
	RX_DEF_LOG("wf: 0x%08x = 0x%08x\n", u4Addr, u4Data);

	return 0;
}

static u32
hdmi2cmd_enable(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 aud_sel = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0)
		return 0;

	ret = kstrtouint(str_p, 10, &aud_sel);

	if (aud_sel == 1) {
		hdmi_rx_power_on(myhdmi);
		hdmi2_set_state(myhdmi, rx_no_signal);
		myhdmi->portswitch = HDMI_SWITCH_1;
		hdmi_rx_task_start(myhdmi, TRUE);
		aud2_enable(myhdmi, TRUE);
	} else {
		myhdmi->portswitch = HDMI_SWITCH_INIT;
		hdmi_rx_task_start(myhdmi, FALSE);
		aud2_enable(myhdmi, FALSE);
		hdmi_rx_power_off(myhdmi);
	}
	return 0;
}

static u32
hdmi2cmd_task_on(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 u1On = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("0: off; 1:on\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &u1On);
	if (u1On == 0)
		hdmi_rx_task_start(myhdmi, FALSE);
	else
		hdmi_rx_task_start(myhdmi, TRUE);

	return 0;
}

static u32
hdmi2cmd_crc_check(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 ntry = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("crc retry : retry: retry times\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &ntry);

	hdmi2com_crc(myhdmi, ntry);

	return 0;
}

static u32
hdmi2cmd_phy_crc_check(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 ntry = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("rx phy crc retry : retry: retry times\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &ntry);

	hdmi2com_ana_crc(myhdmi, ntry);
	return 0;
}

static u32
hdmi2cmd_test_mode(struct MTK_HDMI *myhdmi, char *str_p)
{
	struct HDMIRX_DEV_INFO dev_info;
	struct HDMIRX_VID_PARA vid_para;
	struct HDMIRX_AUD_INFO aud_info;
	u32 temp, temp1;
	u32 i;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("3:force hdcp mode\n");
		RX_DEF_LOG("4:get hdcp2 repeater para\n");
		RX_DEF_LOG("10:set myhdmi->my_debug\n");
		RX_DEF_LOG("11:set myhdmi->my_debug1\n");
		RX_DEF_LOG("20:reset rx\n");
		RX_DEF_LOG("21:hpd chg\n");
		RX_DEF_LOG("30:edid find\n");
		return 0;
	}

	ret = sscanf(str_p, "%d,%d", &temp, &temp1);
	if (ret != 2)
		return 0;

	if (temp > 10000)
		return 0;

	if (temp == 5) {
		hdmi2com_get_stream_id_type(myhdmi);
	} else if (temp == 10) {
		RX_DEF_LOG("[RX]myhdmi->my_debug=%x\n", temp1);
		myhdmi->my_debug = temp1;
	} else if (temp == 11) {
		RX_DEF_LOG("[RX]myhdmi->my_debug1=%x\n", temp1);
		myhdmi->my_debug1 = temp1;
	} else if (temp == 12) {
		RX_DEF_LOG("[RX]myhdmi->my_debug2=%x\n", temp1);
		myhdmi->my_debug2 = temp1;
	} else if (temp == 13) {
		RX_DEF_LOG("[RX]myhdmi->reset_wait_time:%d to %d ms\n",
			myhdmi->reset_wait_time, temp1);
		myhdmi->reset_wait_time = temp1;
	} else if (temp == 14) {
		RX_DEF_LOG("[RX]myhdmi->timing_unstable_time:%d to %d ms\n",
			myhdmi->timing_unstable_time, temp1);
		myhdmi->timing_unstable_time = temp1;
	} else if (temp == 15) {
		RX_DEF_LOG("[RX]myhdmi->wait_data_stable_count:%d to %d ms\n",
			myhdmi->wait_data_stable_count, temp1);
		myhdmi->wait_data_stable_count = temp1;
	} else if (temp == 20) {
		RX_DEF_LOG("[RX]reset rx count = %d\n", temp1);
		myhdmi->stable_count = 0;
		myhdmi->hdcp_stable_count = 0;
		for (i = 0; i < temp1; i++) {
			hdmi2_set_state(myhdmi, rx_detect_ckdt);
			msleep(5000);
			RX_DEF_LOG("[RX]reset rx count=%d,count=%d,hdcp=%d\n",
				i, myhdmi->stable_count,
				myhdmi->hdcp_stable_count);
		}
	} else if (temp == 21) {
		RX_DEF_LOG("[RX]hpd 5 chg count=%d\n", temp1);
		myhdmi->stable_count = 0;
		myhdmi->hdcp_stable_count = 0;
		for (i = 0; i < temp1; i++) {
			hdmi2_set_hpd_low_timer(myhdmi, 30);
			hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
			msleep(5000);
			RX_DEF_LOG("[RX]hpd chg=%d,count=%d,hdcp=%d\n", i,
				myhdmi->stable_count,
				myhdmi->hdcp_stable_count);
		}
	} else if (temp == 22) {
		RX_DEF_LOG("[RX]hpd chg count=%d\n", temp1);
		myhdmi->stable_count = 0;
		myhdmi->hdcp_stable_count = 0;
		for (i = 0; i < temp1; i++) {
			hdmi2_set_hpd_low_timer(myhdmi, 30);
			hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
			msleep(10000);
			RX_DEF_LOG("[RX]hpd 10 chg=%d,count=%d,hdcp=%d\n", i,
				myhdmi->stable_count,
				myhdmi->hdcp_stable_count);
		}
	} else if (temp == 23) {
		RX_DEF_LOG("[RX]stable_count=%d,hdcp_stable_count=%d\n",
			myhdmi->stable_count, myhdmi->hdcp_stable_count);
		myhdmi->stable_count = 0;
		myhdmi->hdcp_stable_count = 0;
	} else if (temp == 24) {
		RX_DEF_LOG("[RX]===== stress test (%d) ======\n",
			temp1);
		myhdmi->stable_count = 0;
		myhdmi->hdcp_stable_count = 0;
		for (i = 0; i < temp1; i++) {
			hdmi2_set_hpd_low_timer(myhdmi, 30);
			hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
			msleep(5000);
			RX_DEF_LOG("[RX]===stress test(%d/%d/%d), %d\n",
				i, temp1,
				myhdmi->stable_count,
				myhdmi->hdcp_stable_count);
			hdmi2com_crc(myhdmi, 3);
			if (myhdmi->crc0 != myhdmi->slt_crc) {
				RX_DEF_LOG("[RX] %s. crc0=0x%x, slt_crc=0x%x\n",
					"--->>>find err, continue",
					myhdmi->crc0,
					myhdmi->slt_crc);
			}
		}
	}  else if (temp == 25) {
		RX_DEF_LOG("[RX]===== find error (%d) ======\n",
			temp1);
		myhdmi->stable_count = 0;
		myhdmi->hdcp_stable_count = 0;
		for (i = 0; i < temp1; i++) {
			hdmi2_set_hpd_low_timer(myhdmi, 30);
			hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
			msleep(5000);
			RX_DEF_LOG("[RX]=== to find err(%d/%d/%d), %d\n",
				i, temp1,
				myhdmi->stable_count,
				myhdmi->hdcp_stable_count);
			hdmi2com_crc(myhdmi, 3);
			if (myhdmi->crc0 != myhdmi->slt_crc) {
				RX_DEF_LOG("[RX] %s. crc0=0x%x, slt_crc=0x%x\n",
					"--->>>find err, break",
					myhdmi->crc0,
					myhdmi->slt_crc);
				return 0;
			}
		}
	} else if (temp == 30) {
		RX_DEF_LOG("[RX]is support hdcp2.x = %d\n", temp1);
		if (temp1 == 0)
			hdmi2com_disable_hdcp2x(myhdmi, TRUE);
		else
			hdmi2com_disable_hdcp2x(myhdmi, FALSE);
	} else if (temp == 40) {
		RX_DEF_LOG("[RX]hdmi_mode_setting = %d\n", temp1);
		myhdmi->hdmi_mode_setting = temp1;
	} else if (temp == 50) {
		hdmi2_show_emp(myhdmi);
	} else if (temp == 60) {
		if (temp1 == 0) {
			RX_DEF_LOG("[RX]hdcp_mode setting HDCP_RECEIVER\n");
			hdmi2_hdcp_set_receiver(myhdmi);
		} else if (temp1 == 1) {
			RX_DEF_LOG("[RX]hdcp_mode setting HDCP_REPEATER\n");
			hdmi2_hdcp_set_repeater(myhdmi);
		} else
			RX_DEF_LOG("[RX]hdcp_mode setting Bad Parameter\n");
	} else if (temp == 61) {
		RX_DEF_LOG("[RX] register hdmitx for repeater mode\n");
	} else if (temp == 100) {
		RX_DEF_LOG("[RX]enable phy ckdt irq, 0x%x\n", temp1);
		vHDMI21PhyIRQEnable(myhdmi, temp1, TRUE);
	} else if (temp == 101) {
		RX_DEF_LOG("[RX]disable phy ckdt irq, 0x%x\n", temp1);
		vHDMI21PhyIRQEnable(myhdmi, temp1, FALSE);
	} else if (temp == 110) {
		RX_DEF_LOG("[RX]power domain, %d\n", temp1);
		if (temp1 == 1) {
			hdmirx_toprgu_rst(myhdmi);
			hdmi_rx_power_on(myhdmi);
		} else if (temp1 == 2) {
			vHDMI21PhyPowerDown(myhdmi);
			hdmi_rx_power_off(myhdmi);
		}
	} else if (temp == 111) {
		RX_DEF_LOG("[RX]hdmirx_set_vcore: %d\n", temp1);
		hdmirx_set_vcore(myhdmi, temp1);
	} else if (temp == 112) {
		RX_DEF_LOG("[RX]auido start : %d, %d\n",
			temp1, myhdmi->aud_enable);
		if (temp1) {
			RX_AUDIO_LOG("[RX][CMD]aud2 Start\n");
			aud2_set_state(myhdmi, ASTATE_RequestAudio);
		} else {
			RX_AUDIO_LOG("[RX][CMD]aud2 stop\n");
			aud2_set_state(myhdmi, ASTATE_AudioOff);
		}
	} else if (temp == 113) {
		RX_DEF_LOG("[RX]sel_edid=%d\n", temp1);
		myhdmi->sel_edid = temp1;
		myhdmi->chg_edid = TRUE;
	} else if (temp == 80) {
		io_get_dev_info(myhdmi, &dev_info);
	} else if (temp == 81) {
		io_get_vid_info(myhdmi, &vid_para);
	} else if (temp == 82) {
		io_get_aud_info(myhdmi, &aud_info);
	}

	return 0;
}

static u32
hdmi2cmd_hw_init(struct MTK_HDMI *myhdmi)
{

	RX_DEF_LOG("hw init for verify\n");
	hdmi2_hw_init(myhdmi);
	hdmi2com_ddc_path_config(myhdmi, TRUE);

	return 0;
}

static u32
hdmi2cmd_capture(struct MTK_HDMI *myhdmi)
{
	u32 temp, count;

	usleep_range(200000, 200050);

	hdmi2com_hdcp_reset(myhdmi);

	hdmi2_set_hpd_high(myhdmi, HDMI_SWITCH_1);
	hdmi2_tmds_term_ctrl(myhdmi, TRUE);
	hdmi2_packet_data_init(myhdmi);
	hdmi2com_path_config(myhdmi);

	RX_DEF_LOG("[RX]wait pdtclk\n");
	count = 0;
	for (temp = 0; temp < 200; temp++) {
		usleep_range(10000, 10050);
		if ((hdmi2com_is_pdt_clk_changed(myhdmi) == FALSE) &&
			(hdmi2com_is_ckdt_1(myhdmi))) {
			count++;
		} else {
			count = 0;
		}
		if (count > 4) {
			RX_DEF_LOG("[RX]pdtclk stable\n");
			break;
		}
	}

	if (hdmi2com_is_ckdt_1(myhdmi))
		RX_DEF_LOG("[RX]detected ck signal\n");
	else
		RX_DEF_LOG("[RX]can not detected ck signal\n");

	vHDMI21PHY14(myhdmi);
	usleep_range(1000, 1050);
	vHDMI21PhySetting(myhdmi);
	usleep_range(1000, 1050);

	hdmi2com_dig_phy_rst1(myhdmi);
	hdmi2com_deep_rst1(myhdmi);
	usleep_range(1000, 1050);
	hdmi2com_vid_clk_changed_clr(myhdmi);
	hdmi2_set_digital_reset_start_time(myhdmi);

	usleep_range(300000, 300050);

	if (hdmi2com_is_scdt_1(myhdmi))
		RX_DEF_LOG("[RX]detected de signal\n");
	else
		RX_DEF_LOG("[RX]can not detected de signal\n");

	hdmi2_timer_handle(myhdmi);

	if (hdmi2com_is_ckdt_1(myhdmi) && hdmi2com_is_scdt_1(myhdmi)) {
		hdmi2_get_pkt(myhdmi);
		hdmi2_color_space_convert(myhdmi);
		/* get video parameter */
		hdmi2_get_vid_para(myhdmi);
		/* set hdmitx repeater mode */
		txapi_rpt_mode(myhdmi, myhdmi->hdmi_mode_setting);
		/* config output path */
		hdmi2com_cfg_out(myhdmi);
		/* set hdmitx */
		txapi_cfg_video(myhdmi);
		/* then search timing */
		hdmi2_show_status(myhdmi);
		dvi2_show_timing_detail(myhdmi);
	} else {
		RX_DEF_LOG("[RX]oh my god,no ckdt or scdt signal!\n");
	}

	return 0;
}

static u32
hdmi2cmd_phy_reset(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 type;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("phy reset for verify\n");
		RX_DEF_LOG("HDMI_RST_ALL=1\n");
		RX_DEF_LOG("HDMI_RST_EQ=2\n");
		RX_DEF_LOG("HDMI_RST_DEEPCOLOR=3\n");
		RX_DEF_LOG("HDMI_RST_FIXEQ=4\n");
		RX_DEF_LOG("HDMI_RST_RTCK=5\n");
		RX_DEF_LOG("HDMI_RST_DIGITALPHY=6\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x", &type);
	if (ret != 1)
		return 0;

	if (type & 0xFF) {
		RX_DEF_LOG("phy reset for verify: %d\n", type);
		hdmi2_phy_reset(myhdmi, type);
	} else if (type & 0x100) {

	} else if (type & 0x200) {
		vHDMI21PhySetting(myhdmi);
	} else if (type & 0x400) {
		RX_DEF_LOG("digital reset\n");
		hdmi2com_dig_phy_rst1(myhdmi);
		hdmi2com_deep_rst1(myhdmi);
	}

	return 0;
}

static u32
hdmi2cmd_check(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 sel = 0, temp;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("0:check status\n");
		RX_DEF_LOG("1:check audio status\n");
		RX_DEF_LOG("2:check analog status\n");
		RX_DEF_LOG("10:check bypass path(RX->DGI->RGB2HDMI->TX)\n");
		RX_DEF_LOG("20:check rx digital clk\n");
		RX_DEF_LOG("21:check rx analog clk\n");
		RX_DEF_LOG("22:check hdck clk\n");
		RX_DEF_LOG("23:check audio clk\n");
		RX_DEF_LOG("30:check dgi/rgb2hdmi2/osd clk\n");
		RX_DEF_LOG("31:repear dgi/rgb2hdmi2/osd clk\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &sel);

	if (sel == 0) {
		RX_DEF_LOG(">>>check status\n");
		if (hdmi2com_is_ckdt_1(myhdmi))
			RX_DEF_LOG("detected ck signal\n");
		else
			RX_DEF_LOG("can not detected ck signal\n");
		if (hdmi2com_is_scdt_1(myhdmi))
			RX_DEF_LOG("detected de signal\n");
		else
			RX_DEF_LOG("can not detected de signal\n");
		hdmi2_timer_handle(myhdmi);
		if (hdmi2com_is_ckdt_1(myhdmi)) {
			/* set tx */
			hdmi2_color_space_convert(myhdmi);

			hdmi2_show_status(myhdmi);
			dvi2_show_timing_detail(myhdmi);
		} else {
			RX_DEF_LOG("no ck signal, not need to check others\n");
		}
	} else if (sel == 10) {
		RX_DEF_LOG(">>>check audio\n");
		aud2_show_status(myhdmi);
	} else if (sel == 20) {
		RX_DEF_LOG(">>>check clk\n");
	} else if (sel == 21) {
		RX_DEF_LOG(">>>check audio clk\n");
		temp =  hdmi_fmeter(2);
		RX_DEF_LOG("[RX]AD_HDMIRX_APLL_CK=%d\n", temp);
	}

	return 0;
}

static u32
hdmi2cmd_phy(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 temp = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("phy:[item]\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &temp);
	RX_DEF_LOG("phy:%d\n", temp);

	return 0;
}

static u32
hdmi2cmd_rx_port(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 reg_sel = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("set port\n");
		RX_DEF_LOG("  1<<0 : HPD\n");
		RX_DEF_LOG("  1<<1 : Rsen\n");
		return 0;
	}

	ret = kstrtouint(str_p, 16, &reg_sel);

	if (reg_sel & (1 << 0)) {
		RX_DEF_LOG("HPD is high\n");
		hdmi2_set_hpd_high(myhdmi, HDMI_SWITCH_1);
	} else {
		RX_DEF_LOG("HPD is low\n");
		hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
	}
	if (reg_sel & (1 << 1)) {
		RX_DEF_LOG("Rsen is high\n");
		hdmi2_tmds_term_ctrl(myhdmi, TRUE);
	} else {
		RX_DEF_LOG("Rsen is low\n");
		hdmi2_tmds_term_ctrl(myhdmi, FALSE);
	}

	return 0;
}

static u32
hdmi2cmd_irq_register(struct MTK_HDMI *myhdmi)
{
	return 0;
}

static u32
hdmi2cmd_irq_enable(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 int_en;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("isr enable\n");
		RX_DEF_LOG("#define HDMI_RX_CKDT_INT (1<<0)\n");
		RX_DEF_LOG("#define HDMI_RX_SCDT_INT (1<<1)\n");
		RX_DEF_LOG("#define HDMI_RX_VSYNC_INT (1<<2)\n");
		RX_DEF_LOG("#define HDMI_RX_CDRADIO_INT (1<<3)\n");
		RX_DEF_LOG("#define HDMI_RX_SCRAMBLING_INT (1<<4)\n");
		RX_DEF_LOG("#define HDMI_RX_FSCHG_INT (1<<5)\n");
		RX_DEF_LOG("#define HDMI_RX_DSDPKT_INT (1<<7)\n");
		RX_DEF_LOG("#define HDMI_RX_HBRPKT_INT (1<<8)\n");
		RX_DEF_LOG("#define HDMI_RX_CHCHG_INT (1<<9)\n");
		RX_DEF_LOG("#define HDMI_RX_AVI_INT (1<<10)\n");
		RX_DEF_LOG("#define HDMI_RX_HDMI_MODE_CHG (1<<13)\n");
		RX_DEF_LOG("#define HDMI_RX_HDMI_NEW_GCP (1<<14)\n");
		RX_DEF_LOG("#define HDMI_RX_PDTCLK_CHG (1<<15)\n");
		RX_DEF_LOG("#define HDMI_RX_H_UNSTABLE (1<<16)\n");
		RX_DEF_LOG("#define HDMI_RX_V_UNSTABLE (1<<17)\n");
		RX_DEF_LOG("#define HDMI_RX_SET_MUTE (1<<18)\n");
		RX_DEF_LOG("#define HDMI_RX_CLEAR_MUTE (1<<19)\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x", &int_en);
	if (ret != 1)
		return 0;

	if ((int_en & 0x80000000) == 0)
		hdmi2com_set_isr_enable(myhdmi, TRUE, int_en);
	else
		hdmi2com_set_isr_enable(myhdmi, FALSE, int_en);

	return 0;
}

static u32
hdmi2cmd_scdc(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 scdc_sel = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("scdc\n");
		RX_DEF_LOG("0 : auto 1/40 and srambling disable\n");
		RX_DEF_LOG(
			"1 : force clk radio to 1/40 and srambling enable\n");
		RX_DEF_LOG(
			"2 : force clk radio to 1/10 and srambling enable\n");
		RX_DEF_LOG("3 : force clk radio to 1/40 and srambling auto\n");
		RX_DEF_LOG("4 : force clk radio to 1/10 and srambling auto\n");
		RX_DEF_LOG(
			"5 : force clk radio to 1/10 and srambling disable\n");
		RX_DEF_LOG("0x100 : scdc clear\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &scdc_sel);

	if (scdc_sel == 100) {
		RX_DEF_LOG("1 : scdc clear\n");
		hdmi2com_scdc_status_clr(myhdmi);
	} else {
		hdmi2com_force_scdc(myhdmi, scdc_sel);
	}

	return 0;
}

static u32
hdmi2cmd_mute(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 reg_sel = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("  1 : video mute\n");
		RX_DEF_LOG("  0 : video unmute\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &reg_sel);

	if (reg_sel)
		hdmi2_set_video_mute(myhdmi, TRUE, TRUE);
	else
		hdmi2_set_video_mute(myhdmi, FALSE, TRUE);

	return 0;
}

static u32
hdmi2cmd_video_out_config(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]config video output\n");
	hdmi2_color_space_convert(myhdmi);
	hdmi2_avi_info_parser(myhdmi);
	hdmi2_video_setting(myhdmi);

	return 0;
}

static u32
hdmi2cmd_infoframe(struct MTK_HDMI *myhdmi)
{
	hdmi2_show_all_infoframe(myhdmi);

	return 0;
}

/*
 *	   step for audio verify\n
 *	   >>>dsd\n
 *	   step1 : <aud 0> enable aud1pll\n
 *	   step2 : <aud 2> reset AACR and fifo\n
 *	   step3 : <audout> dsd:(2 0 0 0), set audio output format\n
 *	   step4 : <aud 4> reset fifo\n
 *	   step5 : <audcheck> check audio info\n
 *	   step6 : <audpath> set audio output path\n
 *	   >>>hdr\n
 *	   step1 : <aud 0> enable aud1pll\n
 *	   step2 : <aud 2> reset AACR and fifo\n
 *	   step3 : <audout> hbr:(1 0 0 0), set audio output format\n
 *	   step4 : <aud 4> reset fifo\n
 *	   step5 : <audcheck> check audio info\n
 *	   step6 : <audpath> set audio output path\n
 *	   >>>pcm\n
 *	   step1 : <aud 0> enable aud1pll\n
 *	   step2 : <aud 2> reset AACR and fifo\n
 *	   step3 : <audout> pcm:(0 0 0 0x0f), set audio output format\n
 *	   step4 : <audcheck> check audio info\n
 *	   step5 : <audpath> set audio output path
 */
static u32
hdmi2cmd_audio(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 aud_sel = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("0:enable audio 1 pll\n");
		RX_DEF_LOG("1:enable audio 2 pll\n");
		RX_DEF_LOG("2:audio reset all\n");
		RX_DEF_LOG("3:audio reset acr\n");
		RX_DEF_LOG("4:audio reset fifo\n");
		RX_DEF_LOG("5:set tx audio input\n");
		RX_DEF_LOG("10:disable audio 1 pll\n");
		RX_DEF_LOG("11:disable audio 2 pll\n");
		RX_DEF_LOG("100:config pcm/spdif audio\n");
		RX_DEF_LOG("101:config hbr audio\n");
		RX_DEF_LOG("102:config dsd audio\n");
		RX_DEF_LOG("200:set audio by detectting audio packet\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &aud_sel);

	RX_DEF_LOG("audio for verify\n");

	if (aud_sel == 0) {
		RX_DEF_LOG("enable audio pll\n");
		hdmi2com_audio_pll_config(myhdmi, TRUE);
	} else if (aud_sel == 2) {
		RX_DEF_LOG("audio reset all\n");
		hdmi2_aud_reset_for_verify(myhdmi);
	} else if (aud_sel == 3) {
		RX_DEF_LOG("audio reset acr\n");
		hdmi2com_aud_acr_rst(myhdmi);
	} else if (aud_sel == 4) {
		RX_DEF_LOG("audio reset fifo\n");
		hdmi2com_aud_fifo_rst(myhdmi);
		hdmi2com_aud_fifo_err_clr(myhdmi);
	} else if (aud_sel == 5) {
		RX_DEF_LOG("set tx audio input\n");
	} else if (aud_sel == 10) {
		RX_DEF_LOG("disable audio pll\n");
		hdmi2com_audio_pll_config(myhdmi, FALSE);
	} else if (aud_sel == 100) {
		RX_DEF_LOG(">>> pcm/spdif seeting,please set audpll first\n");

		RX_DEF_LOG("step1:set hdmi rx audio output\n");
		aud2_audio_out(myhdmi, 0, 0, 0, 0x0f);

		RX_DEF_LOG("step2:reset rx acr and fifo, clear fifo\n");
		hdmi2_aud_reset_for_verify(myhdmi);

		aud2_show_status(myhdmi);
	} else if (aud_sel == 101) {
		RX_DEF_LOG(">>> hbr seeting,please set audpll first\n");

		RX_DEF_LOG("step1:set hdmi rx audio output\n");
		aud2_audio_out(myhdmi, 1, 0, 0, 0x0f);

		RX_DEF_LOG("step2:reset rx acr and fifo, clear fifo\n");
		hdmi2_aud_reset_for_verify(myhdmi);

		aud2_show_status(myhdmi);
	} else if (aud_sel == 102) {
		RX_DEF_LOG(">>> dsd seeting,please set audpll first\n");

		RX_DEF_LOG("step1:set hdmi rx audio output\n");
		aud2_audio_out(myhdmi, 2, 0, 0, 0x0f);

		RX_DEF_LOG("step2:reset rx acr and fifo, clear fifo\n");
		hdmi2_aud_reset_for_verify(myhdmi);

		aud2_show_status(myhdmi);
	} else if (aud_sel == 200) {
		RX_DEF_LOG(">>> set audio by detectting audio packet\n");

		RX_DEF_LOG("step1:enable audpll\n");
		hdmi2com_audio_pll_config(myhdmi, TRUE);

		RX_DEF_LOG("step2:set hdmi rx audio output\n");
		if (hdmi2com_is_hd_audio(myhdmi)) {
			RX_DEF_LOG("detected hbr audio packet\n");
			aud2_audio_out(myhdmi, 1, 0, 0, 0x0f);
		} else if (hdmi2com_is_dsd_audio(myhdmi)) {
			RX_DEF_LOG("detected dsd audio packet\n");
			aud2_audio_out(myhdmi, 2, 0, 0, 0x0f);
		} else {
			RX_DEF_LOG("detected audio packet\n");
			aud2_audio_out(myhdmi, 0, 0, 0, 0x0f);
		}

		RX_DEF_LOG("step3:reset rx acr and fifo, clear fifo isr\n");
		hdmi2_aud_reset_for_verify(myhdmi);

		aud2_show_status(myhdmi);
	}

	return 0;
}

static u32
hdmi2cmd_audio_check(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("check audio info for verify\n");
	aud2_show_status(myhdmi);

	return 0;
}

static u32
hdmi2cmd_audio_out_config(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 aud_type;
	u32 aud_fmt;
	u32 aud_fs;
	u32 aud_ch;
	int ret;

	RX_DEF_LOG("audio output for verify\n");

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("arg1:aud_type,0: iic/spdif/; 1:hbr; 2:dsd\n");
		RX_DEF_LOG("arg2:aud_fmt, LJ or iis, refer to format define\n");
		RX_DEF_LOG("arg3:aud_fs, refer to Fs define\n");
		RX_DEF_LOG("arg4:aud_ch, enable channel\n");
		return 0;
	}

	ret = sscanf(str_p, "%d,%d,%d,%d",
		&aud_type, &aud_fmt, &aud_fs, &aud_ch);
	if (ret != 4)
		return 0;

	aud2_audio_out(myhdmi, aud_type, aud_fmt, aud_fs, aud_ch);
	hdmi2_aud_reset_for_verify(myhdmi);

	return 0;
}

static u32
hdmi2cmd_audio_format_config(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 aud_fmt = 0;
	int ret;

	RX_DEF_LOG("audio format for verify\n");
	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("format, 0:lj; 1:rj; 2:i2s\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &aud_fmt);

	if (aud_fmt == 0)
		aud2_set_i2s_format(myhdmi, FALSE, HDMI_LJT_24BIT, TRUE);
	else if (aud_fmt == 1)
		aud2_set_i2s_format(myhdmi, FALSE, HDMI_RJT_24BIT, TRUE);
	else if (aud_fmt == 2)
		aud2_set_i2s_format(myhdmi, FALSE, HDMI_I2S_24BIT, TRUE);

	return 0;
}

static u32
hdmi2cmd_audio_mute(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 aud_fmt = 0;
	int ret;

	RX_DEF_LOG("audio pin for verify\n");
	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("1:mute audio; !=1:unmute\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &aud_fmt);

	if (aud_fmt == 1)
		aud2_audio_mute(myhdmi, TRUE);
	else
		aud2_audio_mute(myhdmi, FALSE);

	return 0;
}

static u32
hdmi2cmd_edid(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 arg1 = 0;
	u8 edid_buf[128];
	int ret;

	RX_DEF_LOG("edid for verify\n");
	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("0: load edid_4k edid(2 block)\n");
		RX_DEF_LOG("1: load cts_4k_edid edid(2 block)\n");
		RX_DEF_LOG("2: load edid_4k edid(2 block)\n");
		RX_DEF_LOG("3: load test_4_edid edid(4 block)\n");
		RX_DEF_LOG("0x10: show block0 sram edid\n");
		RX_DEF_LOG("0x11: show block1 sram edid\n");
		RX_DEF_LOG("0x12: show block2 sram edid\n");
		RX_DEF_LOG("0x13: show block3 sram edid\n");
		RX_DEF_LOG("0x20: show block0/1 sram edid\n");
		RX_DEF_LOG("0x21: show block0/1/2/3 sram edid\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &arg1);

	if (arg1 == 0) {
		memcpy(&edid_buf[0], &edid_4k[0], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 0, &edid_buf[0]);
		memcpy(&edid_buf[0], &edid_4k[128], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 1, &edid_buf[0]);
	} else if (arg1 == 1) {
		memcpy(&edid_buf[0], &cts_4k_edid[0], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 0, &edid_buf[0]);
		memcpy(&edid_buf[0], &cts_4k_edid[128], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 1, &edid_buf[0]);
	} else if (arg1 == 2) {
		memcpy(&edid_buf[0], &test_4_edid[0], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 0, &edid_buf[0]);
		memcpy(&edid_buf[0], &test_4_edid[128], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 1, &edid_buf[0]);
		memcpy(&edid_buf[0], &test_4_edid[256], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 2, &edid_buf[0]);
		memcpy(&edid_buf[0], &test_4_edid[384], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 3, &edid_buf[0]);
	} else if (arg1 == 3) {
		memcpy(&edid_buf[0], &pdYMH1800Edid[0], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 0, &edid_buf[0]);
		memcpy(&edid_buf[0], &pdYMH1800Edid[128], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 1, &edid_buf[0]);
	} else if (arg1 == 4) {
		memcpy(&edid_buf[0], &vrr30to60[0], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 0, &edid_buf[0]);
		memcpy(&edid_buf[0], &vrr30to60[128], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 1, &edid_buf[0]);
	} else if (arg1 == 5) {
		memcpy(&edid_buf[0], &vrr24to60[0], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 0, &edid_buf[0]);
		memcpy(&edid_buf[0], &vrr24to60[128], 128);
		hdmi2com_write_edid_to_sram(myhdmi, 1, &edid_buf[0]);
	} else if (arg1 == 10) {
		hdmi2com_show_sram_edid(myhdmi, 0);
	} else if (arg1 == 11) {
		hdmi2com_show_sram_edid(myhdmi, 1);
	} else if (arg1 == 12) {
		hdmi2com_show_sram_edid(myhdmi, 2);
	} else if (arg1 == 13) {
		hdmi2com_show_sram_edid(myhdmi, 3);
	} else if (arg1 == 20) {
		hdmi2com_show_sram_edid(myhdmi, 0);
		hdmi2com_show_sram_edid(myhdmi, 1);
	} else if (arg1 == 21) {
		hdmi2com_show_sram_edid(myhdmi, 0);
		hdmi2com_show_sram_edid(myhdmi, 1);
		hdmi2com_show_sram_edid(myhdmi, 2);
		hdmi2com_show_sram_edid(myhdmi, 3);
	}

	return 0;
}

static u32
hdmi2cmd_hdcp_init(struct MTK_HDMI *myhdmi)
{
#if HDMIRX_YOCTO
	RX_DEF_LOG("hdcp init start\n");
	hdcp2_load_hdcp_key(myhdmi);
	hdmi2com_hdcp_init(myhdmi);
	hdmi2com_set_hdcp1x_rpt_mode(myhdmi, 0);
	hdmi2com_set_hdcp2x_rpt_mode(myhdmi, 0);
	hdmi2com_hdcp_reset(myhdmi);
	RX_DEF_LOG("hdcp init done\n");
#endif
	return 0;
}

static u32
hdmi2cmd_hdcp(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 temp = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("0:show hdcp2x irq\n");
		RX_DEF_LOG("1:clear hdcp2x irq\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &temp);

	if (temp == 0)
		hdmi2com_check_hdcp2x_irq(myhdmi);
	else if (temp == 1)
		hdmi2com_hdcp2x_irq_clr(myhdmi);
	else if (temp == 2) {
		RX_DEF_LOG("[RX]hdcp reset\n");
		hdmi2com_hdcp_reset(myhdmi);
	} else if (temp == 3) {
		RX_DEF_LOG("[RX]hdcp init\n");
		hdmi2com_hdcp_init(myhdmi);
	}

	return 0;
}

static u32
hdmi2cmd_hdcp_check(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	hdmi2_hdcp_status(myhdmi);

	if (hdmi2com_upstream_is_hdcp2x_device(myhdmi)) {
		RX_DEF_LOG("upstream is hdcp2.x device\n");

		temp = (u32)hdmi2com_hdcp2x_status(myhdmi);
		if (temp & HDMI_RX_HDCP2_STATUS_AUTH_DONE)
			RX_DEF_LOG("HDMI_RX_HDCP2_STATUS_AUTH_DONE\n");
		if (temp & HDMI_RX_HDCP2_STATUS_AUTH_FAIL)
			RX_DEF_LOG("HDMI_RX_HDCP2_STATUS_AUTH_FAIL\n");
		if (temp & HDMI_RX_HDCP2_STATUS_CCHK_DONE)
			RX_DEF_LOG("HDMI_RX_HDCP2_STATUS_CCHK_DONE\n");
		if (temp & HDMI_RX_HDCP2_STATUS_CCHK_FAIL)
			RX_DEF_LOG("HDMI_RX_HDCP2_STATUS_CCHK_FAIL\n");
		if (temp & HDMI_RX_HDCP2_STATUS_AKE_SENT_RCVD)
			RX_DEF_LOG("HDMI_RX_HDCP2_STATUS_AKE_SENT_RCVD\n");
		if (temp & HDMI_RX_HDCP2_STATUS_SKE_SENT_RCVD)
			RX_DEF_LOG("HDMI_RX_HDCP2_STATUS_SKE_SENT_RCVD\n");
		if (temp & HDMI_RX_HDCP2_STATUS_CERT_SENT_RCVD)
			RX_DEF_LOG("HDMI_RX_HDCP2_STATUS_CERT_SENT_RCVD\n");
		if (temp & HDMI_RX_HDCP2_STATUS_DECRYPT_SUCCESS)
			RX_DEF_LOG("HDMI_RX_HDCP2_STATUS_DECRYPT_SUCCESS\n");
		if (temp & HDMI_RX_HDCP2_STATUS_DECRYPT_FAIL)
			RX_DEF_LOG("HDMI_RX_HDCP2_STATUS_DECRYPT_FAIL\n");

	} else {
		RX_DEF_LOG("upstream is hdcp1.x device\n");

		temp = (u32)hdmi2com_hdcp1x_status(myhdmi);
		if (temp & HDMI_RX_HDCP1_STATUS_AUTH_DONE)
			RX_DEF_LOG("HDMI_RX_HDCP1_STATUS_AUTH_DONE\n");
		if (temp & HDMI_RX_HDCP1_STATUS_AUTH_FAIL)
			RX_DEF_LOG("HDMI_RX_HDCP1_STATUS_AUTH_FAIL\n");
		if (temp & HDMI_RX_HDCP1_STATUS_DECRYPT_SUCCESS)
			RX_DEF_LOG("HDMI_RX_HDCP1_STATUS_DECRYPT_SUCCESS\n");
		if (temp & HDMI_RX_HDCP1_STATUS_DECRYPT_FAIL)
			RX_DEF_LOG("HDMI_RX_HDCP1_STATUS_DECRYPT_FAIL\n");

		hdmi2com_check_hdcp1x_status(myhdmi);
	}

	return 0;
}

static u32
hdmi2cmd_hdcp_key(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 temp = 0;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("0: key1.x ksv\n");
		RX_DEF_LOG("1: key2.x receiver id\n");
		RX_DEF_LOG("2: load key1.x\n");
		RX_DEF_LOG("3: reset key1.x\n");
		RX_DEF_LOG("4: reset key2.x\n");
		RX_DEF_LOG("8:load key1.x(myhdmi->key1x)\n");
		RX_DEF_LOG("9:sram key1.x\n");
		RX_DEF_LOG("10:myhdmi->key1x key1.x\n");
		return 0;
	}

	ret = kstrtouint(str_p, 10, &temp);

	return 0;
}

static u32
hdmi2cmd_user(struct MTK_HDMI *myhdmi, char *str_p)
{
	struct ipi_cmd_s ipi_cmd;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("ipi: id,data\n");
		return 0;
	}

	ret = sscanf(str_p, "0x%x,0x%x", &ipi_cmd.id, &ipi_cmd.data);
	if (ret != 2)
		return 0;

	if ((ipi_cmd.id > 10000) || (ipi_cmd.data > 10000))
		return 0;

	RX_DEF_LOG("[RX] ipi_cmd=0x%x,0x%x\n",
		ipi_cmd.id, ipi_cmd.data);

	return 0;
}

static u32
hdmi2cmd_notify(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 temp = 0;
	int ret;

	ret = kstrtouint(str_p, 10, &temp);

	if (temp >= 100) {
		RX_DEF_LOG("[RX]unknown notify\n");
		RX_DEF_LOG("[RX]HDMI_RX_PWR_5V_CHANGE = 0\n");
		RX_DEF_LOG("[RX]HDMI_RX_TIMING_LOCK = 1\n");
		RX_DEF_LOG("[RX]HDMI_RX_TIMING_UNLOCK = 2\n");
		RX_DEF_LOG("[RX]HDMI_RX_AUD_LOCK = 3\n");
		RX_DEF_LOG("[RX]HDMI_RX_AUD_UNLOCK = 4\n");
		RX_DEF_LOG("[RX]HDMI_RX_ACP_PKT_CHANGE = 5\n");
		RX_DEF_LOG("[RX]HDMI_RX_AVI_INFO_CHANGE = 6\n");
		RX_DEF_LOG("[RX]HDMI_RX_AUD_INFO_CHANGE = 7\n");
		RX_DEF_LOG("[RX]HDMI_RX_HDR_INFO_CHANGE = 8\n");
		return 0;
	}

	hdmirx_state_callback(myhdmi, (enum HDMIRX_NOTIFY_T)temp);

	return 0;
}

static u32
hdmi2cmd_riu0(struct MTK_HDMI *myhdmi)
{
	riu_read(myhdmi);
	return 0;
}

static u32
hdmi2cmd_riu2(struct MTK_HDMI *myhdmi)
{
	riu_read2(myhdmi);
	return 0;
}
#endif

static u32
hdmi2cmd_slt_crc(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 temp;
	int ret;

	ret = sscanf(str_p, "0x%x", &temp);
	if (ret != 1)
		return 0;

	myhdmi->slt_crc = temp;
	RX_DEF_LOG("[RX]slt_crc=0x%x\n", myhdmi->slt_crc);

	return 0;
}

static u32
hdmi2cmd_slt(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	for (temp = 0; temp < 200; temp++) {
		usleep_range(10000, 10050);
		if ((hdmi2com_hdcp2x_state(myhdmi) == 0x2b) &&
			(hdmi2_get_state(myhdmi) == rx_is_stable) &&
			hdmi2com_check_hdcp2x_decrypt_on(myhdmi)) {
			RX_DEF_LOG("[RX]hdcp2.x pass\n");
			break;
		}
	}

	myhdmi->crc0 = 0;
	myhdmi->crc1 = 0;
	hdmi2com_crc(myhdmi, 10);
	RX_DEF_LOG(
		"[RX]crc0=%x,slt_crc=0x%x\n", myhdmi->crc0, myhdmi->slt_crc);

	if (myhdmi->slt_crc == myhdmi->crc0)
		return 1;
	else
		return 0;
}

static u32
hdmi2cmd_debug_type(struct MTK_HDMI *myhdmi, char *str_p)
{
	u32 u4DbgOn;
	int ret;

	if (strncmp(str_p, "help", 4) == 0) {
		RX_DEF_LOG("HDMI_RX_DEBUG_EDID = (1 << 0)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_HOT_PLUG = (1 << 1)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_HDCP = (1 << 2)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_HDCP_RI = (1 << 3)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_HV_TOTAL = (1 << 4)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_AUDIO = (1 << 5)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_INFOFRAME = (1 << 6)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_DEEPCOLOR = (1 << 7)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_3D = (1 << 8)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_XVYCC = (1 << 9)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_SYNC_DET = (1 <<10)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_MHL = (1 << 11)\n");
		RX_DEF_LOG("HDMI_RX_DEBUG_STB = (1 << 12)\n");

		return 0;
	}

	ret = sscanf(str_p, "0x%x", &u4DbgOn);
	if (ret != 1)
		return 0;

	hdmi2_debug_disable(myhdmi, 0xFFFFFFFF);
	hdmi2_debug_enable(myhdmi, u4DbgOn);

	return 0;
}

static void
process_dbg_cmd(struct MTK_HDMI *myhdmi, char *opt)
{
	int ret = 0;

	if (strncmp(opt, "sltcrc:", 7) == 0)
		hdmi2cmd_slt_crc(myhdmi, opt + 7);
	else if (strncmp(opt, "slt", 3) == 0) {
		if (hdmi2cmd_slt(myhdmi) == 1) {
			ret = sprintf(debug_buffer, "%d", 0);
			if (ret < 0)
				return;
		} else {
			ret = sprintf(debug_buffer, "%d", 1);
			if (ret < 0)
				return;
		}
	} else if (strncmp(opt, "dbgtype:", 8) == 0)
		hdmi2cmd_debug_type(myhdmi, opt + 8);
#ifdef HDMIRX_ENG_BUILD
	else if (strncmp(opt, "wg:", 3) == 0)
		hdmi2cmd_write_reg(myhdmi, opt + 3);
	else if (strncmp(opt, "rg:", 3) == 0)
		hdmi2cmd_read_reg(myhdmi, opt + 3);
	else if (strncmp(opt, "rgr:", 4) == 0)
		hdmi2cmd_read_reg_range(myhdmi, opt + 4);
	else if (strncmp(opt, "wf:", 3) == 0)
		hdmi2cmd_write_flied(myhdmi, opt + 3);
	else if (strncmp(opt, "rf:", 3) == 0)
		hdmi2cmd_read_flied(myhdmi, opt + 3);
	else if (strncmp(opt, "init", 4) == 0)
		hdmi2cmd_init(myhdmi);
	else if (strncmp(opt, "en:", 3) == 0)
		hdmi2cmd_enable(myhdmi, opt + 3);
	else if (strncmp(opt, "task:", 5) == 0)
		hdmi2cmd_task_on(myhdmi, opt + 5);
	else if (strncmp(opt, "status", 6) == 0)
		hdmi2cmd_status(myhdmi);
	else if (strncmp(opt, "hwinit", 6) == 0)
		hdmi2cmd_hw_init(myhdmi);
	else if (strncmp(opt, "cap", 3) == 0)
		hdmi2cmd_capture(myhdmi);
	else if (strncmp(opt, "phyrst:", 7) == 0)
		hdmi2cmd_phy_reset(myhdmi, opt + 7);
	else if (strncmp(opt, "mute:", 5) == 0)
		hdmi2cmd_mute(myhdmi, opt + 5);
	else if (strncmp(opt, "check:", 6) == 0)
		hdmi2cmd_check(myhdmi, opt + 6);
	else if (strncmp(opt, "phy:", 4) == 0)
		hdmi2cmd_phy(myhdmi, opt + 4);
	else if (strncmp(opt, "info", 4) == 0)
		hdmi2cmd_infoframe(myhdmi);
	else if (strncmp(opt, "crc:", 4) == 0)
		hdmi2cmd_crc_check(myhdmi, opt + 4);
	else if (strncmp(opt, "phycrc:", 7) == 0)
		hdmi2cmd_phy_crc_check(myhdmi, opt + 7);
	else if (strncmp(opt, "port:", 5) == 0)
		hdmi2cmd_rx_port(myhdmi, opt + 5);
	else if (strncmp(opt, "scdc:", 5) == 0)
		hdmi2cmd_scdc(myhdmi, opt + 5);
	else if (strncmp(opt, "test:", 5) == 0)
		hdmi2cmd_test_mode(myhdmi, opt + 5);
	else if (strncmp(opt, "intr", 4) == 0)
		hdmi2cmd_irq_register(myhdmi);
	else if (strncmp(opt, "inte:", 5) == 0)
		hdmi2cmd_irq_enable(myhdmi, opt + 5);
	else if (strncmp(opt, "aud:", 4) == 0)
		hdmi2cmd_audio(myhdmi, opt + 4);
	else if (strncmp(opt, "audcheck", 8) == 0)
		hdmi2cmd_audio_check(myhdmi);
	else if (strncmp(opt, "aout:", 5) == 0)
		hdmi2cmd_audio_out_config(myhdmi, opt + 5);
	else if (strncmp(opt, "audfmt:", 7) == 0)
		hdmi2cmd_audio_format_config(myhdmi, opt + 7);
	else if (strncmp(opt, "audmute:", 8) == 0)
		hdmi2cmd_audio_mute(myhdmi, opt + 8);
	else if (strncmp(opt, "edid:", 5) == 0)
		hdmi2cmd_edid(myhdmi, opt + 5);
	else if (strncmp(opt, "hdcpinit", 8) == 0)
		hdmi2cmd_hdcp_init(myhdmi);
	else if (strncmp(opt, "hdcp:", 5) == 0)
		hdmi2cmd_hdcp(myhdmi, opt + 5);
	else if (strncmp(opt, "hdcpcheck", 9) == 0)
		hdmi2cmd_hdcp_check(myhdmi);
	else if (strncmp(opt, "hdcpkey:", 8) == 0)
		hdmi2cmd_hdcp_key(myhdmi, opt + 8);
	else if (strncmp(opt, "out", 3) == 0)
		hdmi2cmd_video_out_config(myhdmi);
	else if (strncmp(opt, "user:", 5) == 0)
		hdmi2cmd_user(myhdmi, opt + 5);
	else if (strncmp(opt, "notify:", 7) == 0)
		hdmi2cmd_notify(myhdmi, opt + 7);
	else if (strncmp(opt, "riu0", 4) == 0)
		hdmi2cmd_riu0(myhdmi);
	else if (strncmp(opt, "riu2", 4) == 0)
		hdmi2cmd_riu2(myhdmi);
#endif
}

struct dentry *hdmirx_debugfs;

static int
debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t
debug_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;

	n = strlen(debug_buffer);
	//debug_buffer[n++] = 0;

	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static ssize_t
debug_write(
	struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	ssize_t ret;
	struct MTK_HDMI *myhdmi;

	myhdmi = file->private_data;
	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(myhdmi, debug_buffer);

	return ret;
}

static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

int
hdmi2cmd_debug_init(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]%s\n", __func__);
	hdmirx_debugfs = NULL;
	hdmirx_debugfs =
		debugfs_create_file("hdmirx", 0600, /* S_IFREG | S_IRUGO */
			NULL, (void *)myhdmi, &debug_fops);

	if (IS_ERR(hdmirx_debugfs))
		return PTR_ERR(hdmirx_debugfs);

	return 0;
}

void
hdmi2cmd_debug_uninit(struct MTK_HDMI *myhdmi)
{
	debugfs_remove(hdmirx_debugfs);
}
