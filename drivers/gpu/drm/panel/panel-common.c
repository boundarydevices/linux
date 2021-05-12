/*
 * Copyright (C) 2021, Boundary Devices.  All rights reserved.
 * Copyright (C) 2013, NVIDIA Corporation.  All rights reserved.
 * Based on panel-simple
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/backlight.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

#include <uapi/linux/media-bus-format.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <dt-bindings/display/simple_panel_mipi_cmds.h>

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include "panel-sn65dsi83.h"

/**
 * @modes: Pointer to array of fixed modes appropriate for this panel.  If
 *         only one mode then this can just be the address of this the mode.
 *         NOTE: cannot be used with "timings" and also if this is specified
 *         then you cannot override the mode in the device tree.
 * @num_modes: Number of elements in modes array.
 * @timings: Pointer to array of display timings.  NOTE: cannot be used with
 *           "modes" and also these will be used to validate a device tree
 *           override if one is present.
 * @num_timings: Number of elements in timings array.
 * @bpc: Bits per color.
 * @size: Structure containing the physical size of this panel.
 * @delay: Structure containing various delay values for this panel.
 * @bus_format: See MEDIA_BUS_FMT_... defines.
 * @bus_flags: See DRM_BUS_FLAG_... defines.
 */
struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	const struct display_timing *timings;
	unsigned int num_timings;

	unsigned int bpc;

	/**
	 * @width: width (in millimeters) of the panel's active display area
	 * @height: height (in millimeters) of the panel's active display area
	 */
	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @hpd_absent_delay: Add this to the prepare delay if we know Hot
	 *                    Plug Detect isn't used.
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int hpd_absent_delay;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
		unsigned int before_backlight_on;
	} delay;

	u32 bus_format;
	u32 bus_flags;
	int connector_type;
};

struct cmds {
	const u8* cmds;
	unsigned length;
};

struct interface_cmds {
	struct cmds i2c;
	struct cmds mipi;
	struct cmds spi;
};

struct panel_common {
	struct drm_panel base;
	bool prepared;
	bool enabled;
	bool enabled2;
	bool no_hpd;

	const struct panel_desc *desc;
	struct panel_desc dt_desc;
	struct drm_display_mode dt_mode;

	unsigned enable_high_duration_us;
	unsigned enable_low_duration_us;
	unsigned power_delay_ms;
	unsigned mipi_delay_between_cmds;
	struct regulator *supply;
	struct i2c_adapter *ddc;
	struct clk *mipi_clk;

	struct gpio_desc *gpd_prepare_enable;
	struct gpio_desc *hpd_gpio;

	struct drm_display_mode override_mode;

	enum drm_panel_orientation orientation;

	struct gpio_desc *gpd_power;
	struct gpio_desc *gpd_display_enable;
	struct gpio_desc *reset;
	struct videomode vm;
	struct spi_device *spi;
	unsigned spi_max_frequency;
	struct i2c_adapter *i2c;
	unsigned i2c_max_frequency;
	unsigned i2c_address;
	unsigned char spi_9bit;
	unsigned spi_bits;
	struct interface_cmds cmds_init;
	struct interface_cmds cmds_enable;
	struct interface_cmds cmds_enable2;
	struct interface_cmds cmds_disable;
	struct panel_sn65dsi83 sn65;
	/* Keep a mulitple of 9 */
	unsigned char tx_buf[63] __attribute__((aligned(64)));
	unsigned char rx_buf[63] __attribute__((aligned(64)));
};

static inline struct panel_common *to_panel_common(struct drm_panel *panel)
{
	return container_of(panel, struct panel_common, base);
}

static int spi_send(struct panel_common *panel, int rx)
{
	struct mipi_dsi_device *dsi;
	u8 *p = panel->tx_buf;
	struct spi_transfer t;
	struct spi_message m;
	int ret;

	if (!panel->spi || !panel->spi_bits)
		return 0;
	memset(&t, 0, sizeof(t));
	t.speed_hz = panel->spi_max_frequency;
	t.tx_buf = panel->tx_buf;
	t.rx_buf = (rx) ? panel->rx_buf : NULL;
	t.len = (panel->spi_bits + 7) >> 3;

	panel->spi_bits = 0;
	spi_message_init_with_transfers(&m, &t, 1);
	ret = spi_sync(panel->spi, &m);
	dsi = container_of(panel->base.dev, struct mipi_dsi_device, dev);
	if (ret) {
		dev_err(&dsi->dev,
			"spi(%d), (%d)%02x %02x %02x %02x %02x %02x\n",
			ret, t.len, p[0], p[1], p[2], p[3], p[4], p[5]);
	} else {
		pr_debug("spi(%d), (%d)%02x %02x %02x %02x %02x %02x  "
			"%02x %02x %02x %02x %02x %02x\n",
			ret, t.len, p[0], p[1], p[2], p[3], p[4], p[5],
			p[6], p[7], p[8], p[9], p[10], p[11]);
	}
	return ret;
}

static int store_9bit(struct panel_common *panel, const u8 *p, unsigned l)
{
	int i = 0;
	u8 * buf = panel->tx_buf;
	unsigned val = 0;
	int bits = l * 9;
	int v_bits;
	int ret = 0;

	i = panel->spi_bits;
	if ((i + bits) > sizeof(panel->tx_buf) * 8) {
		ret = spi_send(panel, 0);
		i = 0;
		if (bits > sizeof(panel->tx_buf) * 8) {
			struct mipi_dsi_device *dsi;

			dsi = container_of(panel->base.dev, struct mipi_dsi_device, dev);
			dev_err(&dsi->dev, "too many bytes (%d)\n", l);
			return -EINVAL;
		}
	}

	panel->spi_bits = i + bits;
	buf += i >> 3;
	v_bits = (i & 7);
	if (v_bits) {
		bits += v_bits;
		val = *buf;
		val >>= (8 - v_bits);
	}
	val = (val << 9);
	v_bits += 9;
	if (l) {
		val |= *p++;
		l--;
	}
	while (bits > 0) {
		*buf++ = val >> (v_bits - 8);
		bits -= 8;
		v_bits -= 8;
		if (v_bits < 8) {
			val = (val << 9);
			v_bits += 9;
			if (l) {
				val |= 0x100;
				val |= *p++;
				l--;
			}
		}
	}
	return ret;
}

static int store_high(struct panel_common *panel, int bits)
{
	int i;
	u8 * buf = panel->tx_buf;
	unsigned val = 0;
	int v_bits;

	i = panel->spi_bits;
	if ((i + bits) > sizeof(panel->tx_buf) * 8) {
		struct mipi_dsi_device *dsi;

		dsi = container_of(panel->base.dev, struct mipi_dsi_device, dev);
		dev_err(&dsi->dev, "too many bits (%d)\n", bits);
		return -EINVAL;
	}

	panel->spi_bits = i + bits;

	buf += i >> 3;
	v_bits = (i & 7);
	if (v_bits) {
		bits += v_bits;
		val = *buf;
		val >>= (8 - v_bits);
	}

	while (bits > 0) {
		if (v_bits < 8) {
			val = (val << 24) | 0xffffff;
			v_bits += 24;
		}
		*buf++ = val >> (v_bits - 8);
		bits -= 8;
		v_bits -= 8;
	}
	return 0;
}

static void extract_data(u8 *dst, unsigned bytes, u8 * buf, unsigned start_bit)
{
	unsigned val = 0;
	int v_bits;

	buf += start_bit >> 3;
	v_bits = (start_bit & 7);
	if (v_bits) {
		val = *buf++;
		v_bits = 8 - v_bits;
	}
	while (bytes > 0) {
		if (v_bits < 8) {
			val = (val << 8) | *buf++;
			v_bits += 8;
		}
		*dst++ = val >> (v_bits - 8);
		v_bits -= 8;
		bytes--;
	}
}
int common_i2c_write(struct panel_common *panel, const u8 *tx, unsigned tx_len)
{
	u8 *buf = panel->tx_buf;
	u8 tmp;
	struct i2c_msg msg;
	int ret;

	if (tx_len > sizeof(panel->tx_buf))
		return -EINVAL;
	memcpy(buf, tx, tx_len);
	if (tx_len >= 2) {
		tmp = buf[0];
		buf[0] = buf[1];
		buf[1] = tmp;
	}

	msg.addr = panel->i2c_address;
	msg.flags = 0;
	msg.len = tx_len;
	msg.buf = buf;
	ret = i2c_transfer(panel->i2c, &msg, 1);
	if (ret < 0) {
		msleep(10);
		ret = i2c_transfer(panel->i2c, &msg, 1);
	}
	return ret < 0 ? ret : 0;
}

int common_i2c_read(struct panel_common *panel, const u8 *tx, int tx_len, u8 *rx, unsigned rx_len)
{
	u8 *buf = panel->tx_buf;
	u8 tmp;
	struct i2c_msg msgs[2];
	int ret;

	if (tx_len > sizeof(panel->tx_buf))
		return -EINVAL;
	if (rx_len > sizeof(panel->rx_buf))
		return -EINVAL;
	memcpy(buf, tx, tx_len);
	if (tx_len >= 2) {
		tmp = buf[0];
		buf[0] = buf[1];
		buf[1] = tmp;
	}
	msgs[0].addr = panel->i2c_address;
	msgs[0].flags = 0;
	msgs[0].len = tx_len;
	msgs[0].buf = buf;

	msgs[1].addr = panel->i2c_address;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = rx_len;
	msgs[1].buf = rx;
	ret = i2c_transfer(panel->i2c, msgs, 2);
	return ret < 0 ? ret : 0;
}

#define TYPE_MIPI	0
#define TYPE_I2C	1
#define TYPE_SPI	2

#define lptxtime_ns 75
#define prepare_ns 100
#define zero_ns 250

static int send_cmd_list(struct panel_common *panel, struct cmds *mc, int type, const unsigned char *id)
{
	struct drm_display_mode *dm = &panel->dt_mode;
	struct mipi_dsi_device *dsi;
	const u8 *cmd = mc->cmds;
	unsigned length = mc->length;
	u8 data[8];
	u8 cmd_buf[32];
	const u8 *p;
	unsigned l;
	unsigned len;
	unsigned mask;
	u32 mipi_clk_rate = 0;
	unsigned long tmp;
	int ret;
	int generic;
	int match = 0;
	u64 readval, matchval;
	int skip = 0;
	int match_index;
	int i;

	pr_debug("%s:%d %d\n", __func__, length, type);
	if (!cmd || !length)
		return 0;

	panel->spi_bits = 0;
	dsi = container_of(panel->base.dev, struct mipi_dsi_device, dev);
	while (1) {
		len = *cmd++;
		length--;
		if (!length)
			break;
		if ((len >= S_IF_1_LANE) && (len <= S_IF_4_LANES)) {
			int lane_match = 1 + len - S_IF_1_LANE;

			if (lane_match != dsi->lanes)
				skip = 1;
			continue;
		} else if (len == S_IF_BURST) {
			if (!(dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST))
				skip = 1;
			continue;
		} else if (len == S_IF_NONBURST) {
			if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
				skip = 1;
			continue;
		}
		generic = len & 0x80;
		len &= 0x7f;

		p = cmd;
		l = len;
		ret = 0;
		if ((len < S_DELAY) || (len == S_DCS_LENGTH)
				|| (len == S_DCS_BUF)) {
			if (len == S_DCS_LENGTH) {
				len = *cmd++;
				length--;
				p = cmd;
				l = len;
			} else if (len == S_DCS_BUF) {
				l = *cmd++;
				length--;
				if (l > 32)
					l = 32;
				p = cmd_buf;
				len = 0;
			} else {
				p = cmd;
				l = len;
			}
			if (length < len) {
				dev_err(&dsi->dev, "Unexpected end of data\n");
				break;
			}
			if (!skip) {
				if (type == TYPE_I2C) {
					ret = common_i2c_write(panel, p, l);
				} else if (type == TYPE_SPI) {
					if (panel->spi_9bit) {
						ret = store_9bit(panel, p, l);
					} else {
						if (l < sizeof(panel->tx_buf)) {
							memcpy(panel->tx_buf, p, l);
							panel->spi_bits = l * 8;
							ret = spi_send(panel, 0);
						} else {
							ret = -EINVAL;
						}
					}
				} else {
					if (generic) {
						ret = mipi_dsi_generic_write(dsi, p, l);
					} else {
						ret = mipi_dsi_dcs_write_buffer(dsi, p, l);
					}
					if (panel->mipi_delay_between_cmds)
						msleep(panel->mipi_delay_between_cmds);
				}
			}
		} else if (len == S_MRPS) {
			if (type == TYPE_MIPI) {
				ret = mipi_dsi_set_maximum_return_packet_size(
					dsi, cmd[0]);
			}
			l = len = 1;
		} else if ((len >= S_DCS_READ1) && (len <= S_DCS_READ8)) {
			data[0] = data[1] = data[2] = data[3] = 0;
			data[4] = data[5] = data[6] = data[7] = 0;
			len = len - S_DCS_READ1 + 1;
			match_index = generic ? 2 : 1;
			if (!skip) {
				if (type == TYPE_I2C) {
					ret = common_i2c_read(panel, cmd, match_index, data, len);
				} else if (type == TYPE_SPI) {
					if (panel->spi_9bit) {
						spi_send(panel, 0);
						store_9bit(panel, cmd, match_index);
					} else {
						memcpy(panel->tx_buf, cmd, match_index);
						panel->spi_bits = match_index * 8;
					}
					l = panel->spi_bits;
					store_high(panel, len * 8);
					ret = spi_send(panel, 1);
					extract_data(data, len, panel->rx_buf, l);
				} else if (generic) {
					ret =  mipi_dsi_generic_read(dsi, cmd, 2, data, len);
					/* if an error is pending before BTA, an error report can happen */
					if (ret == -EPROTO)
						ret =  mipi_dsi_generic_read(dsi, cmd, 2, data, len);
					ret = 0;
				} else {
					ret =  mipi_dsi_dcs_read(dsi, cmd[0], data, len);
					if (ret == -EPROTO)
						ret =  mipi_dsi_dcs_read(dsi, cmd[0], data, len);
				}
				readval = 0;
				matchval = 0;
				for (i = 0; i < len; i++) {
					readval |= ((u64)data[i]) << (i << 3);
					matchval |= ((u64)cmd[match_index + i]) << (i << 3);
				}
				if (generic) {
					pr_debug("Read (%d)%s GEN: (%04x) 0x%llx cmp 0x%llx\n",
						ret, id, cmd[0] + (cmd[1] << 8), readval, matchval);
				} else {
					pr_debug("Read (%d)%s DCS: (%02x) 0x%llx cmp 0x%llx\n",
						ret, id, cmd[0], readval, matchval);
				}
				if (readval != matchval)
					match = -EINVAL;
			}
			l = len = len + match_index;
		} else if (len == S_DELAY) {
			if (!skip) {
				if (type == TYPE_SPI)
					spi_send(panel, 0);
				msleep(cmd[0]);
			}
			len = 1;
			if (length <= len)
				break;
			cmd += len;
			length -= len;
			l = len = 0;
		} else if (len >= S_CONST) {
			int scmd, dest_start, dest_len, src_start;
			unsigned val;

			scmd = len;
			dest_start = cmd[0];
			dest_len = cmd[1];
			if (scmd == S_CONST) {
				val = cmd[2] | (cmd[3] << 8) | (cmd[4] << 16) | (cmd[5] << 24);
				len = 6;
				src_start = 0;
			} else {
				src_start = cmd[2];
				len = 3;
				switch (scmd) {
				case S_HSYNC:
					val = dm->hsync_end - dm->hsync_start;
					break;
				case S_HBP:
					val = dm->htotal - dm->hsync_end;
					break;
				case S_HACTIVE:
					val = dm->hdisplay;
					break;
				case S_HFP:
					val = dm->hsync_start - dm->hdisplay;
					break;
				case S_VSYNC:
					val = dm->vsync_end - dm->vsync_start;
					break;
				case S_VBP:
					val = dm->vtotal - dm->vsync_end;
					break;
				case S_VACTIVE:
					val = dm->vdisplay;
					break;
				case S_VFP:
					val = dm->vsync_start - dm->vdisplay;
					break;
				case S_LPTXTIME:
					val = 3;
					if (!mipi_clk_rate && panel->mipi_clk)
						mipi_clk_rate = clk_get_rate(panel->mipi_clk);
					if (!mipi_clk_rate) {
						dev_warn(&dsi->dev, "Unknown mipi_clk_rate\n");
						break;
					}
					/* val = ROUND(lptxtime_ns * mipi_clk_rate/4  /1000000000) */
					tmp = lptxtime_ns;
					tmp *= mipi_clk_rate;
					tmp += 2000000000;
					tmp /= 4000000000;
					val = (unsigned)tmp;
					pr_debug("%s:lptxtime=%d\n", __func__, val);
					if (val > 2047)
						val = 2047;
					break;
				case S_CLRSIPOCOUNT:
					val = 5;
					if (!mipi_clk_rate && panel->mipi_clk)
						mipi_clk_rate = clk_get_rate(panel->mipi_clk);
					if (!mipi_clk_rate) {
						dev_warn(&dsi->dev, "Unknown mipi_clk_rate\n");
						break;
					}
					/* clrsipocount = ROUNDUP((prepare_ns + zero_ns/2) * mipi_clk_rate/4 /1000000000) - 5 */
					tmp = prepare_ns + (zero_ns >> 1) ;
					tmp *= mipi_clk_rate;
					tmp += 4000000000 - 1;
					tmp /= 4000000000;
					val = (unsigned)tmp - 5;
					pr_debug("%s:clrsipocount=%d\n", __func__, val);
					if (val > 63)
						val = 63;
					break;
				default:
					dev_err(&dsi->dev, "Unknown scmd 0x%x0x\n", scmd);
					val = 0;
					break;
				}
			}
			val >>= src_start;
			while (dest_len && dest_start < 256) {
				l = dest_start & 7;
				mask = (dest_len < 8) ? ((1 << dest_len) - 1) : 0xff;
				cmd_buf[dest_start >> 3] &= ~(mask << l);
				cmd_buf[dest_start >> 3] |= (val << l);
				l = 8 - l;
				dest_start += l;
				val >>= l;
				if (dest_len >= l)
					dest_len -= l;
				else
					dest_len = 0;
			}
			l = 0;
		}
		if (ret < 0) {
			if (l >= 6) {
				dev_err(&dsi->dev,
					"Failed to send %s (%d), (%d)%02x %02x: %02x %02x %02x %02x\n",
					id, ret, l, p[0], p[1], p[2], p[3], p[4], p[5]);
			} else if (l >= 2) {
				dev_err(&dsi->dev,
					"Failed to send %s (%d), (%d)%02x %02x\n",
					id, ret, l, p[0], p[1]);
			} else {
				dev_err(&dsi->dev,
					"Failed to send %s (%d), (%d)%02x\n",
					id, ret, l, p[0]);
			}
			return ret;
		} else {
			if (!skip) {
				if (l >= 18) {
					pr_debug("Sent %s (%d), (%d)%02x %02x: %02x %02x %02x %02x"
							"  %02x %02x %02x %02x"
							"  %02x %02x %02x %02x"
							"  %02x %02x %02x %02x\n",
						id, ret, l, p[0], p[1], p[2], p[3], p[4], p[5],
						p[6], p[7], p[8], p[9],
						p[10], p[11], p[12], p[13],
						p[14], p[15], p[16], p[17]);
				} else if (l >= 6) {
					pr_debug("Sent %s (%d), (%d)%02x %02x: %02x %02x %02x %02x\n",
						id, ret, l, p[0], p[1], p[2], p[3], p[4], p[5]);
				} else if (l >= 2) {
					pr_debug("Sent %s (%d), (%d)%02x %02x\n",
						id, ret, l, p[0], p[1]);
				} else if (l) {
					pr_debug("Sent %s (%d), (%d)%02x\n",
						id, ret, l, p[0]);
				}
			}
		}
		if (length < len) {
			dev_err(&dsi->dev, "Unexpected end of data\n");
			break;
		}
		cmd += len;
		length -= len;
		if (!length)
			break;
		skip = 0;
	}
	if (!match && type == TYPE_SPI)
		match = spi_send(panel, 0);
	return match;
};

static int send_all_cmd_lists(struct panel_common *panel, struct interface_cmds *msc)
{
	int ret = 0;

	if (panel->i2c)
		ret = send_cmd_list(panel,  &msc->i2c, TYPE_I2C, "i2c");
	if (!ret)
		ret = send_cmd_list(panel,  &msc->mipi, TYPE_MIPI, "mipi");
	if (!ret && panel->spi)
		ret = send_cmd_list(panel,  &msc->spi, TYPE_SPI, "spi");
	return ret;
}

static unsigned int panel_common_get_timings_modes(struct panel_common *panel,
						   struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	unsigned int i, num = 0;

	for (i = 0; i < panel->desc->num_timings; i++) {
		const struct display_timing *dt = &panel->desc->timings[i];
		struct videomode vm;

		videomode_from_timing(dt, &vm);
		mode = drm_mode_create(connector->dev);
		if (!mode) {
			dev_err(panel->base.dev, "failed to add mode %ux%u\n",
				dt->hactive.typ, dt->vactive.typ);
			continue;
		}

		drm_display_mode_from_videomode(&vm, mode);

		mode->type |= DRM_MODE_TYPE_DRIVER;

		if (panel->desc->num_timings == 1)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_probed_add(connector, mode);
		num++;
	}

	return num;
}

static unsigned int panel_common_get_display_modes(struct panel_common *panel,
						   struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	unsigned int i, num = 0;

	for (i = 0; i < panel->desc->num_modes; i++) {
		const struct drm_display_mode *m = &panel->desc->modes[i];

		mode = drm_mode_duplicate(connector->dev, m);
		if (!mode) {
			dev_err(panel->base.dev, "failed to add mode %ux%u@%u\n",
				m->hdisplay, m->vdisplay,
				drm_mode_vrefresh(m));
			continue;
		}

		mode->type |= DRM_MODE_TYPE_DRIVER;

		if (panel->desc->num_modes == 1)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_set_name(mode);

		drm_mode_probed_add(connector, mode);
		num++;
	}

	return num;
}

static int panel_common_get_non_edid_modes(struct panel_common *panel,
					   struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	bool has_override = panel->override_mode.type;
	unsigned int num = 0;

	if (!panel->desc)
		return 0;

	if (has_override) {
		mode = drm_mode_duplicate(connector->dev,
					  &panel->override_mode);
		if (mode) {
			drm_mode_probed_add(connector, mode);
			num = 1;
		} else {
			dev_err(panel->base.dev, "failed to add override mode\n");
		}
	}

	/* Only add timings if override was not there or failed to validate */
	if (num == 0 && panel->desc->num_timings)
		num = panel_common_get_timings_modes(panel, connector);

	/*
	 * Only add fixed modes if timings/override added no mode.
	 *
	 * We should only ever have either the display timings specified
	 * or a fixed mode. Anything else is rather bogus.
	 */
	WARN_ON(panel->desc->num_timings && panel->desc->num_modes);
	if (num == 0)
		num = panel_common_get_display_modes(panel, connector);

	connector->display_info.bpc = panel->desc->bpc;
	connector->display_info.width_mm = panel->desc->size.width;
	connector->display_info.height_mm = panel->desc->size.height;
	if (panel->desc->bus_format)
		drm_display_info_set_bus_formats(&connector->display_info,
						 &panel->desc->bus_format, 1);
	connector->display_info.bus_flags = panel->desc->bus_flags;

	return num;
}

static int panel_common_disable(struct drm_panel *panel)
{
	struct mipi_dsi_device *dsi;
	struct panel_common *p = to_panel_common(panel);

	if (!p->enabled)
		return 0;

	gpiod_set_value_cansleep(p->gpd_display_enable, 0);

	if (p->desc->delay.disable)
		msleep(p->desc->delay.disable);
	dsi = container_of(p->base.dev, struct mipi_dsi_device, dev);
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	send_all_cmd_lists(p, &p->cmds_disable);

	sn65_disable(&p->sn65);
	p->enabled = false;
	p->enabled2 = false;

	return 0;
}

static int panel_common_unprepare(struct drm_panel *panel)
{
	struct panel_common *p = to_panel_common(panel);

	if (!p->prepared)
		return 0;

	if (p->desc->delay.unprepare)
		msleep(p->desc->delay.unprepare);
	gpiod_set_value_cansleep(p->reset, 1);
	gpiod_set_value_cansleep(p->gpd_prepare_enable, 0);

	regulator_disable(p->supply);


	p->prepared = false;

	return 0;
}

static int panel_common_get_hpd_gpio(struct device *dev,
				     struct panel_common *p, bool from_probe)
{
	int err;

	p->hpd_gpio = devm_gpiod_get_optional(dev, "hpd", GPIOD_IN);
	if (IS_ERR(p->hpd_gpio)) {
		err = PTR_ERR(p->hpd_gpio);

		/*
		 * If we're called from probe we won't consider '-EPROBE_DEFER'
		 * to be an error--we'll leave the error code in "hpd_gpio".
		 * When we try to use it we'll try again.  This allows for
		 * circular dependencies where the component providing the
		 * hpd gpio needs the panel to init before probing.
		 */
		if (err != -EPROBE_DEFER || !from_probe) {
			dev_err(dev, "failed to get 'hpd' GPIO: %d\n", err);
			return err;
		}
	}

	return 0;
}

static int panel_common_prepare(struct drm_panel *panel)
{
	struct panel_common *p = to_panel_common(panel);
	struct mipi_dsi_device *dsi;
	unsigned int delay;
	int err;
	int hpd_asserted;

	if (p->prepared)
		return 0;

	err = regulator_enable(p->supply);
	if (err < 0) {
		dev_err(panel->dev, "failed to enable supply: %d\n", err);
		return err;
	}

	if (p->gpd_power) {
		gpiod_set_value_cansleep(p->gpd_power, 1);
		mdelay(p->power_delay_ms);
	}
	if (p->gpd_prepare_enable) {
		if (p->enable_high_duration_us) {
			gpiod_set_value_cansleep(p->gpd_prepare_enable, 1);
			udelay(p->enable_high_duration_us);
		}
		if (p->enable_low_duration_us) {
			gpiod_set_value_cansleep(p->gpd_prepare_enable, 0);
			udelay(p->enable_low_duration_us);
		}
		gpiod_set_value_cansleep(p->gpd_prepare_enable, 1);
	}
	gpiod_set_value_cansleep(p->reset, 0);


	delay = p->desc->delay.prepare;
	if (p->no_hpd)
		delay += p->desc->delay.hpd_absent_delay;
	if (delay)
		msleep(delay);

	if (p->hpd_gpio) {
		if (IS_ERR(p->hpd_gpio)) {
			err = panel_common_get_hpd_gpio(panel->dev, p, false);
			if (err)
				return err;
		}

		err = readx_poll_timeout(gpiod_get_value_cansleep, p->hpd_gpio,
					 hpd_asserted, hpd_asserted,
					 1000, 2000000);
		if (hpd_asserted < 0)
			err = hpd_asserted;

		if (err) {
			dev_err(panel->dev,
				"error waiting for hpd GPIO: %d\n", err);
			return err;
		}
	}

	dsi = container_of(p->base.dev, struct mipi_dsi_device, dev);
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	err = send_all_cmd_lists(p, &p->cmds_init);
	if (err) {
		regulator_disable(p->supply);
		return err;
	}
	p->prepared = true;

	return 0;
}

static int panel_common_enable(struct drm_panel *panel)
{
	struct panel_common *p = to_panel_common(panel);
	struct mipi_dsi_device *dsi;
	int ret;

	if (p->enabled)
		return 0;

	dsi = container_of(p->base.dev, struct mipi_dsi_device, dev);
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	ret = send_all_cmd_lists(p, &p->cmds_enable);
	if (ret < 0)
		goto fail;
	if (p->desc->delay.enable)
		msleep(p->desc->delay.enable);
	sn65_enable(&p->sn65);
	p->enabled = true;

	return 0;
fail:
	if (p->reset)
		gpiod_set_value_cansleep(p->reset, 1);
	if (p->gpd_prepare_enable)
		gpiod_set_value_cansleep(p->gpd_prepare_enable, 0);
	return ret;
}

static int panel_common_enable2(struct drm_panel *panel)
{
	struct panel_common *p = to_panel_common(panel);
	struct mipi_dsi_device *dsi;
	int ret;

	if (p->enabled2)
		return 0;

	dsi = container_of(p->base.dev, struct mipi_dsi_device, dev);
	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
	ret = send_all_cmd_lists(p, &p->cmds_enable2);
	if (ret < 0)
		goto fail;

	sn65_enable2(&p->sn65);
	gpiod_set_value_cansleep(p->gpd_display_enable, 1);
	if (p->desc->delay.before_backlight_on)
		msleep(p->desc->delay.before_backlight_on);

	p->enabled2 = true;

	return 0;
fail:
	p->enabled = false;
	if (p->reset)
		gpiod_set_value_cansleep(p->reset, 1);
	if (p->gpd_prepare_enable)
		gpiod_set_value_cansleep(p->gpd_prepare_enable, 0);
	return ret;
}

static int panel_common_get_modes(struct drm_panel *panel,
				  struct drm_connector *connector)
{
	struct panel_common *p = to_panel_common(panel);
	int num = 0;

	/* probe EDID if a DDC bus is available */
	if (p->ddc) {
		struct edid *edid = drm_get_edid(connector, p->ddc);

		drm_connector_update_edid_property(connector, edid);
		if (edid) {
			num += drm_add_edid_modes(connector, edid);
			kfree(edid);
		}
	}

	/* add hard-coded panel modes */
	num += panel_common_get_non_edid_modes(p, connector);

	/* set up connector's "panel orientation" property */
	drm_connector_set_panel_orientation(connector, p->orientation);

	return num;
}

static int panel_common_get_timings(struct drm_panel *panel,
				    unsigned int num_timings,
				    struct display_timing *timings)
{
	struct panel_common *p = to_panel_common(panel);
	unsigned int i;

	if (p->desc->num_timings < num_timings)
		num_timings = p->desc->num_timings;

	if (timings)
		for (i = 0; i < num_timings; i++)
			timings[i] = p->desc->timings[i];

	return p->desc->num_timings;
}

static const struct drm_panel_funcs panel_common_funcs = {
	.disable = panel_common_disable,
	.unprepare = panel_common_unprepare,
	.prepare = panel_common_prepare,
	.enable = panel_common_enable,
	.enable2 = panel_common_enable2,
	.get_modes = panel_common_get_modes,
	.get_timings = panel_common_get_timings,
};

static void check_for_cmds(struct device_node *np, const char *dt_name, struct cmds *mc)
{
	void *data;
	int data_len;
	int ret;

	/* Check for mipi command arrays */
	if (!of_get_property(np, dt_name, &data_len) || !data_len)
		return;

	data = kmalloc(data_len, GFP_KERNEL);
	if (!data)
		return;

	ret = of_property_read_u8_array(np, dt_name, data, data_len);
	if (ret) {
		pr_info("failed to read %s from DT: %d\n",
			dt_name, ret);
		kfree(data);
		return;
	}
	mc->cmds = data;
	mc->length = data_len;
}

#define panel_common_BOUNDS_CHECK(to_check, bounds, field) \
	(to_check->field.typ >= bounds->field.min && \
	 to_check->field.typ <= bounds->field.max)
static void panel_common_parse_panel_timing_node(struct device *dev,
						 struct panel_common *panel,
						 const struct display_timing *ot)
{
	const struct panel_desc *desc = panel->desc;
	struct videomode vm;
	unsigned int i;

	if (WARN_ON(desc->num_modes)) {
		dev_err(dev, "Reject override mode: panel has a fixed mode\n");
		return;
	}
	if (WARN_ON(!desc->num_timings)) {
		dev_err(dev, "Reject override mode: no timings specified\n");
		return;
	}

	for (i = 0; i < panel->desc->num_timings; i++) {
		const struct display_timing *dt = &panel->desc->timings[i];

		if (!panel_common_BOUNDS_CHECK(ot, dt, hactive) ||
		    !panel_common_BOUNDS_CHECK(ot, dt, hfront_porch) ||
		    !panel_common_BOUNDS_CHECK(ot, dt, hback_porch) ||
		    !panel_common_BOUNDS_CHECK(ot, dt, hsync_len) ||
		    !panel_common_BOUNDS_CHECK(ot, dt, vactive) ||
		    !panel_common_BOUNDS_CHECK(ot, dt, vfront_porch) ||
		    !panel_common_BOUNDS_CHECK(ot, dt, vback_porch) ||
		    !panel_common_BOUNDS_CHECK(ot, dt, vsync_len))
			continue;

		if (ot->flags != dt->flags)
			continue;

		videomode_from_timing(ot, &vm);
		drm_display_mode_from_videomode(&vm, &panel->override_mode);
		panel->override_mode.type |= DRM_MODE_TYPE_DRIVER |
					     DRM_MODE_TYPE_PREFERRED;
		break;
	}

	if (WARN_ON(!panel->override_mode.type))
		dev_err(dev, "Reject override mode: No display_timing found\n");
}

static void init_common(struct panel_common *p, struct device_node *np, struct panel_desc *ds,
		struct drm_display_mode *dm, struct mipi_dsi_device *dsi)
{
	of_property_read_u32(np, "delay-prepare", &ds->delay.prepare);
	of_property_read_u32(np, "delay-enable", &ds->delay.enable);
	of_property_read_u32(np, "delay-disable", &ds->delay.disable);
	of_property_read_u32(np, "delay-unprepare", &ds->delay.unprepare);
	of_property_read_u32(np, "delay-before-backlight-on", &ds->delay.before_backlight_on);
	of_property_read_u32(np, "min-hs-clock-multiple", &dm->min_hs_clock_multiple);
	of_property_read_u32(np, "mipi-dsi-multiple", &dm->mipi_dsi_multiple);
	of_property_read_u32(np, "mipi-delay-between-cmds", &p->mipi_delay_between_cmds);
	of_property_read_u32(np, "enable-high-duration-us", &p->enable_high_duration_us);
	of_property_read_u32(np, "enable-low-duration-us", &p->enable_low_duration_us);
	of_property_read_u32(np, "power-delay-ms", &p->power_delay_ms);
	if (dsi) {
		if (of_property_read_bool(np, "mode-video-hfp-disable"))
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_NO_HFP;
		if (of_property_read_bool(np, "mode-video-hbp-disable"))
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_NO_HBP;
		if (of_property_read_bool(np, "mode-video-hsa-disable"))
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_NO_HSA;
	}
}

static int panel_common_probe(struct device *dev, const struct panel_desc *desc,
		struct mipi_dsi_device *dsi)
{
	struct device_node *spi_node;
	struct device_node *np = dev->of_node;
	struct device_node *i2c_node;
	struct device_node *sn65_np;
	struct spi_device *spi = NULL;
	struct i2c_adapter *i2c = NULL;
	struct panel_common *panel;
	struct display_timing dt;
	struct device_node *ddc;
	struct clk *mipi_clk;
	int connector_type;
	u32 bus_flags;
	int err;

	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	panel->enabled = false;
	panel->enabled2 = false;
	panel->prepared = false;
	panel->desc = desc;
	if (!desc) {
		struct videomode vm;
		struct drm_display_mode *dm = &panel->dt_mode;
		const char *bf;
		struct panel_desc *ds = &panel->dt_desc;
		struct device_node *cmds_np;
		u32 bridge_de_active;
		u32 bridge_sync_active;

		desc = ds;
		ds->connector_type = DRM_MODE_CONNECTOR_DSI;

		of_property_read_u32(np, "panel-width-mm", &ds->size.width);
		of_property_read_u32(np, "panel-height-mm", &ds->size.height);
		err = of_get_videomode(np, &vm, 0);
		if (of_property_read_u32(np, "bridge-de-active", &bridge_de_active)) {
			bridge_de_active = -1;
		}
		if (of_property_read_u32(np, "bridge-sync-active", &bridge_sync_active)) {
			bridge_sync_active = -1;
		}

		if (err < 0)
			return err;
		drm_display_mode_from_videomode(&vm, dm);
		if (vm.flags & DISPLAY_FLAGS_DE_HIGH)
			ds->bus_flags |= DRM_BUS_FLAG_DE_HIGH;
		if (vm.flags & DISPLAY_FLAGS_DE_LOW)
			ds->bus_flags |= DRM_BUS_FLAG_DE_LOW;
		if (bridge_de_active <= 1) {
			ds->bus_flags &= ~(DRM_BUS_FLAG_DE_HIGH |
					DRM_BUS_FLAG_DE_LOW);
			ds->bus_flags |= bridge_de_active ? DRM_BUS_FLAG_DE_HIGH
					: DRM_BUS_FLAG_DE_LOW;
		}

		if (bridge_sync_active <= 1) {
			dm->flags &= ~(
				DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
				DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC);
			dm->flags |= bridge_sync_active ?
				DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC :
				DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC;
		}
		if (vm.flags & DISPLAY_FLAGS_PIXDATA_NEGEDGE)
			ds->bus_flags |= DRM_BUS_FLAG_PIXDATA_SAMPLE_NEGEDGE;
		if (vm.flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
			ds->bus_flags |= DRM_BUS_FLAG_PIXDATA_SAMPLE_POSEDGE;
		dev_info(dev, "vm.flags=%x bus_flags=%x flags=%x\n", vm.flags, ds->bus_flags, dm->flags);

		err = of_property_read_string(np, "bus-format", &bf);
		if (err) {
			dev_err(dev, "bus-format missing %d\n", err);
			return err;
		}
		if (!strcmp(bf, "rgb888")) {
			ds->bus_format = MEDIA_BUS_FMT_RGB888_1X24;
		} else if (!strcmp(bf, "rgb666")) {
			ds->bus_format = MEDIA_BUS_FMT_RGB666_1X18;
		} else {
			dev_err(dev, "unknown bus-format %s\n", bf);
			return -EINVAL;
		}
		init_common(panel, np, ds, dm, dsi);
		of_property_read_u32(np, "bits-per-color", &ds->bpc);
		ds->modes = dm;
		ds->num_modes = 1;
		panel->desc = ds;
		cmds_np = of_parse_phandle(np, "mipi-cmds", 0);
		if (cmds_np) {
			struct fwnode_handle *fwnode;

			fwnode = of_fwnode_handle(cmds_np);
			panel->gpd_display_enable =
				devm_fwnode_get_gpiod_from_child(dev,
					"display-enable", fwnode,
					GPIOD_OUT_LOW, "display-enable");
			if (IS_ERR(panel->gpd_display_enable)) {
				err = PTR_ERR(panel->gpd_display_enable);
				if (err != -ENOENT)
					dev_err(dev, "failed to request display-enable: %d\n", err);
				panel->gpd_display_enable = NULL;
			}

			i2c_node = of_parse_phandle(cmds_np, "i2c-bus", 0);
			if (i2c_node) {
				i2c = of_find_i2c_adapter_by_node(i2c_node);
				of_node_put(i2c_node);

				if (!i2c) {
					pr_debug("%s:i2c deferred\n", __func__);
					return -EPROBE_DEFER;
				}
				panel->i2c = i2c;
			}

			spi_node = of_parse_phandle(cmds_np, "spi", 0);
			if (spi_node) {
				spi = of_find_spi_device_by_node(spi_node);
				of_node_put(spi_node);

				if (!spi) {
					pr_debug("%s:spi deferred\n", __func__);
					err = -EPROBE_DEFER;
					goto free_i2c;
				}
				panel->spi = spi;
			}

			if (i2c) {
				check_for_cmds(cmds_np, "i2c-cmds-init",
				       &panel->cmds_init.i2c);
				check_for_cmds(cmds_np, "i2c-cmds-enable",
				       &panel->cmds_enable.i2c);
				check_for_cmds(cmds_np, "i2c-cmds-enable2",
					       &panel->cmds_enable2.i2c);
				check_for_cmds(cmds_np, "i2c-cmds-disable",
				       &panel->cmds_disable.i2c);
				of_property_read_u32(cmds_np, "i2c-address", &panel->i2c_address);
				of_property_read_u32(cmds_np, "i2c-max-frequency", &panel->i2c_max_frequency);
			}
			check_for_cmds(cmds_np, "mipi-cmds-init",
				       &panel->cmds_init.mipi);
			check_for_cmds(cmds_np, "mipi-cmds-enable",
				       &panel->cmds_enable.mipi);
			/* enable 2 is after frame data transfer has started */
			check_for_cmds(cmds_np, "mipi-cmds-enable2",
				       &panel->cmds_enable2.mipi);
			check_for_cmds(cmds_np, "mipi-cmds-disable",
				       &panel->cmds_disable.mipi);

			if (spi) {
				if (of_property_read_bool(cmds_np, "spi-9-bit"))
					panel->spi_9bit = 1;
				check_for_cmds(cmds_np, "spi-cmds-init",
				       &panel->cmds_init.spi);
				check_for_cmds(cmds_np, "spi-cmds-enable",
				       &panel->cmds_enable.spi);
				check_for_cmds(cmds_np, "spi-cmds-enable2",
				       &panel->cmds_enable2.spi);
				check_for_cmds(cmds_np, "spi-cmds-disable",
				       &panel->cmds_disable.spi);
				of_property_read_u32(cmds_np, "spi-max-frequency", &panel->spi_max_frequency);
			}
			init_common(panel, cmds_np, ds, dm, dsi);
		}
		pr_info("%s: delay %d %d, %d %d\n", __func__,
			ds->delay.prepare, ds->delay.enable,
			ds->delay.disable, ds->delay.unprepare);
	}

	mipi_clk = devm_clk_get_optional(dev, "mipi_clk");
	if (IS_ERR(mipi_clk)) {
		err = PTR_ERR(mipi_clk);
		dev_dbg(dev, "%s:devm_clk_get mipi_clk  %d\n", __func__, err);
		return err;
	}
	panel->mipi_clk = mipi_clk;

	panel->no_hpd = of_property_read_bool(np, "no-hpd");
	if (!panel->no_hpd) {
		err = panel_common_get_hpd_gpio(dev, panel, true);
		if (err)
			return err;
	}

	panel->supply = devm_regulator_get(dev, "power");
	if (IS_ERR(panel->supply)) {
		err = PTR_ERR(panel->supply);
		goto free_spi;
	}

	panel->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(panel->reset)) {
		err = PTR_ERR(panel->reset);
		dev_err(dev, "failed to request reset: %d\n", err);
		goto free_spi;
	}

	panel->gpd_power = devm_gpiod_get_optional(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(panel->gpd_power)) {
		err = PTR_ERR(panel->gpd_power);
		dev_err(dev, "failed to request power: %d\n", err);
		goto free_spi;
	}

	panel->gpd_prepare_enable = devm_gpiod_get_optional(dev, "enable",
						     GPIOD_OUT_LOW);
	if (IS_ERR(panel->gpd_prepare_enable)) {
		err = PTR_ERR(panel->gpd_prepare_enable);
		if (err != -EPROBE_DEFER)
			dev_err(dev, "failed to request GPIO: %d\n", err);
		goto free_spi;
	}

	err = of_drm_get_panel_orientation(np, &panel->orientation);
	if (err) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", np, err);
		return err;
	}

	ddc = of_parse_phandle(np, "ddc-i2c-bus", 0);
	if (ddc) {
		panel->ddc = of_find_i2c_adapter_by_node(ddc);
		of_node_put(ddc);

		if (!panel->ddc)
			return -EPROBE_DEFER;
	}

	if (!of_get_display_timing(np, "panel-timing", &dt))
		panel_common_parse_panel_timing_node(dev, panel, &dt);

	sn65_np = of_parse_phandle(np, "sn65dsi83", 0);
	if (sn65_np) {
		if (of_device_is_available(sn65_np)) {
			panel->sn65.mipi_clk = mipi_clk;
			err = sn65_init(dev, &panel->sn65, np, sn65_np);
			if (err < 0)
				goto free_ddc;
		}
	}

	connector_type = desc->connector_type;
	/* Catch common mistakes for panels. */
	switch (connector_type) {
	case 0:
		dev_warn(dev, "Specify missing connector_type\n");
		connector_type = DRM_MODE_CONNECTOR_DPI;
		break;
	case DRM_MODE_CONNECTOR_LVDS:
		WARN_ON(desc->bus_flags &
			~(DRM_BUS_FLAG_DE_LOW |
			  DRM_BUS_FLAG_DE_HIGH |
			  DRM_BUS_FLAG_DATA_MSB_TO_LSB |
			  DRM_BUS_FLAG_DATA_LSB_TO_MSB));
		WARN_ON(desc->bus_format != MEDIA_BUS_FMT_RGB666_1X7X3_SPWG &&
			desc->bus_format != MEDIA_BUS_FMT_RGB888_1X7X4_SPWG &&
			desc->bus_format != MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA);
		WARN_ON(desc->bus_format == MEDIA_BUS_FMT_RGB666_1X7X3_SPWG &&
			desc->bpc != 6);
		WARN_ON((desc->bus_format == MEDIA_BUS_FMT_RGB888_1X7X4_SPWG ||
			 desc->bus_format == MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA) &&
			desc->bpc != 8);
		break;
	case DRM_MODE_CONNECTOR_eDP:
		if (desc->bus_format == 0)
			dev_warn(dev, "Specify missing bus_format\n");
		if (desc->bpc != 6 && desc->bpc != 8)
			dev_warn(dev, "Expected bpc in {6,8} but got: %u\n", desc->bpc);
		break;
	case DRM_MODE_CONNECTOR_DSI:
		if (desc->bpc != 6 && desc->bpc != 8)
			dev_warn(dev, "Expected bpc in {6,8} but got: %u\n", desc->bpc);
		break;
	case DRM_MODE_CONNECTOR_DPI:
		bus_flags = DRM_BUS_FLAG_DE_LOW |
			    DRM_BUS_FLAG_DE_HIGH |
			    DRM_BUS_FLAG_PIXDATA_SAMPLE_POSEDGE |
			    DRM_BUS_FLAG_PIXDATA_SAMPLE_NEGEDGE |
			    DRM_BUS_FLAG_DATA_MSB_TO_LSB |
			    DRM_BUS_FLAG_DATA_LSB_TO_MSB |
			    DRM_BUS_FLAG_SYNC_SAMPLE_POSEDGE |
			    DRM_BUS_FLAG_SYNC_SAMPLE_NEGEDGE;
		if (desc->bus_flags & ~bus_flags)
			dev_warn(dev, "Unexpected bus_flags(%d)\n", desc->bus_flags & ~bus_flags);
		if (!(desc->bus_flags & bus_flags))
			dev_warn(dev, "Specify missing bus_flags\n");
		if (desc->bus_format == 0)
			dev_warn(dev, "Specify missing bus_format\n");
		if (desc->bpc != 6 && desc->bpc != 8)
			dev_warn(dev, "Expected bpc in {6,8} but got: %u\n", desc->bpc);
		break;
	default:
		dev_warn(dev, "Specify a valid connector_type: %d\n", desc->connector_type);
		connector_type = DRM_MODE_CONNECTOR_DPI;
		break;
	}

	drm_panel_init(&panel->base, dev, &panel_common_funcs, connector_type);

	err = drm_panel_of_backlight(&panel->base);
	if (err)
		goto free_ddc;

	drm_panel_add(&panel->base);

	dev_set_drvdata(dev, panel);

	return 0;

free_ddc:
	if (panel->ddc)
		put_device(&panel->ddc->dev);
free_spi:
	if (spi)
		put_device(&spi->dev);
free_i2c:
	if (i2c)
		put_device(&i2c->dev);

	return err;
}

static int panel_common_remove(struct device *dev)
{
	struct panel_common *panel = dev_get_drvdata(dev);

	drm_panel_remove(&panel->base);
	drm_panel_disable(&panel->base);
	drm_panel_unprepare(&panel->base);

	if (panel->ddc)
		put_device(&panel->ddc->dev);
	if (panel->spi)
		put_device(&panel->spi->dev);
	if (panel->i2c)
		put_device(&panel->i2c->dev);

	sn65_remove(&panel->sn65);
	return 0;
}

static void panel_common_shutdown(struct device *dev)
{
	struct panel_common *panel = dev_get_drvdata(dev);

	drm_panel_disable(&panel->base);
	drm_panel_unprepare(&panel->base);
}

struct panel_desc_dsi {
	struct panel_desc desc;

	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
};

static const struct of_device_id dsi_of_match[] = {
	{
		.compatible = "panel,common",
		.data = NULL
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, dsi_of_match);

static int panel_common_dsi_probe(struct mipi_dsi_device *dsi)
{
	const struct panel_desc_dsi *desc;
	const struct of_device_id *id;
	const struct panel_desc *pd = NULL;
	int err;

	id = of_match_node(dsi_of_match, dsi->dev.of_node);
	if (!id)
		return -ENODEV;

	desc = id->data;

	if (desc) {
		dsi->mode_flags = desc->flags;
		dsi->format = desc->format;
		dsi->lanes = desc->lanes;
		pd = &desc->desc;
	} else {
		struct device_node *np = dsi->dev.of_node;
		const char *df;

		err = of_property_read_u32(np, "dsi-lanes", &dsi->lanes);
		if (err < 0) {
			dev_err(&dsi->dev, "Failed to get dsi-lanes property (%d)\n", err);
			return err;
		}
		err = of_property_read_string(np, "dsi-format", &df);
		if (err) {
			dev_err(&dsi->dev, "dsi-format missing. %d\n", err);
			return err;
		}
		if (!strcmp(df, "rgb888")) {
			dsi->format = MIPI_DSI_FMT_RGB888;
		} else if (!strcmp(df, "rgb666")) {
			dsi->format = MIPI_DSI_FMT_RGB666;
		} else {
			dev_err(&dsi->dev, "unknown dsi-format %s\n", df);
			return -EINVAL;
		}
		if (of_property_read_bool(np, "mode-clock-non-contiguous"))
			dsi->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS;
		if (of_property_read_bool(np, "mode-skip-eot"))
			dsi->mode_flags |= MIPI_DSI_MODE_NO_EOT_PACKET;
		if (of_property_read_bool(np, "mode-video"))
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO;
		if (of_property_read_bool(np, "mode-video-burst"))
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
		if (of_property_read_bool(np, "mode-video-hse"))
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_HSE;
		if (of_property_read_bool(np, "mode-video-mbc"))
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_MBC;
		if (of_property_read_bool(np, "mode-video-sync-pulse"))
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	}
	err = panel_common_probe(&dsi->dev, pd, dsi);
	if (err < 0)
		return err;

	err = mipi_dsi_attach(dsi);
	if (err) {
		struct panel_common *panel = dev_get_drvdata(&dsi->dev);

		drm_panel_remove(&panel->base);
	}

	return err;
}

struct panel_sn65dsi83 *dev_to_panel_sn65(struct device *dev)
{
	struct panel_common *panel = dev_get_drvdata(dev);

	return &panel->sn65;
}

static void panel_common_dsi_remove(struct mipi_dsi_device *dsi)
{
	int err;

	err = mipi_dsi_detach(dsi);
	if (err < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", err);

	panel_common_remove(&dsi->dev);
}

static void panel_common_dsi_shutdown(struct mipi_dsi_device *dsi)
{
	panel_common_shutdown(&dsi->dev);
}

static struct mipi_dsi_driver panel_common_dsi_driver = {
	.driver = {
		.name = "panel-common",
		.of_match_table = dsi_of_match,
	},
	.probe = panel_common_dsi_probe,
	.remove = panel_common_dsi_remove,
	.shutdown = panel_common_dsi_shutdown,
};

static int __init panel_common_init(void)
{
	int err;

	err = mipi_dsi_driver_register(&panel_common_dsi_driver);
	if (err < 0)
		return err;

	return 0;
}
module_init(panel_common_init);

static void __exit panel_common_exit(void)
{
	mipi_dsi_driver_unregister(&panel_common_dsi_driver);
}
module_exit(panel_common_exit);

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("DRM Driver for common Panels");
MODULE_LICENSE("GPL and additional rights");
