/*
 *  tc358778.c - rgb to mipi output chip
 *
 *  Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_notifier.h>
#include <drm/drm_mode.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/mxcfb.h>
#include <linux/kthread.h>
#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/mipi_display.h>
#include <video/videomode.h>
#include <dt-bindings/display/simple_panel_mipi_cmds.h>

#define HALT_CHECK_DELAY	100
#define HALT_CHECK_CNT		10

struct mipi_cmd {
	const u8* mipi_cmds;
	unsigned length;
};

struct tc358778_priv
{
	struct i2c_client	*client;
	struct device_node	*disp_node;
	struct gpio_desc	*gp_reset;
	struct device_node	*of_node;
	struct notifier_block	fbnb;
	struct notifier_block	drmnb;
	struct clk		*pixel_clk;
	struct mutex		power_mutex;
	struct delayed_work	tc_work;
	struct workqueue_struct *tc_workqueue;
	unsigned long		mode_flags;
	u32			int_cnt;
	u32			pixelclock;
	u32			byte_clock;
	u32			bit_clock;
	u16			show_reg;
	u16			pllctrl1;
	u16			datafmt;
	u8			chip_enabled;
	u8			pixel_clk_active;
	u8			dsi_lanes;
	u8			dsi_bpp;
	u8			pulse;
	u8			skip_eot;
	u8			gp_reset_active;
	u8			halt_check_cnt;
	struct mipi_cmd		mipi_cmds_init;
	struct mipi_cmd		mipi_cmds_enable;
	struct mipi_cmd		mipi_cmds_disable;
	struct mipi_cmd		mipi_cmds_detect;
};

#define R32	0x8000

/* register definitions according to the tc358778 data sheet */
/* Global 16-bit registers */
#define TC_CHIPID		0x0000
#define TC_SYSCTL		0x0002
#define TC_CONFCTL		0x0004
#define TC_VSDLY		0x0006
#define TC_DATAFMT		0x0008
#define TC_GPIOEN		0x000e
#define TC_GPIODIR		0x0010
#define TC_GPIOIN		0x0012
#define TC_GPIOOUT		0x0014
#define TC_PLLCTL0		0x0016
#define TC_PLLCTL1		0x0018
#define TC_CMDBYTE		0x0022
#define TC_PP_MISC		0x0032
#define TC_DSITX_DT		0x0050
#define TC_FIFOSTATUS		0x00f8

/* TX PHY 32-bit(A16 of address set means 32 bits) registers */
#define TC_CLW_DPHYCONTTX	(0x0100 + R32)
#define TC_D0W_DPHYCONTTX	(0x0104 + R32)
#define TC_D1W_DPHYCONTTX	(0x0108 + R32)
#define TC_D2W_DPHYCONTTX	(0x010c + R32)
#define TC_D3W_DPHYCONTTX	(0x0110 + R32)
#define TC_CLW_CNTRL		(0x0140 + R32)
#define TC_D0W_CNTRL		(0x0144 + R32)
#define TC_D1W_CNTRL		(0x0148 + R32)
#define TC_D2W_CNTRL		(0x014c + R32)
#define TC_D3W_CNTRL		(0x0150 + R32)

/* TX PPI 32-bit(A16 of address set means 32 bits) registers */
#define TC_STARTCNTRL		(0x0204 + R32)
#define TC_STATUS		(0x0208 + R32)
#define TC_LINEINITCNT		(0x0210 + R32)
#define TC_LPTXTIMECNT		(0x0214 + R32)
#define TC_TCLK_HEADERCNT	(0x0218 + R32)
#define TC_TCLK_TRAILCNT	(0x021c + R32)
#define TC_THS_HEADERCNT	(0x0220 + R32)
#define TC_TWAKEUP		(0x0224 + R32)
#define TC_TCLK_POSTCNT		(0x0228 + R32)
#define TC_THS_TRAILCNT		(0x022c + R32)
#define TC_HSTXVREGCNT		(0x0230 + R32)
#define TC_HSTXVREGEN		(0x0234 + R32)
#define TC_TXOPTIONCNTRL	(0x0238 + R32)
#define TC_BTACNTRL1		(0x023c + R32)

/* TX CTRL 32-bit(A16 of address set means 32 bits) registers */
#define TC_DSI_CONTROL		(0x040c + R32)
#define TC_DSI_STATUS		(0x0410 + R32)
#define TC_DSI_INT		(0x0414 + R32)
#define TC_DSI_INT_ENA		(0x0418 + R32)
#define TC_DSICMD_RXFIFO	(0x0430 + R32)
#define TC_DSI_ACKERR		(0x0434 + R32)
#define TC_DSI_ACKERR_INTENA	(0x0438 + R32)
#define TC_DSI_ACKERR_HALT	(0x043c + R32)
#define TC_DSI_RXERR		(0x0440 + R32)
#define TC_DSI_RXERR_INTENA	(0x0444 + R32)
#define TC_DSI_RXERR_HALT	(0x0448 + R32)
#define TC_DSI_ERR		(0x044c + R32)
#define TC_DSI_ERR_INTENA	(0x0450 + R32)
#define TC_DSI_ERR_HALT		(0x0454 + R32)
#define TC_DSI_CONFW		(0x0500 + R32)
#define TC_DSI_RESET		(0x0504 + R32)
#define TC_DSI_INT_CLR		(0x050c + R32)
#define TC_DSI_START		(0x0518 + R32)

/* DSITX CTRL 16-bit registers */
#define TC_DSICMD_TX		0x0600
#define TC_DSICMD_TYPE		0x0602
#define TC_DSICMD_WC		0x0604
#define TC_DSICMD_WD0		0x0610
#define TC_DSICMD_WD1		0x0612
#define TC_DSICMD_WD2		0x0614
#define TC_DSICMD_WD3		0x0616
#define TC_DSI_EVENT		0x0620
#define TC_DSI_VSW		0x0622
#define TC_DSI_VBPR		0x0624
#define TC_DSI_VACT		0x0626
#define TC_DSI_HSW		0x0628
#define TC_DSI_HBPR		0x062a
#define TC_DSI_HACT		0x062c

/* DEBUG 16-bit registers */
#define TC_VBUFCTL		0x00e0
#define TC_DBG_WIDTH		0x00e2
#define TC_DBG_VBLANK		0x00e4
#define TC_DBG_DATA		0x00e8

#define SET_DSI_CONTROL		0xa3000000
#define SET_DSI_INT_ENA		0xa6000000
#define CLR_DSI_CONTROL		0xc3000000

static const char *client_name = "tc358778";

static const u16 registers_to_show[] = {
	TC_CHIPID,
	TC_SYSCTL,
	TC_CONFCTL,
	TC_VSDLY,
	TC_DATAFMT,
	TC_GPIOEN,
	TC_GPIODIR,
	TC_GPIOIN,
	TC_GPIOOUT,
	TC_PLLCTL0,
	TC_PLLCTL1,
	TC_CMDBYTE,
	TC_PP_MISC,
	TC_DSITX_DT,
	TC_FIFOSTATUS,

/* TX PHY 32-bit(A16 of address set means 32 bits) registers */
	TC_CLW_DPHYCONTTX,
	TC_D0W_DPHYCONTTX,
	TC_D1W_DPHYCONTTX,
	TC_D2W_DPHYCONTTX,
	TC_D3W_DPHYCONTTX,
	TC_CLW_CNTRL,
	TC_D0W_CNTRL,
	TC_D1W_CNTRL,
	TC_D2W_CNTRL,
	TC_D3W_CNTRL,

/* TX PPI 32-bit(A16 of address set means 32 bits) registers */
	TC_STARTCNTRL,
	TC_STATUS,
	TC_LINEINITCNT,
	TC_LPTXTIMECNT,
	TC_TCLK_HEADERCNT,
	TC_TCLK_TRAILCNT,
	TC_THS_HEADERCNT,
	TC_TWAKEUP,
	TC_TCLK_POSTCNT,
	TC_THS_TRAILCNT,
	TC_HSTXVREGCNT,
	TC_HSTXVREGEN,
	TC_TXOPTIONCNTRL,
	TC_BTACNTRL1,

/* TX CTRL 32-bit(A16 of address set means 32 bits) registers */
	TC_DSI_CONTROL,
	TC_DSI_STATUS,
	TC_DSI_INT,
	TC_DSI_INT_ENA,
	TC_DSICMD_RXFIFO,
	TC_DSI_ACKERR,
	TC_DSI_ACKERR_INTENA,
	TC_DSI_ACKERR_HALT,
	TC_DSI_RXERR,
	TC_DSI_RXERR_INTENA,
	TC_DSI_RXERR_HALT,
	TC_DSI_ERR,
	TC_DSI_ERR_INTENA,
	TC_DSI_ERR_HALT,
	TC_DSI_CONFW,
	TC_DSI_RESET,
	TC_DSI_INT_CLR,
	TC_DSI_START,

/* DSITX CTRL 16-bit registers */
	TC_DSICMD_TX,
	TC_DSICMD_TYPE,
	TC_DSICMD_WC,
	TC_DSICMD_WD0,
	TC_DSICMD_WD1,
	TC_DSICMD_WD2,
	TC_DSICMD_WD3,
	TC_DSI_EVENT,
	TC_DSI_VSW,
	TC_DSI_VBPR,
	TC_DSI_VACT,
	TC_DSI_HSW,
	TC_DSI_HBPR,
	TC_DSI_HACT,

/* DEBUG 16-bit registers */
	TC_VBUFCTL,
	TC_DBG_WIDTH,
	TC_DBG_VBLANK,
	TC_DBG_DATA,
};

/*
 * tc_read16 - read data from a register of the i2c slave device.
 */
static int tc_read16(struct tc358778_priv *tc, u16 reg)
{
	struct i2c_client *client = tc->client;
	struct i2c_msg msgs[2];
	u8 buf[2];
	int ret;
	int val;

	if (reg >= R32) {
		dev_err(&client->dev, "%s: bad reg=%x\n", __func__, reg);
		return -EINVAL;
	}
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = buf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s:reg=%x ret=%d\n", __func__, reg, ret);
		return ret;
	}
	val = (buf[0] << 8) | buf[1];
	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, val);
	return val;
}

/*
 * tc_read32 - read data from a 32-bit register of the i2c slave device.
 */
static int tc_read32(struct tc358778_priv *tc, u16 reg, u32* result)
{
	struct i2c_client *client = tc->client;
	struct i2c_msg msgs[2];
	u8 buf[4];
	int ret;

	if (reg < R32) {
		dev_err(&client->dev, "%s: bad reg=%x\n", __func__, reg);
		return -EINVAL;
	}
	reg -= R32;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 4;
	msgs[1].buf = buf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s:reg=%x ret=%d\n", __func__, reg, ret);
		return ret;
	}
	*result = (buf[0] << 8) | buf[1] |
		(buf[2] << 24) | (buf[3] << 16);
	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, *result);
	return 0;
}

/*
 * tc_write16 - write data to a register of the i2c slave device.
 */
static int tc_write16(struct tc358778_priv *tc, u16 reg, u16 val)
{
	struct i2c_client *client = tc->client;
	int ret;
	u8 buf[4];

	if (reg >= R32) {
		dev_err(&client->dev, "%s: bad reg=%x\n", __func__, reg);
		return -EINVAL;
	}
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

	ret = i2c_master_send(client, buf, 4);

	if (ret < 0)
		dev_err(&client->dev, "%s failed(%i) reg(%04x) = %04x\n",
				__func__, ret, reg, val);
	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, val);
	return ret;
}

static int tc_update16(struct tc358778_priv *tc, u16 reg, u16 clear_mask, u16 set_mask)
{
	int val = tc_read16(tc, reg);

	if (val < 0)
		return val;
	val &= ~clear_mask;
	val |= set_mask;
	return tc_write16(tc, reg, val);
}

/*
 * tc_write32 - write data to a register of the i2c slave device.
 */
static int tc_write32(struct tc358778_priv *tc, u16 reg, u32 val)
{
	struct i2c_client *client = tc->client;
	int ret;
	u8 buf[8];

	if (reg < R32) {
		dev_err(&client->dev, "%s: bad reg=%x\n", __func__, reg);
		return -EINVAL;
	}
	reg -= R32;
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = (val >> 8) & 0xff;
	buf[3] = val & 0xff;
	buf[4] = (val >> 24) & 0xff;
	buf[5] = (val >> 16) & 0xff;

	ret = i2c_master_send(client, buf, 6);

	if (ret < 0)
		dev_err(&client->dev, "%s failed(%i) reg(%04x) = %08x\n",
				__func__, ret, reg, val);
	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, val);
	return ret;
}

static int tc_flush_read_fifo(struct tc358778_priv *tc)
{
	u32 val;
	u32 status;
	int ret;
	int try = 0;

	do {
		ret = tc_read32(tc, TC_DSI_STATUS, &status);
		if (ret < 0)
			return ret;
		if (!(status & 0x20))
			break;
		if (try++ >= 1000)
			return -ETIMEDOUT;
		ret = tc_read32(tc, TC_DSICMD_RXFIFO, &val);
		if (ret < 0)
			return ret;
		pr_info("%s: fifo %08x\n", __func__, val);
	} while (1);
	return 0;
}

#if 0
static int check_for_halt(struct tc358778_priv *tc, char from)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(20);
	unsigned status;
	int ret;

	do {
		ret = tc_read32(tc, TC_DSI_STATUS, &status);
		if (ret >= 0) {
			if (status & 1) {
				pr_err("%s: halted, 0x%x %c\n", __func__, ret, from);
				return -EIO;
			}
			break;
		}
		if (time_after(jiffies, timeout)) {
			pr_err("%s: timeout %d\n", __func__, ret);
			return -ETIMEDOUT;
		}
	} while (1);
	return 0;
}
#else
static int check_for_halt(struct tc358778_priv *tc, char from)
{
	return 0;
}
#endif

static int tc_start_tx_and_complete(struct tc358778_priv *tc)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(50);
	int ret;

	tc_write16(tc, TC_DSICMD_TX, 1);	/* Start */
	do {
		ret = tc_read16(tc, TC_DSICMD_TX);
		if (ret < 0)
			continue;
		if (!(ret & 1)) {
			ret = check_for_halt(tc, 'x');
			if (ret)
				return ret;
			break;
		}
		if (time_after(jiffies, timeout)) {
			pr_err("%s: timeout, 0x%x\n", __func__, ret);
			return -ETIMEDOUT;
		}
		msleep(1);
	} while (1);
	return 0;
}

static int tc_wait_tx_idle(struct tc358778_priv *tc)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(50);
	int ret;
	int status = 0;

	do {
		ret = tc_read32(tc, TC_DSI_STATUS, &status);
		if ((ret >= 0) && !(ret & 0x200))
				break;
		if (time_after(jiffies, timeout)) {
			pr_err("%s: timeout, 0x%x\n", __func__, ret);
			return -ETIMEDOUT;
		}
		msleep(1);
	} while (1);
	return 0;
}

#define DCS_TYPE(type)	(type << 8)
#define DCS_ID(id)	id

static int tc_mipi_dsi_pkt_write_indirect(struct tc358778_priv *tc, u8 id, const u8* buf, int buf_cnt)
{
	u16 val;
	int ret;
	int delay = (buf_cnt + 19) / 10;

	if ((buf_cnt <= 8) || (buf_cnt > 1024))
		return -EINVAL;
	tc_wait_tx_idle(tc);
	check_for_halt(tc, 'i');
	tc_write16(tc, TC_DSITX_DT, DCS_ID(id));
	tc_write16(tc, TC_CMDBYTE, buf_cnt);
	tc_write16(tc, TC_VBUFCTL, 0x8000);
	while (buf_cnt) {
		val = *buf++;
		buf_cnt--;
		if (buf_cnt) {
			val |= (*buf++ << 8);
			buf_cnt--;
		}
		tc_write16(tc, TC_DBG_DATA, val);
		val = 0;
		if (buf_cnt) {
			val = *buf++;
			buf_cnt--;
		}
		if (buf_cnt) {
			val |= (*buf++ << 8);
			buf_cnt--;
		}
		tc_write16(tc, TC_DBG_DATA, val);
	}
	tc_write16(tc, TC_VBUFCTL, 0xe000);	/* start */
	msleep(delay);
	tc_wait_tx_idle(tc);
	tc_write16(tc, TC_VBUFCTL, 0x2000);
	check_for_halt(tc, '2');
	ret = tc_write16(tc, TC_VBUFCTL, 0);
	return ret;
}

static int tc_mipi_dsi_pkt_write(struct tc358778_priv *tc, u8 id, const u8* buf, int buf_cnt)
{
	int ret;
	u16 val;
	u8 type = (buf_cnt <= 2) ? 0x10 : 0x40;

	tc_flush_read_fifo(tc);
	if (buf_cnt > 8)
		return tc_mipi_dsi_pkt_write_indirect(tc, id, buf, buf_cnt);
	tc_write16(tc, TC_DSICMD_TYPE, DCS_TYPE(type) |
			DCS_ID(id));
	tc_write16(tc, TC_DSICMD_WC, buf_cnt <= 2 ? 0 : buf_cnt);
	val = buf[0];
	if (buf_cnt > 1)
		val |= (buf[1] << 8);
	tc_write16(tc, TC_DSICMD_WD0, val);
	if (buf_cnt > 2) {
		val = buf[2];
		if (buf_cnt > 3)
			val |= (buf[3] << 8);
		tc_write16(tc, TC_DSICMD_WD1, val);
		if (buf_cnt > 4) {
			val = buf[4];
			if (buf_cnt > 5)
				val |= (buf[5] << 8);
			tc_write16(tc, TC_DSICMD_WD2, val);
			if (buf_cnt > 6) {
				val = buf[6];
				if (buf_cnt > 7)
					val |= (buf[7] << 8);
				tc_write16(tc, TC_DSICMD_WD3, val);
			}
		}
	}
	ret = tc_start_tx_and_complete(tc);
	if (ret < 0)
		dev_err(&tc->client->dev, "%s: write failed(%d) %02x %02x\n", __func__, ret, buf[0], buf[1]);

	return ret;
}

static int tc_mipi_dsi_pkt_read(struct tc358778_priv *tc, u8 id, const u8* cmd, u8* dst, int dst_cnt)
{
	u8 type = (dst_cnt <= 2) ? 0x10 : 0x40;
	int ret;
	u32 val;
	u32 status;
	u32 resp = 0;
	int try = 0;

	tc_flush_read_fifo(tc);
	tc_write16(tc, TC_DSICMD_TYPE, DCS_TYPE(type) | DCS_ID(id));
	tc_write16(tc, TC_DSICMD_WC, 0);
	tc_write16(tc, TC_DSICMD_WD0, cmd[0]);
	tc_start_tx_and_complete(tc);

	while (dst_cnt) {
		do {
			status = 0;
			ret = tc_read32(tc, TC_DSI_STATUS, &status);
			if (ret < 0)
				continue;
			if (status & 0x20)
				break;
			if (try++ >= 1000)
				return -ETIMEDOUT;
		} while (1);

		ret = tc_read32(tc, TC_DSICMD_RXFIFO, &val);
		if (ret < 0)
			return ret;
		if (!resp) {
			resp = val & 0xff;
			val >>= 8;
			switch (resp) {
			case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
			case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
				*dst++ = val;
				if (!--dst_cnt)
					break;
				val >>= 8;
				fallthrough;
			case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
			case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
				*dst++ = val;
				--dst_cnt;
				if (--dst_cnt)
					return -EINVAL;
				break;
			}
			continue;
		}
		*dst++ = val;
		if (!--dst_cnt)
			break;

		val >>= 8;
		*dst++ = val;
		if (!--dst_cnt)
			break;

		val >>= 8;
		*dst++ = val;
		if (!--dst_cnt)
			break;

		val >>= 8;
		*dst++ = val;
		if (!--dst_cnt)
			break;
	}
	return 0;
}

static int send_mipi_cmd_list(struct tc358778_priv *tc, struct mipi_cmd *mc)
{
	struct fb_var_screeninfo *var = NULL;
	const u8 *cmd = mc->mipi_cmds;
	unsigned length = mc->length;
	u8 data[8];
	u8 cmd_buf[32];
	const u8 *p;
	unsigned l;
	unsigned len;
	unsigned mask;
	u32 mipi_clk_rate = 0;
	u64 tmp;
	int ret;
	int generic;
	int match = 0;
	u64 readval, matchval;
	int skip = 0;
	int match_index;
	int i;

	pr_debug("%s:%p %x\n", __func__, cmd, length);
	if (!cmd || !length)
		return 0;

	while (1) {
		len = *cmd++;
		length--;
		if (!length)
			break;
		if ((len >= S_IF_1_LANE) && (len <= S_IF_4_LANES)) {
			int lane_match = 1 + len - S_IF_1_LANE;

			if (lane_match != tc->dsi_lanes)
				skip = 1;
			continue;
		} else if (len == S_IF_BURST) {
			if (!(tc->mode_flags & MIPI_DSI_MODE_VIDEO_BURST))
				skip = 1;
			continue;
		} else if (len == S_IF_NONBURST) {
			if (tc->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
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
				dev_err(&tc->client->dev, "Unexpected end of data\n");
				break;
			}
			if (!skip) {
				u8 id = 0;

				if (generic) {
					if (len > 2)
						id = MIPI_DSI_GENERIC_LONG_WRITE;
					else if (len == 2)
						id = MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM;
					else if (len == 1)
						id = MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM;
				} else {
					if (len > 2)
						id = MIPI_DSI_DCS_LONG_WRITE;
					else if (len == 2)
						id = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
					else if (len == 1)
						id = MIPI_DSI_DCS_SHORT_WRITE;
				}
				if (id)
					ret = tc_mipi_dsi_pkt_write(tc, id, p, l);
			}
		} else if (len == S_MRPS) {
			l = len = 1;
			ret = tc_mipi_dsi_pkt_write(tc,
					MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
					cmd, len);
		} else if ((len >= S_DCS_READ1) && (len <= S_DCS_READ8)) {
			data[0] = data[1] = data[2] = data[3] = 0;
			data[4] = data[5] = data[6] = data[7] = 0;
			len = len - S_DCS_READ1 + 1;
			match_index = generic ? 2 : 1;
			if (!skip) {
				/* fixme generic not done yet */
				ret =  tc_mipi_dsi_pkt_read(tc,
						MIPI_DSI_DCS_READ, cmd,
						data, len);
				readval = 0;
				matchval = 0;
				for (i = 0; i < len; i++) {
					readval |= ((u64)data[i]) << (i << 3);
					matchval |= ((u64)cmd[match_index + i]) << (i << 3);
				}
				if (generic) {
					pr_debug("Read (%d) GEN: (%04x) 0x%llx cmp 0x%llx\n",
						ret, cmd[0] + (cmd[1] << 8), readval, matchval);
				} else {
					pr_debug("Read (%d) DCS: (%02x) 0x%llx cmp 0x%llx\n",
						ret, cmd[0], readval, matchval);
				}
				if (readval != matchval)
					match = -EINVAL;
			}
			l = len = len + match_index;
		} else if (len == S_DELAY) {
			if (!skip) {
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

			if (!var) {
				struct fb_info *fbi = find_registered_fb_from_dt(tc->disp_node);
				if (fbi)
					var = &fbi->var;
			}

			scmd = len;
			dest_start = cmd[0];
			dest_len = cmd[1];
			val = 0;
			src_start = cmd[2];
			len = 3;
			if (scmd == S_CONST) {
				val = cmd[2] | (cmd[3] << 8) | (cmd[4] << 16) | (cmd[5] << 24);
				len = 6;
				src_start = 0;
			} else if (!var) {
				dev_err(&tc->client->dev, "Unknown display settings\n");
			} else {
				switch (scmd) {
				case S_HSYNC:
					val = var->hsync_len;
					break;
				case S_HBP:
					val = var->left_margin;
					break;
				case S_HACTIVE:
					val = var->xres;
					break;
				case S_HFP:
					val = var->right_margin;
					break;
				case S_VSYNC:
					val = var->vsync_len;
					break;
				case S_VBP:
					val = var->upper_margin;
					break;
				case S_VACTIVE:
					val = var->yres;
					break;
				case S_VFP:
					val = var->lower_margin;
					break;
				case S_LPTXTIME:
					val = 3;
					if (!mipi_clk_rate)
						mipi_clk_rate = tc->bit_clock >> 1;
					if (!mipi_clk_rate) {
						dev_warn(&tc->client->dev, "Unknown mipi_clk_rate\n");
						break;
					}
					/* val = ROUND(lptxtime_ns * mipi_clk_rate/4  /1000000000) */
#define lptxtime_ns 75
#define prepare_ns 100
#define zero_ns 250
					tmp = lptxtime_ns;
					tmp *= mipi_clk_rate;
					tmp += 2000000000ULL;
					do_div(tmp, 4000000000ULL);
					val = (unsigned)tmp;
					pr_debug("%s:lptxtime=%d\n", __func__, val);
					if (val > 2047)
						val = 2047;
					break;
				case S_CLRSIPOCOUNT:
					val = 5;
					if (!mipi_clk_rate)
						mipi_clk_rate = tc->bit_clock >> 1;
					if (!mipi_clk_rate) {
						dev_warn(&tc->client->dev, "Unknown mipi_clk_rate\n");
						break;
					}
					/* clrsipocount = ROUNDUP((prepare_ns + zero_ns/2) * mipi_clk_rate/4 /1000000000) - 5 */
					tmp = prepare_ns + (zero_ns >> 1) ;
					tmp *= mipi_clk_rate;
					tmp += 4000000000ULL - 1;
					do_div(tmp, 4000000000UL);
					val = (unsigned)tmp - 5;
					pr_debug("%s:clrsipocount=%d\n", __func__, val);
					if (val > 63)
						val = 63;
					break;
				default:
					dev_err(&tc->client->dev, "Unknown scmd 0x%x0x\n", scmd);
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
				dev_err(&tc->client->dev,
					"Failed to send (%d), (%d)%02x %02x: %02x %02x %02x %02x\n",
					ret, l, p[0], p[1], p[2], p[3], p[4], p[5]);
			} else if (l >= 2) {
				dev_err(&tc->client->dev,
					"Failed to send (%d), (%d)%02x %02x\n",
					ret, l, p[0], p[1]);
			} else {
				dev_err(&tc->client->dev,
					"Failed to send (%d), (%d)%02x\n",
					ret, l, p[0]);
			}
			return ret;
		} else {
			if (!skip) {
				if (l >= 18) {
					pr_debug("Sent (%d), (%d)%02x %02x: %02x %02x %02x %02x"
							"  %02x %02x %02x %02x"
							"  %02x %02x %02x %02x"
							"  %02x %02x %02x %02x\n",
						ret, l, p[0], p[1], p[2], p[3], p[4], p[5],
						p[6], p[7], p[8], p[9],
						p[10], p[11], p[12], p[13],
						p[14], p[15], p[16], p[17]);
				} else if (l >= 6) {
					pr_debug("Sent (%d), (%d)%02x %02x: %02x %02x %02x %02x\n",
						ret, l, p[0], p[1], p[2], p[3], p[4], p[5]);
				} else if (l >= 2) {
					pr_debug("Sent (%d), (%d)%02x %02x\n",
						ret, l, p[0], p[1]);
				} else if (l) {
					pr_debug("Sent (%d), (%d)%02x\n",
						ret, l, p[0]);
				}
			}
		}
		if (length < len) {
			dev_err(&tc->client->dev, "Unexpected end of data\n");
			break;
		}
		cmd += len;
		length -= len;
		if (!length)
			break;
		skip = 0;
	}
	return match;
};

static void set_speed_lpm_mode(struct tc358778_priv *tc, bool lpm_mode)
{
	if (lpm_mode) {
		/* Set to LP mode */
		tc->mode_flags |= MIPI_DSI_MODE_LPM;
		tc_write32(tc, TC_DSI_CONFW, CLR_DSI_CONTROL | 0x80);
	} else {
		/* Set to HS mode */
		tc->mode_flags &= ~MIPI_DSI_MODE_LPM;
		tc_write32(tc, TC_DSI_CONFW, SET_DSI_CONTROL | 0x80);
	}

}
static void tc_powerdown_locked(struct tc358778_priv *tc, int cmd_disable)
{
	tc->chip_enabled = 0;
	if (tc->client->irq)
		disable_irq(tc->client->irq);
	set_speed_lpm_mode(tc, true);
	if (cmd_disable)
		send_mipi_cmd_list(tc, &tc->mipi_cmds_disable);
	/*
	 * Disable Parallel Input
	 * 1. Set FrmStop to 1, wait 1 frame
	 * 2. clear PP_En
	 * 3. Set RstPtr to 1
	 */
	tc_update16(tc, TC_PP_MISC, 0, BIT(15));

	msleep(35);
	tc_update16(tc, TC_CONFCTL, BIT(6), 0);
	tc_update16(tc, TC_PP_MISC, 0, BIT(14));
	/* Stop dsi continuous clock */
	tc_write32(tc, TC_TXOPTIONCNTRL, 0);
	/* Disable D-PHY */
	tc_write32(tc, TC_CLW_CNTRL, 0x1);
	tc_write32(tc, TC_D0W_CNTRL, 0x1);
	tc_write32(tc, TC_D1W_CNTRL, 0x1);
	tc_write32(tc, TC_D2W_CNTRL, 0x1);
	tc_write32(tc, TC_D3W_CNTRL, 0x1);
	/* Disable PLL */
	tc_write16(tc, TC_PLLCTL1, tc->pllctrl1 & ~0x10);
	tc_write16(tc, TC_PLLCTL1, tc->pllctrl1 & ~0x13);
	/* Power down ppi */
	tc_write32(tc, TC_HSTXVREGEN, 0);
	/* assert reset to display */
	tc_write16(tc, TC_GPIOOUT, 0);
	tc->gp_reset_active = 1;
	gpiod_set_value(tc->gp_reset, 1);
}

static void tc_powerdown_c(struct tc358778_priv *tc)
{
	cancel_delayed_work_sync(&tc->tc_work);
	mutex_lock(&tc->power_mutex);
	if (tc->chip_enabled)
		tc_powerdown_locked(tc, 1);
	mutex_unlock(&tc->power_mutex);
}

static void tc_powerdown(struct tc358778_priv *tc)
{
	tc->pixel_clk_active = 0;
	tc_powerdown_c(tc);
}

static void tc_enable_gp(struct gpio_desc *gp_reset)
{
	if (!gp_reset)
		return;
	gpiod_set_value(gp_reset, 1);
	msleep(15);	/* disabled for at least 10 ms */
	gpiod_set_value(gp_reset, 0);
	msleep(2);
}

static void check_for_cmds(struct device_node *np, const char *dt_name, struct mipi_cmd *mc)
{
	void *data;
	int data_len;
	int ret;

	if (mc->mipi_cmds) {
		kfree(mc->mipi_cmds);
		mc->mipi_cmds = NULL;
	}
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
	mc->mipi_cmds = data;
	mc->length = data_len;
}

/*
 * continued fraction
 *      2  1  2  1  2
 * 0 1  2  3  8 11 30
 * 1 0  1  1  3  4 11
 */
static void rational_best_ratio_bigger(unsigned long *pnum, unsigned long *pdenom, unsigned max_n, unsigned max_d)
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

static void tc_setup_pll(struct tc358778_priv *tc, u32 pixelclock)
{
	int frs;
	unsigned long fbd, prd;
	u32 bitclk, bc;
	u64 temp;

	/*
	 * pll_clk(phy bitclk) = ((PCLK/4) * (FBD / PRD)) >> FRS
	 *
	 * FDB : 1 - 512
	 * PRD : 1 - 16
	 *
	 * pll_clk	FRS
	 *    500M-1G    : 0
	 *    250M-500M  : 1
	 *    125M-250M  : 2
	 *    62.5M-125M : 3
	 */
	bitclk = pixelclock * tc->dsi_bpp / tc->dsi_lanes;
	/* try for pll 1/8 more than what is needed */
//	bitclk += (bitclk >> 3);
	if (bitclk <= 125000000)
		frs = 3;
	else if (bitclk <= 250000000)
		frs = 2;
	else if (bitclk <= 500000000)
		frs = 1;
	else
		frs = 0;
	fbd = tc->dsi_bpp << (frs + 2);
	/* try for pll 1/8 more than what is needed */
//	fbd += (fbd >> 3);
	prd = tc->dsi_lanes;
	rational_best_ratio_bigger(&fbd, &prd, 512, 16);

	temp = pixelclock;
	temp *= fbd;
	do_div(temp, prd);
	bc = temp >> (frs + 2);

	pr_info("pixelclock=%d bitclk=%d %d fbd=%ld, prd=%ld, frs=%d\n", pixelclock,
		bitclk, bc, fbd, prd, frs);
	tc_write16(tc, TC_PLLCTL0, ((prd - 1) << 12) | (fbd - 1));
#define LBWS_50_PERCENT	2
	tc->pllctrl1 = (frs << 10) | (LBWS_50_PERCENT << 8) | (1 << 4) | (3 << 0);
	tc->byte_clock = bc >> 3;
	tc->bit_clock = bc;
}

static int tc_pll_init(struct tc358778_priv *tc)
{
	struct fb_info *fbi = find_registered_fb_from_dt(tc->disp_node);
	struct fb_var_screeninfo *var;
	u32 pixel_rate = 0;

	if (!fbi)
		return -EBUSY;
	var = &fbi->var;

	if (tc->pixel_clk)
		pixel_rate = clk_get_rate(tc->pixel_clk);
	if (!pixel_rate) {
		u64 pclk = 1000000000000ULL;

		if (!var->pixclock) {
			pr_info("pixclock=%d\n", var->pixclock);
			return -EAGAIN;
		}
		do_div(pclk, var->pixclock);
		pixel_rate = pclk;
	}
	pr_info("pixclock=%d pixel_rate=%d\n", var->pixclock, pixel_rate);
	tc->pixelclock = pixel_rate;
	tc_setup_pll(tc, tc->pixelclock);
	return 0;
}

static int tc_powerup1(struct tc358778_priv *tc)
{
	unsigned id = (tc->dsi_bpp == 24) ? MIPI_DSI_PACKED_PIXEL_STREAM_24 : (tc->dsi_bpp == 18) ?
			MIPI_DSI_PACKED_PIXEL_STREAM_18 : MIPI_DSI_PACKED_PIXEL_STREAM_16;
	u16 datafmt = (tc->dsi_bpp == 24) ? 3 << 4 : (tc->dsi_bpp == 18) ?
			4 << 4 : 5 << 4;
	u32 ctrl;
	u32 regen;
	unsigned cnt, cnt2, lptxtimecnt;
	int ret;

	tc_enable_gp(tc->gp_reset);
	tc->gp_reset_active = 0;
	tc_write16(tc, TC_CONFCTL, 4);
	tc_write16(tc, TC_SYSCTL, 1);
	tc_write16(tc, TC_SYSCTL, 0);
	set_speed_lpm_mode(tc, true);
	msleep(2);

	/* Enable PLL */
	ret = tc_pll_init(tc);
	if (ret)
		return ret;
	tc_write16(tc, TC_PLLCTL1, tc->pllctrl1 & ~0x10);
	msleep(3);
	tc_write16(tc, TC_PLLCTL1, tc->pllctrl1);

	tc_write16(tc, TC_VSDLY, 0x08);
#if 1
	tc->datafmt = datafmt | 1;
#else
	tc->datafmt = datafmt | 5;
#endif
	tc_write16(tc, TC_DATAFMT, tc->datafmt);
	tc_write16(tc, TC_DSITX_DT, DCS_ID(id));
	/* This is for GPIO3-10 (Parallel port input 16-23 */
	tc_write16(tc, TC_GPIOEN, 0);
	/*
	 * gpio1 is output, to reset the display
	 * gpio2 is output, standby
	 */
	tc_write16(tc, TC_GPIODIR, 0xfff9);
	/* Pulse reset pin of mipi display */
	tc_write16(tc, TC_GPIOOUT, 0);
	msleep(2);
	tc_write16(tc, TC_GPIOOUT, 2);
	msleep(1);
	tc_write16(tc, TC_GPIOOUT, 6);
	msleep(1);

	/* Enable D-PHY */
	ctrl = tc->dsi_lanes ? (tc->dsi_lanes - 1) << 1 : 0;
	if (tc->skip_eot)
		ctrl |= 1;

	tc_write32(tc, TC_CLW_CNTRL, 0x0);
	tc_write32(tc, TC_D0W_CNTRL, 0x0);
	tc_write32(tc, TC_D1W_CNTRL, 0x0);
	tc_write32(tc, TC_D2W_CNTRL, 0x0);
	tc_write32(tc, TC_D3W_CNTRL, 0x0);
	tc_write32(tc, TC_DSI_START, 1);
	/* clear bits */
	tc_write32(tc, TC_DSI_CONFW, CLR_DSI_CONTROL | (ctrl ^ 7) | 0x8000);
	/* set bits */
	tc_write32(tc, TC_DSI_CONFW, SET_DSI_CONTROL | ctrl | 0x100);
	/* ppi setup */
#if 0
	tc_write32(tc, TC_LINEINITCNT, 0x2c88);
	tc_write32(tc, TC_LPTXTIMECNT, 0x05);
	tc_write32(tc, TC_TCLK_HEADERCNT, 0x1f06);
	tc_write32(tc, TC_TCLK_TRAILCNT, 0x03);
	tc_write32(tc, TC_THS_HEADERCNT, 0x606);
	tc_write32(tc, TC_TWAKEUP, 0x4a88);
	tc_write32(tc, TC_TCLK_POSTCNT, 0x0b);
	tc_write32(tc, TC_THS_TRAILCNT, 0x04);
#else
	/*
	 * byte_clock / (lineinitcnt + 1) must be < 10 KHz( >100us period)
	 * lineinitcnt = byte_clock / 10 KHz
	 */
#define MICROSEC	1000000
#define NANOSEC		1000000000
#define LP11_STATE	(MICROSEC/120)	/* 120 us to give cushion */
	cnt = tc->byte_clock / LP11_STATE;
	tc_write32(tc, TC_LINEINITCNT, cnt);
	/*
	 * (lptxtimecnt + 1) / byte_clock > 50ns
	 * byte_clock / (lptxtimecnt + 1) must be < 20 MHz( >50ns period)
	 * lptxtimecnt > byte_clock / 20 MHz
	 */
#define LPTX_FREQ	(NANOSEC/60)	/* 60 ns to give cushion */
	lptxtimecnt = tc->byte_clock / LPTX_FREQ;
	tc_write32(tc, TC_LPTXTIMECNT, lptxtimecnt);
	/*
	 * byte_clock / (zerocnt + 1) must be < 3.33 MHz( >300ns period)
	 * zerocnt = byte_clock / 3.33 MHz
	 *
	 * byte_clock / (preparecnt + 1) must be < 26.3 MHz( >38ns period)
	 * preparecnt = byte_clock / 26.31 MHz
	 */
#define ZERO_FREQ	(NANOSEC/350)	/* 350 ns to give cushion */
	cnt = tc->byte_clock / ZERO_FREQ;
#define PREPARE_FREQ	(NANOSEC/60)	/* 60 ns to give cushion */
	cnt2 = tc->byte_clock / PREPARE_FREQ;
	tc_write32(tc, TC_TCLK_HEADERCNT, (cnt << 8) | cnt2);

	/*
	 * (trailcnt + 5) / byte_clock - (2.5/ bit_clock) > 60ns
	 * (trailcnt + 5) / (bit_clock/8) - (2.5/ bit_clock)) > 60ns
	 * (trailcnt + 5) / (1/8) - 2.5 > 60ns * bit_clock
	 * 8(trailcnt + 5) - 2.5 > 60ns * bit_clock
	 * (trailcnt + 5) - 5/16 > (60/8)ns * bit_clock
	 * trailcnt > (60/8000000000) * bit_clock - 5 + 5/16
	 * trailcnt > bit_clock/ (8000000000/60) - 5 + 5/16
	 * trailcnt > (bit_clock / (1000000000/(60*2)) - 80 + 5)/ 16
	 * trailcnt > (bit_clock / (1000000000/(60*2)) - 75) / 16
	 */
#define TRAIL_FREQ16	(NANOSEC/(70*2))	/* 70 ns to give cushion */
	cnt = tc->bit_clock / TRAIL_FREQ16 + 1;
	if (cnt >= 75)
		cnt -= 75;
	else
		cnt = 0;
	cnt += 15;
	cnt >>= 4;
	tc_write32(tc, TC_TCLK_TRAILCNT, cnt);

	/*
	 * (zerocnt + 11) / byte_clock + (1/ bit_clock) > 145ns
	 * (zerocnt + 11) / (bit_clock/8) - (1/ bit_clock)) > 145ns
	 * (zerocnt + 11) / (1/8) - 1 > 145ns * bit_clock
	 * 8(zerocnt + 11) - 1 > 145ns * bit_clock
	 * (zerocnt + 11) - 1/8 > (145/8)ns * bit_clock
	 * zerocnt > (145/8)ns * bit_clock - 11 + 1/8
	 * zerocnt > bit_clock/ (8000000000/145) - 11 + 1/8
	 * zerocnt > (bit_clock / (1000000000/145) - 88 + 1)/ 8
	 * zerocnt > (bit_clock / 6.896 MHz - 87) / 8
	 */
#define ZERO_FREQ8	(NANOSEC/160)	/* 160 ns to give cushion */
	cnt = tc->bit_clock / ZERO_FREQ8 + 1;
	if (cnt >= 87)
		cnt -= 87;
	else
		cnt = 0;
	cnt += 7;
	cnt >>= 3;
	/*
	 * (preparecnt + 1) / byte_clock - (4/ bit_clock) > 40ns
	 * (preparecnt + 1) / (bit_clock/8) - (4/ bit_clock)) > 40ns
	 * (preparecnt + 1) / (1/8) - 4 > 40ns * bit_clock
	 * 8(preparecnt + 1) - 4 > 40ns * bit_clock
	 * (preparecnt + 1) - 1/2 > (40/8)ns * bit_clock
	 * preparecnt > (40/8)ns * bit_clock - 1/2
	 * preparecnt > bit_clock/ (8000000000/40) - 1/2
	 * preparecnt > (bit_clock / (4000000000/40) - 1)/ 2
	 */
#define PREPARE_FREQ2	(NANOSEC /(60/4))	/* 60 ns to give cushion */
	cnt2 = tc->bit_clock / PREPARE_FREQ2;
	cnt2 += 1;
	cnt2 >>= 1;
	tc_write32(tc, TC_THS_HEADERCNT, (cnt << 8) | cnt2);

	/*
	 * (wakeupcnt + 1) * (lptxtimecnt + 1) / byte_clock  > 1ms
	 * (wakeupcnt + 1) > 1ms * byte_clock / (lptxtimecnt + 1)
	 * wakeupcnt > (1/1000) * byte_clock / (lptxtimecnt + 1) -1
	 * wakeupcnt > byte_clock / (1000/1 * (lptxtimecnt + 1)) - 1
	 */
#define WAKE_FREQ	(MICROSEC /1100)	/* 1100 us to give cushion */
	cnt = tc->byte_clock / ((lptxtimecnt + 1) * WAKE_FREQ);
	tc_write32(tc, TC_TWAKEUP, cnt);

	/*
	 * (postcnt + 3.5) / byte_clock + (2.5 / bit_clock) > 60ns + (52/bit_clock)
	 * (postcnt + 3.5) / byte_clock - (49.5 / bit_clock) > 60ns
	 * (postcnt + 3.5) / (bit_clock/8) - (49.5/ bit_clock)) > 60ns
	 * (postcnt + 3.5) / (1/8) - 49.5 > 60ns * bit_clock
	 * 8(postcnt + 3.5) - 49.5 > 60ns * bit_clock
	 * (postcnt + 3.5) - 99/16 > (60/8)ns * bit_clock
	 * (postcnt + 56/16) - 99/16 > (60/8)ns * bit_clock
	 * postcnt - 43/16 > (60/8)ns * bit_clock
	 * postcnt > (60/8)ns * bit_clock + 43/16
	 * postcnt > bit_clock/ (8000000000/60) + 43/16
	 * postcnt > (bit_clock / (1000000000/(60*2)) + 43) / 16
	 */
#define POST_FREQ16	(NANOSEC / (70*2))	/* 70 ns to give cushion */
	cnt = tc->bit_clock / POST_FREQ16 + 1 + 43 + 15;
	cnt >>= 4;
	tc_write32(tc, TC_TCLK_POSTCNT, cnt);

	/*
	 * (trailcnt + 5) / byte_clock + (11 / bit_clock) > 70ns
	 * (trailcnt + 5) / (bit_clock/8) - (11/ bit_clock)) > 70ns
	 * (trailcnt + 5) / (1/8) - 11 > 70ns * bit_clock
	 * 8(trailcnt + 5) - 11 > 70ns * bit_clock
	 * (trailcnt + 5) - 11/8 > (70/8)ns * bit_clock
	 * (trailcnt + 40/8) - 11/8 > (70/8)ns * bit_clock
	 * trailcnt + 29/8 > (70/8)ns * bit_clock
	 * trailcnt > (70/8)ns * bit_clock - 29/8
	 * trailcnt > bit_clock/ (8000000000/70) - 29/8
	 * trailcnt > (bit_clock / (1000000000/70) - 29)/ 8
	 */
#define TRAIL_FREQ8	(NANOSEC / 80)	/* 80 ns to give cushion */
	cnt = tc->bit_clock / TRAIL_FREQ8 + 1;
	if (cnt >= 29)
		cnt -= 29;
	else
		cnt = 0;
	cnt += 7;
	cnt >>= 3;
	tc_write32(tc, TC_THS_TRAILCNT, cnt);
#endif
	regen = (tc->dsi_lanes >= 4) ? 0x1f :
		(tc->dsi_lanes >= 3) ? 0x0f :
		(tc->dsi_lanes >= 2) ? 0x07 :
		(tc->dsi_lanes >= 1) ? 0x03 : 0;
	tc_write32(tc, TC_HSTXVREGEN, regen);

	/*
	 * (txtagocnt + 1) * 4 / byte_clock >= 4 * (lptxtimecnt + 1) / byte_clock
	 * (txtagocnt + 1) * 4 >= 4 * (lptxtimecnt + 1)
	 * (txtagocnt + 1) >= (lptxtimecnt + 1)
	 * txtagocnt >= lptxtimecnt
	 */
	/*
	 * (rxtasurecnt + 3) / byte_clock > (lptxtimecnt + 1) / byte_clock
	 * (rxtasurecnt + 3) > (lptxtimecnt + 1)
	 * rxtasurecnt > lptxtimecnt - 2
	 */
	cnt2 = (lptxtimecnt > 1) ? (lptxtimecnt - 1) : 0;
	tc_write32(tc, TC_BTACNTRL1, (lptxtimecnt << 16) | cnt2);
	/* Set dsi continuous clock */
	tc_write32(tc, TC_TXOPTIONCNTRL, 1);
	tc_write32(tc, TC_STARTCNTRL, 1);
	return 0;
}

static void check_mipi_cmds_from_dtb(struct tc358778_priv *tc, struct device_node *np)
{
	struct device_node *cmds_np = of_parse_phandle(np, "mipi-cmds", 0);
	struct device_node *np1 = cmds_np;
	struct device_node *npd = NULL;
	unsigned char buf[16];
	int i;
	int ret;

	for (i = 0; i < 100; i++) {
		if (i == 0) {
			np1 = cmds_np;
			if (!np1)
				continue;
		} else {
			snprintf(buf, sizeof(buf), "mipi-cmds-%d", i);
			np1 = of_parse_phandle(np, buf, 0);
			if (!np1)
				break;
			if (np1 == cmds_np)
				continue;
		}
		if (!npd)
			npd = np1;
		check_for_cmds(np1, "mipi-cmds-detect", &tc->mipi_cmds_detect);
		if (!tc->mipi_cmds_detect.length)
			continue;
		set_speed_lpm_mode(tc, true);
		ret = send_mipi_cmd_list(tc, &tc->mipi_cmds_detect);
		if (!ret) {
			npd = np1;
			break;
		}
		/* Now reset the panel because commands were not for it.*/
		tc_powerdown(tc);
		msleep(150);
		ret = tc_powerup1(tc);
		if (ret) {
			pr_err("%s: tc_powerup1 failed ret=%d", __func__, ret);
			return;
		}
		msleep(150);

	}
	if (npd) {
		const char *pname = NULL;

		of_property_read_string(npd, "name", &pname);
		if (pname)
			pr_info("%s: mipi cmds used: %s\n", __func__, pname);
		check_for_cmds(npd, "mipi-cmds-init", &tc->mipi_cmds_init);
		check_for_cmds(npd, "mipi-cmds-enable", &tc->mipi_cmds_enable);
		check_for_cmds(npd, "mipi-cmds-disable", &tc->mipi_cmds_disable);
	}
}

static unsigned pix_to_bytecnt(struct tc358778_priv *tc, unsigned pix)
{
	unsigned groups = ((pix + tc->dsi_lanes - 1) / tc->dsi_lanes);

	return ((groups * tc->dsi_bpp + 7) / 8) * tc->dsi_lanes;
}

static unsigned pix_to_delay_byte_clocks(struct tc358778_priv *tc, unsigned pix)
{
	u64 delay = pix;

	delay *= tc->byte_clock;
	delay += (tc->pixelclock >> 1);
	do_div(delay, tc->pixelclock);

	delay = (delay + 2) & ~3;
	return delay;
}

static int tc_setup_regs(struct tc358778_priv *tc)
{
	struct fb_info *fbi = find_registered_fb_from_dt(tc->disp_node);
	struct fb_var_screeninfo *var;
	unsigned conf, hsw, hsw_hbpr;
	u64 temp;
	u32 t1, vsdelay;
	u32 xres, left_margin, right_margin;

	if (!fbi)
		return -EBUSY;
	var = &fbi->var;

	left_margin = var->left_margin;
	right_margin = var->right_margin;
	xres = var->xres;

	pr_info("%s: %dx%d margins= %d,%d %d,%d  syncs=%d %d\n",
		__func__, xres, var->yres,
		left_margin, right_margin,
		var->upper_margin, var->lower_margin,
		var->hsync_len, var->vsync_len);

	tc_write16(tc, TC_PP_MISC,
		(fbi->var.sync & FB_SYNC_HOR_HIGH_ACT) ? 1 : 0);
	conf = (1 << 6) | 4;
	if (fbi->var.sync & FB_SYNC_VERT_HIGH_ACT)
		conf |= (1 << 5);
	if (fbi->var.sync & FB_SYNC_OE_LOW_ACT)
		conf |= (1 << 4);
	tc_write16(tc, TC_CONFCTL, conf);

	tc_write16(tc, TC_DSI_EVENT, tc->pulse ? 0 : 1);
	tc_write16(tc, TC_DSI_VSW, var->vsync_len +
		(tc->pulse ? 0 : var->upper_margin));
	/* vback_porch used in Pulse var only */
	tc_write16(tc, TC_DSI_VBPR, var->upper_margin);
	tc_write16(tc, TC_DSI_VACT, var->yres);

	hsw = pix_to_delay_byte_clocks(tc, var->hsync_len);
	hsw_hbpr = pix_to_delay_byte_clocks(tc, var->hsync_len + left_margin);
#if 1
	tc_write16(tc, TC_DSI_HSW, tc->pulse ? hsw : hsw_hbpr);
#else
	tc_write16(tc, TC_DSI_HSW, tc->pulse ? hsw : 0x228);
#endif
	/* hback_porch used in Pulse var only */
	if (tc->pulse) {
		u32 hbpr = hsw_hbpr - hsw;

		if (hbpr < 2)
			hbpr = 2;
		tc_write16(tc, TC_DSI_HBPR, hbpr);
	}
	tc_write16(tc, TC_DSI_HACT, pix_to_bytecnt(tc, xres));

	/*
	 *  delay_time + output_time > input_time
	 *  delay_time = (vsdelay + 40 + hsw_hbpr)/byte_clock
	 *  output_time = (HACT*bpp/8/byte_clock/lanes)
	 *  input_time = (HSW+HBP+HACT)/pixelclock
	 *
	 *  (vsdelay + 40 + hsw_hbpr)/byte_clock + (HACT*bpp/8/byte_clock/lanes) > input_time
	 *  (vsdelay + 40 + hsw_hbpr) + (HACT*bpp/8/lanes) > input_time * byte_clock
	 *  vsdelay  > input_time * byte_clock - (HACT*bpp/8/lanes) - 40 - hsw_hbpr
	 *  vsdelay  > (HSW+HBP+HACT)/pixelclock * byte_clock - (HACT*bpp/8/lanes) - 40 - hsw_hbpr
	 *  vsdelay  > (HSW+HBP+HACT)/pixelclock * byte_clock - (((HACT*bpp/8)/lanes) + 40 + hsw_hbpr)
	 *
	 */
	temp = var->hsync_len + left_margin + xres;
	temp *= tc->byte_clock;
	do_div(temp, tc->pixelclock);
	t1 = (xres * ((tc->dsi_bpp + 7) / 8)) / tc->dsi_lanes + 40 + hsw_hbpr;
	vsdelay = temp;
	if (vsdelay > t1)
		vsdelay -= t1;
	else
		vsdelay = 0;
	vsdelay += 8;
	if (vsdelay > 1023)
		vsdelay = 1023;
	tc_write16(tc, TC_VSDLY, vsdelay);
	return 0;
}

static int tc_powerup_locked(struct tc358778_priv *tc)
{
	int ret = tc_powerup1(tc);
	unsigned id = (tc->dsi_bpp == 24) ? MIPI_DSI_PACKED_PIXEL_STREAM_24 : (tc->dsi_bpp == 18) ?
			MIPI_DSI_PACKED_PIXEL_STREAM_18 : MIPI_DSI_PACKED_PIXEL_STREAM_16;

	if (ret)
		return ret;

	if (!tc->mipi_cmds_init.length)
		check_mipi_cmds_from_dtb(tc, tc->of_node);

	tc_setup_regs(tc);

	set_speed_lpm_mode(tc, true);
	ret = send_mipi_cmd_list(tc, &tc->mipi_cmds_init);
	if (ret < 0)
		return ret;

	check_for_halt(tc, 'a');
	/* Send exit sleep */
	ret = send_mipi_cmd_list(tc, &tc->mipi_cmds_enable);
	if (ret < 0)
		return ret;

	check_for_halt(tc, 'b');
	set_speed_lpm_mode(tc, false);
	check_for_halt(tc, 'c');

	tc_write16(tc, TC_DSITX_DT, DCS_ID(id));
	if (!(tc->datafmt & 2)) {
		tc->datafmt |= 2;
		tc_write16(tc, TC_DATAFMT, tc->datafmt);
	}
	check_for_halt(tc, 'd');

	/*
	 * Enable Parallel Input
	 * 1. Clear RstPtr and FrmStop
	 * 2. Set PP_En
	 */
	tc_update16(tc, TC_PP_MISC, BIT(14) |BIT(15), 0);
	tc_update16(tc, TC_CONFCTL, 0, BIT(6));

	if (tc->client->irq) {
		enable_irq(tc->client->irq);
		/* write TC_DSI_INT_ENA */
		tc_write32(tc, TC_DSI_CONFW, SET_DSI_INT_ENA | 0x000f);
	}
	return 0;
}

static int tc_powerup_c(struct tc358778_priv *tc)
{
	int ret = 0;

	mutex_lock(&tc->power_mutex);
	if (!tc->chip_enabled) {
		ret = tc_powerup_locked(tc);
		if (!ret)
			tc->chip_enabled = 1;
	}
	mutex_unlock(&tc->power_mutex);
	tc->halt_check_cnt = 0;
	queue_delayed_work(tc->tc_workqueue, &tc->tc_work,
		msecs_to_jiffies(HALT_CHECK_DELAY));
	return ret;
}

static int tc_powerup(struct tc358778_priv *tc)
{
	tc->pixel_clk_active = 1;
	return tc_powerup_c(tc);
}

static int tc_drm_event(struct notifier_block *nb, unsigned long event, void *data)
{
	struct drm_device *drm_dev = data;
	struct device_node *node = drm_dev->dev->of_node;
	struct tc358778_priv *tc = container_of(nb, struct tc358778_priv, drmnb);
	struct device *dev = &tc->client->dev;

	dev_dbg(dev, "%s: event %lx\n", __func__, event);

	if (node != tc->disp_node)
		return 0;

	switch (event) {
	case DRM_MODE_DPMS_ON:
		tc_powerup(tc);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		tc_powerdown(tc);
		break;
	default:
		dev_info(dev, "%s: unknown event %lx\n", __func__, event);
	}

	return 0;
}

static int tc_fb_event(struct notifier_block *nb, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct fb_info *info = evdata->info;
	struct device_node *node = info->device->of_node;
	struct tc358778_priv *tc = container_of(nb, struct tc358778_priv, fbnb);
	struct device *dev;
	int blank_type;

	dev = &tc->client->dev;
	dev_dbg(dev, "%s: event %lx\n", __func__, event);
	if (node != tc->disp_node)
		return 0;

	switch (event) {
	case FB_EVENT_BLANK: {
		blank_type = *((int *)evdata->data);
		if (blank_type == FB_BLANK_UNBLANK) {
			tc_powerup(tc);
		} else {
			tc_powerdown(tc);
		}
		dev_info(dev, "%s: blank type 0x%x\n", __func__, blank_type );
		break;
	}
#ifdef CONFIG_FB_EXTENDED_EVENTS
	case FB_EVENT_SUSPEND: {
		dev_info(dev, "%s: suspend\n", __func__ );
		tc_powerdown(tc);
		break;
	}
	case FB_EVENT_RESUME: {
		dev_info(dev, "%s: resume\n", __func__ );
		break;
	}
	case FB_EVENT_FB_REGISTERED: {
		break;
	}
#endif
	case FB_EVENT_MODE_CHANGE: {
		/* panel resolution should not really be changing for an LCD panel */
		break;
	}
	default:
		dev_info(dev, "%s: unknown event %lx\n", __func__, event);
	}
	return 0;
}

static int tc_check_halt(struct tc358778_priv *tc)
{
	int status = 1;
	int ret = 0;

	mutex_lock(&tc->power_mutex);
	if (tc->pixel_clk_active) {
		if (!tc->gp_reset_active)
			tc_read32(tc, TC_DSI_STATUS, &status);
		if (status & 3) {
			/* halted */
			struct device *dev = &tc->client->dev;
			int dsi_err = 0;

			if (!tc->gp_reset_active) {
				tc_read32(tc, TC_DSI_ERR, &dsi_err);
				dev_info(dev, "%s: dsi_err=%x, status=%x\n",
					__func__, dsi_err, status);
			}
			tc_powerdown_locked(tc, 0);
			ret = tc_powerup_locked(tc);
			if (ret) {
				dev_info(dev, "%s: powerup failed %d\n",
					__func__, ret);
			} else {
				tc->chip_enabled = 1;
			}
			ret = 1;
			tc->halt_check_cnt = 0;
		}
	}
	mutex_unlock(&tc->power_mutex);
	return ret;
}

/*
 * We only report errors in this handler
 */
static irqreturn_t tc_irq_handler(int irq, void *id)
{
	struct tc358778_priv *tc = id;
	struct device *dev = &tc->client->dev;
	int dsi_int = tc_read16(tc, TC_DSI_INT);

	if (dsi_int > 0) {

		dev_info(dev, "%s: dsi_int %x\n", __func__,
			dsi_int);
		if (dsi_int & 2) {
			int dsi_rxerr = tc_read16(tc, TC_DSI_RXERR);

			dev_info(dev, "%s: dsi_rxerr %x\n", __func__,
				dsi_rxerr);
		}
		if (dsi_int & 1) {
			int dsi_ackerr = tc_read16(tc, TC_DSI_ACKERR);

			dev_info(dev, "%s: dsi_ackerr %x\n", __func__,
				dsi_ackerr);
		}
		tc_write16(tc, TC_DSI_INT_CLR, dsi_int);
		if (dsi_int & 7) {
			tc_check_halt(tc);
		}
		if (tc->int_cnt++ > 10) {
			disable_irq_nosync(tc->client->irq);
			dev_err(dev, "%s: irq disabled\n", __func__);
		} else {
			msleep(100);
		}
		return IRQ_HANDLED;
	} else {
		dev_err(dev, "%s: read error %d\n", __func__, dsi_int);
	}
	return IRQ_NONE;
}

static void tc_work_func(struct work_struct *work)
{
	struct tc358778_priv *tc = container_of(to_delayed_work(work),
			struct tc358778_priv, tc_work);
	int ret;

	if (!tc->pixel_clk_active)
		return;
	ret = tc_check_halt(tc);
	if (!ret)
		tc->halt_check_cnt++;
	if (tc->halt_check_cnt < HALT_CHECK_CNT)
		queue_delayed_work(tc->tc_workqueue, &tc->tc_work,
				msecs_to_jiffies(HALT_CHECK_DELAY));
}


static int find_size(u16 reg)
{
	const u16 *p = registers_to_show;
	u32 r;
	int i;

	for (i = 0; i < ARRAY_SIZE(registers_to_show); i++) {
		r = *p++;
		if (!((r ^ reg) & ~R32))
			return (r >= R32) ? 1 : 0;
	}
	return -EINVAL;
}

static ssize_t tc358778_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tc358778_priv *tc = dev_get_drvdata(dev);
	int ret;
	const u16 *p = registers_to_show;
	int i = 0;
	int total = 0;
	int reg;
	int cnt;
	u32 result32 = 0;

	if (!tc->pixel_clk_active)
		return -EBUSY;
	if (tc->show_reg != 0) {
		int s32 = find_size(tc->show_reg);

		if (s32 < 0)
			return s32;
		if (s32) {
			ret = tc_read32(tc, tc->show_reg | R32, &result32);
			return sprintf(buf, "%04x: %08x (%d)\n", tc->show_reg,
					result32, ret);
		}
		ret = tc_read16(tc, tc->show_reg);
		return sprintf(buf, "%04x: %04x (%d)\n", tc->show_reg,
				(ret >= 0) ? ret : 0, ret);
	}

	while (i < ARRAY_SIZE(registers_to_show)) {
		reg = *p++;
		i++;
		/* don't read from fifo when showing all regs*/
		if (reg == TC_DSICMD_RXFIFO)
			continue;
		if (reg >= R32) {
			result32 = 0;
			ret = tc_read32(tc, reg, &result32);
			cnt = sprintf(&buf[total], "%04x: %08x (%d)\n",
					reg & ~R32, result32, ret);
		} else {
			ret = tc_read16(tc, reg);
			cnt = sprintf(&buf[total], "%04x: %04x (%d)\n",
					reg, (ret >= 0) ? ret : 0, ret);
		}
		if (cnt <= 0)
			break;
		total += cnt;
	}
	return total;
}

static ssize_t tc358778_reg_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned val;
	int ret;
	struct tc358778_priv *tc = dev_get_drvdata(dev);
	char *endp;
	unsigned reg = simple_strtol(buf, &endp, 16);
	int s32 = find_size(reg);

	if (s32 < 0)
		return count;

	if (!tc->pixel_clk_active)
		return -EBUSY;

	tc->show_reg = reg;
	if (!endp)
		return count;
	if (*endp == 0x20)
		endp++;
	if (!*endp || *endp == 0x0a)
		return count;
	val = simple_strtol(endp, &endp, 16);
	if (!s32 && (val >= 0x10000))
		return count;

	dev_err(dev, "%s:reg=0x%x, val=0x%x\n", __func__, reg, val);
	ret = s32 ? tc_write32(tc, reg | R32, val) : tc_write16(tc, reg, (u16)val);
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(tc358778_reg, 0644, tc358778_reg_show, tc358778_reg_store);

static ssize_t tc358778_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tc358778_priv *tc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", tc->chip_enabled);
}

static ssize_t tc358778_enable_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct tc358778_priv *tc = dev_get_drvdata(dev);
	char *endp;
	unsigned enable;

	if (!tc->pixel_clk_active)
		return -EBUSY;

	enable = simple_strtol(buf, &endp, 16);
	if (enable) {
		tc_powerup_c(tc);
	} else {
		tc_powerdown_c(tc);
	}


	return count;
}

static DEVICE_ATTR(tc358778_enable, 0644, tc358778_enable_show, tc358778_enable_store);

/*
 * I2C init/probing/exit functions
 */
static int tc358778_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int ret;
	struct tc358778_priv *tc;
	struct i2c_adapter *adapter;
        struct device_node *np = client->dev.of_node;
	struct gpio_desc *gp_reset;
	const char *df;
	u32 dsi_lanes;
	struct fb_info *fbi;

	adapter = to_i2c_adapter(client->dev.parent);

	ret = i2c_check_functionality(adapter, I2C_FUNC_I2C);
	if (!ret) {
		dev_err(&client->dev, "i2c_check_functionality failed\n");
		ret = -ENODEV;
		goto exit1;
	}

	gp_reset = devm_gpiod_get_optional(&client->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(gp_reset)) {
		if (PTR_ERR(gp_reset) != -EPROBE_DEFER)
			dev_err(&client->dev, "Failed to get reset gpio: %ld\n",
				PTR_ERR(gp_reset));
		ret = PTR_ERR(gp_reset);
		goto exit1;
	}
	if (gp_reset) {
		tc_enable_gp(gp_reset);
	} else {
		dev_warn(&client->dev, "no enable pin available");
	}
	tc = devm_kzalloc(&client->dev, sizeof(*tc), GFP_KERNEL);
	if (!tc) {
		ret = -ENOMEM;
		goto exit1;
	}
	tc->client = client;
	tc->gp_reset = gp_reset;
	tc->of_node = np;
	mutex_init(&tc->power_mutex);
	INIT_DELAYED_WORK(&tc->tc_work, tc_work_func);
	tc->tc_workqueue = create_workqueue("tc358778_wq");
	if (!tc->tc_workqueue) {
		pr_err("Failed to create tc work queue");
		goto exit1;
	}

	tc->disp_node = of_parse_phandle(np, "display", 0);
	if (!tc->disp_node) {
		ret = -ENODEV;
		goto exit1;
	}

	fbi = find_registered_fb_from_dt(tc->disp_node);
	if (!fbi) {
		ret = -EPROBE_DEFER;
		goto exit1;
	}

	ret = tc_read16(tc, TC_CHIPID);
	if (ret != 0x4401) {
		/* enable might be used for something else, change to input */
		gpiod_direction_input(gp_reset);
		dev_info(&client->dev, "i2c read failed, %x(%d)\n",
				(ret >= 0) ? ret : 0, ret);
		ret = -ENODEV;
		goto exit1;
	}

	if (of_property_read_u32(np, "dsi-lanes", &dsi_lanes) < 0) {
		ret = -EINVAL;
		goto exit1;
	}
	if (dsi_lanes < 1 || dsi_lanes > 4) {
		ret = -EINVAL;
		goto exit1;
	}
	tc->dsi_lanes = dsi_lanes;
	if (of_property_read_bool(np, "mode-skip-eot"))
		tc->skip_eot = 1;
	if (of_property_read_bool(np, "mode-video-sync-pulse"))
		tc->pulse |= 1;
	ret = of_property_read_string(np, "dsi-format", &df);
	if (ret) {
		dev_err(&client->dev, "dsi-format missing %d\n", ret);
		goto exit1;
	}
	if (of_property_read_bool(np, "mode-video-burst"))
		tc->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
	tc->dsi_bpp = !strcmp(df, "rgb666") ? 18 : 24;
	tc->pixel_clk = devm_clk_get(&client->dev, "pixel");
	if (IS_ERR(tc->pixel_clk))
		tc->pixel_clk = NULL;

	if (client->irq) {
		irq_set_status_flags(client->irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(&client->dev, client->irq,
			NULL, tc_irq_handler,
			IRQF_ONESHOT, client->name, tc);
		if (ret) {
			pr_info("%s: request_irq failed, irq:%i\n", client_name, client->irq);
			goto exit1;
		}
	}
	i2c_set_clientdata(client, tc);
	tc->drmnb.notifier_call = tc_drm_event;
	ret = drm_register_client(&tc->drmnb);
	if (ret < 0) {
		dev_err(&client->dev, "drm_register_client failed(%d)\n", ret);
		goto exit1;
	}
	tc->fbnb.notifier_call = tc_fb_event;
	ret = fb_register_client(&tc->fbnb);
	if (ret < 0) {
		dev_err(&client->dev, "fb_register_client failed(%d)\n", ret);
		goto exit1;
	}
	ret = device_create_file(&client->dev, &dev_attr_tc358778_enable);
	ret = device_create_file(&client->dev, &dev_attr_tc358778_reg);
	if (ret < 0)
		pr_warn("failed to add tc358778 sysfs files\n");

	tc_powerup(tc);
	dev_info(&client->dev, "succeeded\n");
	return 0;
exit1:
	if (client->irq)
		irq_set_irq_type(client->irq, IRQ_TYPE_NONE);
	return ret;
}

static int tc358778_remove(struct i2c_client *client)
{
	struct tc358778_priv *tc = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&tc->tc_work);
	destroy_workqueue(tc->tc_workqueue);
	device_remove_file(&client->dev, &dev_attr_tc358778_reg);
	device_remove_file(&client->dev, &dev_attr_tc358778_enable);
	fb_unregister_client(&tc->drmnb);
	fb_unregister_client(&tc->fbnb);
	tc_powerdown(tc);
	return 0;
}

static void tc358778_shutdown(struct i2c_client *client)
{
	tc358778_remove(client);
}

static const struct i2c_device_id tc358778_id[] = {
	{"tc358778", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, tc358778_id);

static const struct of_device_id tc358778_of_match[] = {
	{
		.compatible = "tc358778",
		.data = NULL
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, tc358778_of_match);

static struct i2c_driver tc358778_driver = {
	.driver = {
		.name = "tc358778",
		.of_match_table = tc358778_of_match,
		.owner = THIS_MODULE,
	},
	.probe = tc358778_probe,
	.remove = tc358778_remove,
	.shutdown = tc358778_shutdown,
	.id_table = tc358778_id,
};

module_i2c_driver(tc358778_driver);

MODULE_AUTHOR("Boundary Devices, Inc.");
MODULE_DESCRIPTION("tc358778 rgb to mipi bridge");
MODULE_LICENSE("GPL");
