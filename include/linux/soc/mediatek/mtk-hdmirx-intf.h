/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Macross Chen <macross.chen@mediatek.com>
 */

#ifndef __MTK_HDMIRX_INTF_H
#define __MTK_HDMIRX_INTF_H

#include <linux/device.h>

/* color space */
enum hdmirx_intf_cs {
	HDMIRX_INTF_CS_RGB = 0,	/* RGB */
	HDMIRX_INTF_CS_YUV444,		/* YUV444 */
	HDMIRX_INTF_CS_YUV422,		/* YUV422 */
	HDMIRX_INTF_CS_YUV420		/* YUV420 */
};

struct hdmirx_capture_interface {
	unsigned int width;
	unsigned int height;
	unsigned int frame_rate;	/* Hz */
	enum hdmirx_intf_cs color_space;

	void *priv;
};

struct hdmirx_capture_ops {
	int (*probe)(struct hdmirx_capture_interface *intf);
	void (*disconnect)(struct hdmirx_capture_interface *intf);
};

struct hdmirx_capture_driver {
	const char *name;
	const struct hdmirx_capture_ops *ops;

	struct device *hdmirx_dev;
	void *priv;
};

#if IS_ENABLED(CONFIG_MTK_HDMI_RX)

extern int hdmirx_register_capture_driver(struct hdmirx_capture_driver *capture_driver);
extern void hdmirx_unregister_capture_driver(struct hdmirx_capture_driver *capture_driver);

#else

static inline int hdmirx_register_capture_driver(struct hdmirx_capture_driver *capture_driver)
{
	return 0;
}
static inline void hdmirx_unregister_capture_driver(struct hdmirx_capture_driver *capture_driver) {}

#endif /* IS_ENABLED(CONFIG_MTK_HDMI_RX) */

#endif	/* __MTK_HDMIRX_INTF_H */
