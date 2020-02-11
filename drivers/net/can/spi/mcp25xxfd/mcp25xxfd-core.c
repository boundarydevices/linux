// SPDX-License-Identifier: GPL-2.0
//
// mcp25xxfd - Microchip MCP25xxFD Family CAN controller driver
//
// Copyright (c) 2019, 2020 Pengutronix,
//                          Marc Kleine-Budde <kernel@pengutronix.de>
//
// Based on:
//
// CAN bus driver for Microchip 25XXFD CAN Controller with SPI Interface
//
// Copyright (c) 2019 Martin Sperl <kernel@martin.sperl.org>
//

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>

#include <asm/unaligned.h>

#include "mcp25xxfd.h"

#define DEVICE_NAME "mcp25xxfd"

#define MCP25XXFD_SYSCLOCK_HZ_MAX 40000000
#define MCP25XXFD_SYSCLOCK_HZ_MIN 1000000
#define MCP25XXFD_SPICLOCK_HZ_MAX 20000000
#define MCP25XXFD_OSC_PLL_MULTIPLIER 10
#define MCP25XXFD_OSC_DELAY_MS 3

/* DS80000792B - MCP2517FD Errata
 *
 * Incorrect CRC for certain READ_CRC commands
 *
 * It is possible that there is a mismatch between the transmitted CRC
 * and the actual CRC for the transmitted data when data is updated at
 * a specific time during the SPI READ_CRC command. In these cases the
 * transmitted CRC is wrong. The data transmitted is correct.
 *
 * Fix/Work Around:
 *
 * If a CRC mismatch occurs, reissue the READ_CRC command. Only bits
 * 7/15/23/31 of the following registers can be affected:
 *
 * - CiTXIF		(*)
 * - CiRXIF		(*)
 * - CiCON
 * - CiTBC
 * - CiINT
 * - CiRXOVIF		(*)
 * - CiTXATIF		(*)
 * - CiTXREQ		(*)
 * - CiTREC
 * - CiBDIAG0
 * - CiBDIAG1
 * - CiTXQSTA
 * - CiFIFOSTAm
 *
 * The occurrence can be minimized by not using FIFOs 7/15/23/31. In
 * these cases, the registers CiTXIF, CiRXIF, CiRXOVIF, CiTXATIF and
 * CiTXREQ are not affected.
 *
 * Bit 31 of RAM reads with CRC could also be affected. This can be
 * avoided by reading from a received FIFO only after the message has
 * been loaded into the FIFO, indicated by the receive flags. This is
 * the recommended procedure independent of the issue described here.
 */

static const struct can_bittiming_const mcp25xxfd_bittiming_const = {
	.name = DEVICE_NAME,
	.tseg1_min = 2,
	.tseg1_max = 256,
	.tseg2_min = 1,
	.tseg2_max = 128,
	.sjw_max = 128,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

static const struct can_bittiming_const mcp25xxfd_data_bittiming_const = {
	.name = DEVICE_NAME,
	.tseg1_min = 1,
	.tseg1_max = 32,
	.tseg2_min = 1,
	.tseg2_max = 16,
	.sjw_max = 16,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

static const char *mcp25xxfd_get_mode_str(const u8 mode)
{
	switch (mode) {
	case MCP25XXFD_CAN_CON_MODE_MIXED:
		return "Mixed (CAN FD/CAN 2.0)"; break;
	case MCP25XXFD_CAN_CON_MODE_SLEEP:
		return "Sleep"; break;
	case MCP25XXFD_CAN_CON_MODE_INT_LOOPBACK:
		return "Internal Loopback"; break;
	case MCP25XXFD_CAN_CON_MODE_LISTENONLY:
		return "Listen Only"; break;
	case MCP25XXFD_CAN_CON_MODE_CONFIG:
		return "Configuration"; break;
	case MCP25XXFD_CAN_CON_MODE_EXT_LOOPBACK:
		return "External Loopback"; break;
	case MCP25XXFD_CAN_CON_MODE_CAN2_0:
		return "CAN 2.0"; break;
	case MCP25XXFD_CAN_CON_MODE_RESTRICTED:
		return "Restricted Operation"; break;
	}

	return "<unknown>";
}

static inline int mcp25xxfd_vdd_enable(const struct mcp25xxfd_priv *priv)
{
	if (!priv->reg_vdd)
		return 0;

	return regulator_enable(priv->reg_vdd);
}

static inline int mcp25xxfd_vdd_disable(const struct mcp25xxfd_priv *priv)
{
	if (!priv->reg_vdd)
		return 0;

	return regulator_disable(priv->reg_vdd);
}

static inline int
mcp25xxfd_transceiver_enable(const struct mcp25xxfd_priv *priv)
{
	if (!priv->reg_xceiver)
		return 0;

	return regulator_enable(priv->reg_xceiver);
}

static inline int
mcp25xxfd_transceiver_disable(const struct mcp25xxfd_priv *priv)
{
	if (!priv->reg_xceiver)
		return 0;

	return regulator_disable(priv->reg_xceiver);
}

static int mcp25xxfd_clks_and_vdd_enable(const struct mcp25xxfd_priv *priv)
{
	int err;

	err = clk_prepare_enable(priv->clk);
	if (err)
		return err;

	err = mcp25xxfd_vdd_enable(priv);
	if (err)
		clk_disable_unprepare(priv->clk);

	return err;
}

static int mcp25xxfd_clks_and_vdd_disable(const struct mcp25xxfd_priv *priv)
{
	int err;

	err = mcp25xxfd_vdd_disable(priv);
	if (err)
		return err;

	clk_disable_unprepare(priv->clk);

	return 0;
}

static inline int
mcp25xxfd_cmd_prepare_write(struct mcp25xxfd_reg_write_buf *write_reg_buf,
			    const u16 reg, const u32 mask, const u32 val)
{
	u8 first_byte, last_byte, len;
	__le32 val_le32;

	first_byte = mcp25xxfd_first_byte_set(mask);
	last_byte = mcp25xxfd_last_byte_set(mask);
	len = last_byte - first_byte + 1;

	write_reg_buf->cmd = mcp25xxfd_cmd_write(reg + first_byte);
	val_le32 = cpu_to_le32(val >> 8 * first_byte);
	memcpy(write_reg_buf->data, &val_le32, len);

	return sizeof(write_reg_buf->cmd) + len;
}

static inline int
mcp25xxfd_tef_obj_tail_get_rel_addr_from_chip(const struct mcp25xxfd_priv *priv,
					      u16 *tef_obj_tail_rel_addr)
{
	int err;
	u32 tef_ua;

	err = regmap_read(priv->map, MCP25XXFD_CAN_TEFUA, &tef_ua);
	if (err)
		return err;

	*tef_obj_tail_rel_addr = tef_ua;

	return 0;
}

static inline int
mcp25xxfd_tef_tail_get_from_chip(const struct mcp25xxfd_priv *priv,
				 u8 *tef_tail)
{
	int err;
	u16 tef_obj_tail_rel_addr;

	err = mcp25xxfd_tef_obj_tail_get_rel_addr_from_chip(priv,
							    &tef_obj_tail_rel_addr);
	if (err)
		return err;

	*tef_tail = tef_obj_tail_rel_addr / sizeof(struct mcp25xxfd_hw_tef_obj);

	return 0;
}

static inline int
mcp25xxfd_rx_obj_tail_get_rel_addr_from_chip(const struct mcp25xxfd_priv *priv,
					     u16 *rx_obj_tail_rel_addr)
{
	int err;
	u32 fifo_ua;

	err = regmap_read(priv->map, MCP25XXFD_CAN_FIFOUA(MCP25XXFD_RX_FIFO(0)),
			  &fifo_ua);
	if (err)
		return err;

	*rx_obj_tail_rel_addr = fifo_ua;

	return 0;
}

static inline int
mcp25xxfd_rx_tail_get_from_chip(const struct mcp25xxfd_priv *priv, u8 *rx_tail)
{
	int err;
	u16 rx_obj_tail_rel_addr;

	err = mcp25xxfd_rx_obj_tail_get_rel_addr_from_chip(priv,
							   &rx_obj_tail_rel_addr);
	if (err)
		return err;

	rx_obj_tail_rel_addr -= (sizeof(struct mcp25xxfd_hw_tef_obj) +
				 priv->tx.obj_size) * priv->tx.obj_num;
	*rx_tail = rx_obj_tail_rel_addr / priv->rx.obj_size;

	return 0;
}

static void
mcp25xxfd_tx_ring_init_one(const struct mcp25xxfd_priv *priv,
			   struct mcp25xxfd_tx_obj *tx_obj, const u8 n)
{
	u32 val;
	u16 addr;
	u8 len;

	/* FIFO load */
	addr = mcp25xxfd_get_tx_obj_addr(priv, n);
	tx_obj->load.buf.cmd = mcp25xxfd_cmd_write(addr);
	/* len is calculated on the fly */

	spi_message_init_with_transfers(&tx_obj->load.msg,
					&tx_obj->load.xfer, 1);
	tx_obj->load.xfer.tx_buf = &tx_obj->load.buf;
	/* len is assigned on the fly */

	/* FIFO trigger */
	addr = MCP25XXFD_CAN_FIFOCON(MCP25XXFD_TX_FIFO);
	val = MCP25XXFD_CAN_FIFOCON_TXREQ | MCP25XXFD_CAN_FIFOCON_UINC;
	len = mcp25xxfd_cmd_prepare_write(&tx_obj->trigger.buf, addr, val, val);

	spi_message_init_with_transfers(&tx_obj->trigger.msg,
					&tx_obj->trigger.xfer, 1);
	tx_obj->trigger.xfer.tx_buf = &tx_obj->trigger.buf;
	tx_obj->trigger.xfer.len = len;
}

static void mcp25xxfd_ring_init(struct mcp25xxfd_priv *priv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(priv->tx.obj); i++) {
		struct mcp25xxfd_tx_obj *tx_obj = &priv->tx.obj[i];

		mcp25xxfd_tx_ring_init_one(priv, tx_obj, i);
	}

	priv->tef.head = 0;
	priv->tef.tail = 0;
	priv->tx.head = 0;
	priv->tx.tail = 0;
	priv->rx.head = 0;
	priv->rx.tail = 0;
}

static inline int
mcp25xxfd_chip_get_mode(const struct mcp25xxfd_priv *priv, u8 *mode)
{
	u32 val;
	int err;

	err = regmap_read(priv->map, MCP25XXFD_CAN_CON, &val);
	if (err)
		return err;

	*mode = FIELD_GET(MCP25XXFD_CAN_CON_OPMOD_MASK, val);

	return 0;
}

static int
__mcp25xxfd_chip_set_mode(const struct mcp25xxfd_priv *priv,
			  const u8 mode_req, bool nowait)
{
	u32 con, con_reqop;
	int err;

	con_reqop = FIELD_PREP(MCP25XXFD_CAN_CON_REQOP_MASK, mode_req);
	err = regmap_update_bits(priv->map, MCP25XXFD_CAN_CON,
				 MCP25XXFD_CAN_CON_REQOP_MASK, con_reqop);
	if (err)
		return err;

	if (mode_req == MCP25XXFD_CAN_CON_MODE_SLEEP || nowait)
		return 0;

	err = regmap_read_poll_timeout(priv->map, MCP25XXFD_CAN_CON, con,
				       FIELD_GET(MCP25XXFD_CAN_CON_OPMOD_MASK,
						 con) == mode_req,
				       10000, 1000000);
	if (err) {
		u8 mode = FIELD_GET(MCP25XXFD_CAN_CON_OPMOD_MASK, con);

		netdev_err(priv->ndev,
			   "Controller failed to enter mode %s Mode (%u) and stays in %s Mode (%u).\n",
			   mcp25xxfd_get_mode_str(mode_req), mode_req,
			   mcp25xxfd_get_mode_str(mode), mode);
		return err;
	}

	return 0;
}

static inline int
mcp25xxfd_chip_set_mode(const struct mcp25xxfd_priv *priv,
			const u8 mode_req)
{
	return __mcp25xxfd_chip_set_mode(priv, mode_req, false);
}

static inline int
mcp25xxfd_chip_set_mode_nowait(const struct mcp25xxfd_priv *priv,
			       const u8 mode_req)
{
	return __mcp25xxfd_chip_set_mode(priv, mode_req, true);
}

static int mcp25xxfd_chip_clock_enable(const struct mcp25xxfd_priv *priv)
{
	u32 osc, osc_reference, osc_mask;
	int err;

	/* Set Power On Defaults for "Clock Output Divisor" and remove
	 * "Oscillator Disable" bit.
	 */
	osc = FIELD_PREP(MCP25XXFD_OSC_CLKODIV_MASK, MCP25XXFD_OSC_CLKODIV_10);
	osc_reference = MCP25XXFD_OSC_OSCRDY;
	osc_mask = MCP25XXFD_OSC_OSCRDY | MCP25XXFD_OSC_PLLRDY;

	/* Note:
	 *
	 * If the controller is in Sleep Mode the following write only
	 * removes the "Oscillator Disable" bit and powers it up. All
	 * other bits are unaffected.
	 */
	err = regmap_write(priv->map, MCP25XXFD_OSC, osc);
	if (err)
		return err;

	/* Wait for "Oscillator Ready" bit */
	return regmap_read_poll_timeout(priv->map, MCP25XXFD_OSC, osc,
					(osc & osc_mask) == osc_reference,
					10000, 1000000);
}

static int mcp25xxfd_chip_clock_init(const struct mcp25xxfd_priv *priv)
{
	u32 osc;
	int err;

	/* Activate Low Power Mode on Oscillator Disable. This only
	 * works on the MCP2518FD. The MCP2517FD will go into normal
	 * Sleep Mode instead.
	 */
	osc = MCP25XXFD_OSC_LPMEN |
		FIELD_PREP(MCP25XXFD_OSC_CLKODIV_MASK,
			   MCP25XXFD_OSC_CLKODIV_10);
	err = regmap_write(priv->map, MCP25XXFD_OSC, osc);
	if (err)
		return err;

	/* Set Time Base Counter Prescaler to 1.
	 *
	 * This means an overflow of the 32 bit Time Base Counter
	 * register at 40 MHz every 107 seconds.
	 */
	return regmap_write(priv->map, MCP25XXFD_CAN_TSCON,
			    MCP25XXFD_CAN_TSCON_TBCEN);
}

static int mcp25xxfd_chip_softreset(const struct mcp25xxfd_priv *priv)
{
	const __be16 cmd = mcp25xxfd_cmd_reset();
	u8 mode;
	int err;

	/* The Set Mode and SPI Reset command only seems to works if
	 * the controller is not in Sleep Mode.
	 */
	err = mcp25xxfd_chip_clock_enable(priv);
	if (err)
		return err;

	err = mcp25xxfd_chip_set_mode(priv, MCP25XXFD_CAN_CON_MODE_CONFIG);
	if (err)
		return err;

	/* spi_write_then_read() works with non DMA-safe buffers */
	err = spi_write_then_read(priv->spi, &cmd, sizeof(cmd), NULL, 0);
	if (err)
		return err;

	err = mcp25xxfd_chip_get_mode(priv, &mode);
	if (err)
		return err;

	if (mode != MCP25XXFD_CAN_CON_MODE_CONFIG) {
		netdev_err(priv->ndev,
			   "Controller not in Config Mode after reset, but in %s Mode (%u).\n",
			   mcp25xxfd_get_mode_str(mode), mode);
		return -EINVAL;
	}

	return 0;
}

static int mcp25xxfd_set_bittiming(const struct mcp25xxfd_priv *priv)
{
	const struct can_bittiming *bt = &priv->can.bittiming;
	const struct can_bittiming *dbt = &priv->can.data_bittiming;
	u32 val = 0;
	s8 tdco;
	int err;

	/* CAN Control Register
	 *
	 * - no transmit bandwidth sharing
	 * - config mode
	 * - disable transmit queue
	 * - store in transmit FIFO event
	 * - transition to restricted operation mode on system error
	 * - ESI is transmitted recessive when ESI of message is high or
	 *   CAN controller error passive
	 * - restricted retransmission attempts,
	 *   use TQXCON_TXAT and FIFOCON_TXAT
	 * - wake-up filter bits T11FILTER
	 * - use CAN bus line filter for wakeup
	 * - protocol exception is treated as a form error
	 * - Do not compare data bytes
	 */
	val = FIELD_PREP(MCP25XXFD_CAN_CON_REQOP_MASK,
			 MCP25XXFD_CAN_CON_MODE_CONFIG) |
		MCP25XXFD_CAN_CON_STEF |
		MCP25XXFD_CAN_CON_ESIGM |
		MCP25XXFD_CAN_CON_RTXAT |
		FIELD_PREP(MCP25XXFD_CAN_CON_WFT_MASK,
			   MCP25XXFD_CAN_CON_WFT_T11FILTER) |
		MCP25XXFD_CAN_CON_WAKFIL |
		MCP25XXFD_CAN_CON_PXEDIS;

	if (!(priv->can.ctrlmode & CAN_CTRLMODE_FD_NON_ISO))
		val |= MCP25XXFD_CAN_CON_ISOCRCEN;

	err = regmap_write(priv->map, MCP25XXFD_CAN_CON, val);
	if (err)
		return err;

	/* Nominal Bit Time */
	val = FIELD_PREP(MCP25XXFD_CAN_NBTCFG_BRP_MASK, bt->brp - 1) |
		FIELD_PREP(MCP25XXFD_CAN_NBTCFG_TSEG1_MASK,
			   bt->prop_seg + bt->phase_seg1 - 1) |
		FIELD_PREP(MCP25XXFD_CAN_NBTCFG_TSEG2_MASK,
			   bt->phase_seg2 - 1) |
		FIELD_PREP(MCP25XXFD_CAN_NBTCFG_SJW_MASK, bt->sjw - 1);

	err = regmap_write(priv->map, MCP25XXFD_CAN_NBTCFG, val);
	if (err)
		return err;

	if (!(priv->can.ctrlmode & CAN_CTRLMODE_FD))
		return 0;

	/* Data Bit Time */
	val = FIELD_PREP(MCP25XXFD_CAN_DBTCFG_BRP_MASK, dbt->brp - 1) |
		FIELD_PREP(MCP25XXFD_CAN_DBTCFG_TSEG1_MASK,
			   dbt->prop_seg + dbt->phase_seg1 - 1) |
		FIELD_PREP(MCP25XXFD_CAN_DBTCFG_TSEG2_MASK,
			   dbt->phase_seg2 - 1) |
		FIELD_PREP(MCP25XXFD_CAN_DBTCFG_SJW_MASK, dbt->sjw - 1);

	err = regmap_write(priv->map, MCP25XXFD_CAN_DBTCFG, val);
	if (err)
		return err;

	/* Transmitter Delay Compensation */
	tdco = clamp_t(int, dbt->brp * (dbt->prop_seg + dbt->phase_seg1),
		       -64, 63);
	val = FIELD_PREP(MCP25XXFD_CAN_TDC_TDCMOD_MASK,
			 MCP25XXFD_CAN_TDC_TDCMOD_AUTO) |
		FIELD_PREP(MCP25XXFD_CAN_TDC_TDCO_MASK, tdco);

	return regmap_write(priv->map, MCP25XXFD_CAN_TDC, val);
}

static int mcp25xxfd_chip_pinctrl_init(const struct mcp25xxfd_priv *priv)
{
	u32 val;

	if (!priv->rx_int)
		return 0;

	/* Configure GPIOs:
	 * - PIN0: GPIO Input
	 * - PIN1: RX Interrupt
	 */
	val = MCP25XXFD_IOCON_PM0 | MCP25XXFD_IOCON_TRIS0;
	return regmap_write(priv->map, MCP25XXFD_IOCON, val);
}

static int mcp25xxfd_chip_fifo_compute(struct mcp25xxfd_priv *priv)
{
	int tef_obj_size, tx_obj_size, rx_obj_size;
	int tx_obj_num, rx_obj_num;
	int ram_free;

	tef_obj_size = sizeof(struct mcp25xxfd_hw_tef_obj);
	if (priv->can.ctrlmode & CAN_CTRLMODE_FD) {
		tx_obj_num = MCP25XXFD_TX_OBJ_NUM_CANFD;
		tx_obj_size = sizeof(struct mcp25xxfd_hw_tx_obj_canfd);
		rx_obj_size = sizeof(struct mcp25xxfd_hw_rx_obj_canfd);
	} else {
		tx_obj_num = MCP25XXFD_TX_OBJ_NUM_CAN;
		tx_obj_size = sizeof(struct mcp25xxfd_hw_tx_obj_can);
		rx_obj_size = sizeof(struct mcp25xxfd_hw_rx_obj_can);
	}

	ram_free = MCP25XXFD_RAM_SIZE - tx_obj_num *
		(tef_obj_size + tx_obj_size);
	rx_obj_num = min(ram_free / rx_obj_size, 32);
	ram_free -= rx_obj_num * rx_obj_size;

	if ((priv->can.ctrlmode & CAN_CTRLMODE_FD &&
	     rx_obj_num > MCP25XXFD_RX_OBJ_NUM_CANFD) ||
	    (!(priv->can.ctrlmode & CAN_CTRLMODE_FD) &&
	     rx_obj_num > MCP25XXFD_RX_OBJ_NUM_CAN)) {
		netdev_err(priv->ndev,
			   "Too many rx-objects (calculated=%d, max=%d).\n",
			   rx_obj_num, priv->can.ctrlmode & CAN_CTRLMODE_FD ?
			   MCP25XXFD_RX_OBJ_NUM_CANFD :
			   MCP25XXFD_RX_OBJ_NUM_CAN);
		return -ENOMEM;
	}

	priv->tx.obj_num = tx_obj_num;
	priv->tx.obj_size = tx_obj_size;
	priv->rx.obj_num = rx_obj_num;
	priv->rx.obj_size = rx_obj_size;

	netdev_dbg(priv->ndev,
		   "FIFO setup: tef: %d*%d bytes = %d bytes, tx: %d*%d bytes = %d, rx: %d*%d bytes = %d bytes, free: %d bytes.\n",
		   tx_obj_num, tef_obj_size, tef_obj_size * tx_obj_num,
		   tx_obj_num, tx_obj_size, tx_obj_size * tx_obj_num,
		   rx_obj_num, rx_obj_size, rx_obj_size * rx_obj_num,
		   ram_free);

	return 0;
}

static int mcp25xxfd_chip_fifo_init(struct mcp25xxfd_priv *priv)
{
	u32 val;
	int err;

	err = mcp25xxfd_chip_fifo_compute(priv);
	if (err)
		return err;

	mcp25xxfd_ring_init(priv);

	/* TEF */
	val = FIELD_PREP(MCP25XXFD_CAN_TEFCON_FSIZE_MASK,
			 priv->tx.obj_num - 1) |
		MCP25XXFD_CAN_TEFCON_TEFTSEN |
		MCP25XXFD_CAN_TEFCON_TEFOVIE |
		MCP25XXFD_CAN_TEFCON_TEFNEIE;

	err = regmap_write(priv->map, MCP25XXFD_CAN_TEFCON, val);
	if (err)
		return err;

	/* FIFO 1 - TX */
	val = FIELD_PREP(MCP25XXFD_CAN_FIFOCON_FSIZE_MASK,
			 priv->tx.obj_num - 1) |
		MCP25XXFD_CAN_FIFOCON_TXEN |
		MCP25XXFD_CAN_FIFOCON_TXATIE;

	if (priv->can.ctrlmode & CAN_CTRLMODE_FD)
		val |= FIELD_PREP(MCP25XXFD_CAN_FIFOCON_PLSIZE_MASK,
				  MCP25XXFD_CAN_FIFOCON_PLSIZE_64);
	else
		val |= FIELD_PREP(MCP25XXFD_CAN_FIFOCON_PLSIZE_MASK,
				  MCP25XXFD_CAN_FIFOCON_PLSIZE_8);

	if (priv->can.ctrlmode & CAN_CTRLMODE_ONE_SHOT)
		val |= FIELD_PREP(MCP25XXFD_CAN_FIFOCON_TXAT_MASK,
				  MCP25XXFD_CAN_FIFOCON_TXAT_ONE_SHOT);
	else
		val |= FIELD_PREP(MCP25XXFD_CAN_FIFOCON_TXAT_MASK,
				  MCP25XXFD_CAN_FIFOCON_TXAT_UNLIMITED);

	err = regmap_write(priv->map, MCP25XXFD_CAN_FIFOCON(MCP25XXFD_TX_FIFO),
			   val);
	if (err)
		return err;

	/* FIFO 2 - RX */
	val = FIELD_PREP(MCP25XXFD_CAN_FIFOCON_FSIZE_MASK,
			 priv->rx.obj_num - 1) |
		MCP25XXFD_CAN_FIFOCON_RXTSEN |
		MCP25XXFD_CAN_FIFOCON_RXOVIE |
		MCP25XXFD_CAN_FIFOCON_TFNRFNIE;

	if (priv->can.ctrlmode & CAN_CTRLMODE_FD)
		val |= FIELD_PREP(MCP25XXFD_CAN_FIFOCON_PLSIZE_MASK,
				  MCP25XXFD_CAN_FIFOCON_PLSIZE_64);
	else
		val |= FIELD_PREP(MCP25XXFD_CAN_FIFOCON_PLSIZE_MASK,
				  MCP25XXFD_CAN_FIFOCON_PLSIZE_8);

	err = regmap_write(priv->map,
			   MCP25XXFD_CAN_FIFOCON(MCP25XXFD_RX_FIFO(0)), val);
	if (err)
		return err;

	/* RX Filter */
	val = MCP25XXFD_CAN_FLTCON_FLTEN0 |
		FIELD_PREP(MCP25XXFD_CAN_FLTCON_F0BP_MASK,
			   MCP25XXFD_RX_FIFO(0));
	return regmap_write(priv->map, MCP25XXFD_CAN_FLTCON(0), val);
}

static int mcp25xxfd_chip_ecc_init(const struct mcp25xxfd_priv *priv)
{
	void *ram;
	int err;

	ram = kzalloc(MCP25XXFD_RAM_SIZE, GFP_KERNEL);
	if (!ram)
		return -ENOMEM;

	err = regmap_raw_write(priv->map, MCP25XXFD_RAM_START, ram,
			       MCP25XXFD_RAM_SIZE);
	kfree(ram);

	return err;
}

static u8 mcp25xxfd_get_normal_mode(const struct mcp25xxfd_priv *priv)
{
	u8 mode;

	if (priv->can.ctrlmode & CAN_CTRLMODE_FD)
		mode = MCP25XXFD_CAN_CON_MODE_MIXED;
	else
		mode = MCP25XXFD_CAN_CON_MODE_CAN2_0;

	return mode;
}

static int
__mcp25xxfd_chip_set_normal_mode(const struct mcp25xxfd_priv *priv,
				 bool nowait)
{
	u8 mode;

	mode = mcp25xxfd_get_normal_mode(priv);

	return __mcp25xxfd_chip_set_mode(priv, mode, nowait);
}

static inline int
mcp25xxfd_chip_set_normal_mode(const struct mcp25xxfd_priv *priv)
{
	return __mcp25xxfd_chip_set_normal_mode(priv, false);
}

static inline int
mcp25xxfd_chip_set_normal_mode_nowait(const struct mcp25xxfd_priv *priv)
{
	return __mcp25xxfd_chip_set_normal_mode(priv, true);
}

static int mcp25xxfd_chip_interrupts_enable(const struct mcp25xxfd_priv *priv)
{
	u32 val;
	int err;

	val = MCP25XXFD_CAN_INT_CERRIE |
		MCP25XXFD_CAN_INT_SERRIE |
		MCP25XXFD_CAN_INT_RXOVIE |
		MCP25XXFD_CAN_INT_TXATIE |
		MCP25XXFD_CAN_INT_SPICRCIE |
		MCP25XXFD_CAN_INT_ECCIE |
		MCP25XXFD_CAN_INT_TEFIE |
		MCP25XXFD_CAN_INT_MODIE |
		MCP25XXFD_CAN_INT_RXIE;

	if (priv->can.ctrlmode & CAN_CTRLMODE_BERR_REPORTING)
		val |= MCP25XXFD_CAN_INT_IVMIE;

	err = regmap_write(priv->map, MCP25XXFD_CAN_INT, val);
	if (err)
		return err;

	val = MCP25XXFD_CRC_FERRIE |
		MCP25XXFD_CRC_CRCERRIE;
	err = regmap_write(priv->map, MCP25XXFD_CRC, val);
	if (err)
		return err;

	val = MCP25XXFD_ECCCON_DEDIE |
		MCP25XXFD_ECCCON_SECIE |
		MCP25XXFD_ECCCON_ECCEN;
	return regmap_write(priv->map, MCP25XXFD_ECCCON, val);
}

static int mcp25xxfd_chip_interrupts_disable(const struct mcp25xxfd_priv *priv)
{
	int err;

	err = regmap_write(priv->map, MCP25XXFD_ECCCON, 0);
	if (err)
		return err;

	err = regmap_write(priv->map, MCP25XXFD_CRC, 0);
	if (err)
		return err;

	return regmap_write(priv->map, MCP25XXFD_CAN_INT, 0);
}

static int mcp25xxfd_chip_start(struct mcp25xxfd_priv *priv)
{
	int err;

	err = mcp25xxfd_chip_softreset(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	err = mcp25xxfd_chip_clock_init(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	err = mcp25xxfd_set_bittiming(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	err = mcp25xxfd_chip_pinctrl_init(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	err = mcp25xxfd_chip_fifo_init(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	err = mcp25xxfd_chip_ecc_init(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	/* Note:
	 *
	 * First enable the interrupts, then bring the chip into
	 * Normal Mode. Otherwise on a MCP2517FD a burst of CAN
	 * messages on the bus may result in overwritten RX FIFO
	 * contents and ECC errors.
	 *
	 * The current theory is that the SPI read access disturbes
	 * the RX process in the chip.
	 */
	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	err = mcp25xxfd_chip_interrupts_enable(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	err = mcp25xxfd_chip_set_normal_mode(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	return 0;

 out_chip_set_mode_sleep:
	mcp25xxfd_chip_interrupts_disable(priv);
	mcp25xxfd_chip_set_mode(priv, MCP25XXFD_CAN_CON_MODE_SLEEP);
	priv->can.state = CAN_STATE_STOPPED;

	return err;
}

static int mcp25xxfd_chip_stop(struct mcp25xxfd_priv *priv,
			       const enum can_state state)
{
	priv->can.state = state;

	mcp25xxfd_chip_interrupts_disable(priv);
	return mcp25xxfd_chip_set_mode(priv, MCP25XXFD_CAN_CON_MODE_SLEEP);
}

static int mcp25xxfd_set_mode(struct net_device *ndev, enum can_mode mode)
{
	struct mcp25xxfd_priv *priv = netdev_priv(ndev);
	int err;

	switch (mode) {
	case CAN_MODE_START:
		err = mcp25xxfd_chip_start(priv);
		if (err)
			return err;

		netif_wake_queue(ndev);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int mcp25xxfd_get_berr_counter(const struct net_device *ndev,
				      struct can_berr_counter *bec)
{
	const struct mcp25xxfd_priv *priv = netdev_priv(ndev);
	u32 trec;
	int err;

	/* Avoid waking up the controller when the interface is down. */
	if (!(ndev->flags & IFF_UP))
		return 0;

	err = regmap_read(priv->map, MCP25XXFD_CAN_TREC, &trec);
	if (err)
		return err;

	bec->txerr = FIELD_GET(MCP25XXFD_CAN_TREC_TEC_MASK, trec);
	bec->rxerr = FIELD_GET(MCP25XXFD_CAN_TREC_REC_MASK, trec);

	return 0;
}

static int mcp25xxfd_check_tef_tail(struct mcp25xxfd_priv *priv)
{
	u8 tef_tail_chip, tef_tail;
	int err;

	if (!IS_ENABLED(CONFIG_CAN_MCP25XXFD_SANITY))
		return 0;

	err = mcp25xxfd_tef_tail_get_from_chip(priv, &tef_tail_chip);
	if (err)
		return err;

	mcp25xxfd_log_hw_tef_tail(priv, tef_tail_chip);

	tef_tail = mcp25xxfd_get_tef_tail(priv);
	if (tef_tail_chip != tef_tail) {
		netdev_err(priv->ndev,
			   "TEF tail of chip (0x%02x) and ours (0x%08x) inconsistent.\n",
			   tef_tail_chip, tef_tail);
		return -EILSEQ;
	}

	return 0;
}

static int mcp25xxfd_check_rx_tail(const struct mcp25xxfd_priv *priv)
{
	u8 rx_tail_chip, rx_tail;
	int err;

	if (!IS_ENABLED(CONFIG_CAN_MCP25XXFD_SANITY))
		return 0;

	err = mcp25xxfd_rx_tail_get_from_chip(priv, &rx_tail_chip);
	if (err)
		return err;

	rx_tail = mcp25xxfd_get_rx_tail(priv);
	if (rx_tail_chip != rx_tail) {
		netdev_err(priv->ndev,
			   "RX tail of chip (%d) and ours (%d) inconsistent.\n",
			   rx_tail_chip, rx_tail);
		return -EILSEQ;
	}

	return 0;
}

static int
mcp25xxfd_handle_tefif_recover(const struct mcp25xxfd_priv *priv, const u32 seq)
{
	u32 tef_sta;
	int err;

	err = regmap_read(priv->map, MCP25XXFD_CAN_TEFSTA, &tef_sta);
	if (err)
		return err;

	if (tef_sta & MCP25XXFD_CAN_TEFSTA_TEFOVIF) {
		netdev_err(priv->ndev,
			   "Transmit Event FIFO buffer overflow.\n");
		return -ENOBUFS;
	}

	netdev_info(priv->ndev,
		   "Transmit Event FIFO buffer %s (seq=0x%08x, tef_tail=0x%08x, tef_head=0x%08x, tx_head=0x%08x)\n",
		   tef_sta & MCP25XXFD_CAN_TEFSTA_TEFNEIF ?
		   "empty." : "not empty anymore?",
		   seq, priv->tef.tail, priv->tef.head, priv->tx.head);

	return -EAGAIN;
}

static int
mcp25xxfd_handle_tefif_one(struct mcp25xxfd_priv *priv,
			   const struct mcp25xxfd_hw_tef_obj *hw_tef_obj)
{
	struct net_device_stats *stats = &priv->ndev->stats;
	u32 seq, seq_masked, tef_tail_masked;
	int err;

	seq = FIELD_GET(MCP25XXFD_OBJ_FLAGS_SEQ_MCP2518FD_MASK,
			hw_tef_obj->flags);

	/* Use the MCP2517FD mask on the MCP2518FD, too. We only
	 * compare 7 bits, this should be enough to detect
	 * net-yet-completed, i.e. old TEF objects.
	 */
	seq_masked = seq &
		field_mask(MCP25XXFD_OBJ_FLAGS_SEQ_MCP2517FD_MASK);
	tef_tail_masked = priv->tef.tail &
		field_mask(MCP25XXFD_OBJ_FLAGS_SEQ_MCP2517FD_MASK);
	if (seq_masked != tef_tail_masked)
		return mcp25xxfd_handle_tefif_recover(priv, seq);

	mcp25xxfd_log(priv, hw_tef_obj->id);

	stats->tx_bytes +=
		can_rx_offload_get_echo_skb(&priv->offload,
					    mcp25xxfd_get_tef_tail(priv),
					    hw_tef_obj->ts);
	stats->tx_packets++;

	/* finally increment the TEF pointer */
	err = regmap_update_bits(priv->map, MCP25XXFD_CAN_TEFCON,
				 GENMASK(15, 8),
				 MCP25XXFD_CAN_TEFCON_UINC);
	if (err)
		return err;

	priv->tx.tail++;
	priv->tef.tail++;

	return mcp25xxfd_check_tef_tail(priv);
}

static int mcp25xxfd_tef_ring_update(struct mcp25xxfd_priv *priv)
{
	u32 fifo_sta, new_head;
	u8 tx_ci;
	int err;

	/* guess head */
	err = regmap_read(priv->map, MCP25XXFD_CAN_FIFOSTA(MCP25XXFD_TX_FIFO),
			  &fifo_sta);
	if (err)
		return err;

	tx_ci = FIELD_GET(MCP25XXFD_CAN_FIFOSTA_FIFOCI_MASK, fifo_sta);
	new_head = round_down(priv->tef.head, priv->tx.obj_num) + tx_ci;

	if (new_head <= priv->tef.head)
		new_head += priv->tx.obj_num;

	priv->tef.head = min(new_head, priv->tx.head);

	mcp25xxfd_log_hw_tx_ci(priv, tx_ci);

	return mcp25xxfd_check_tef_tail(priv);
}

static inline int
mcp25xxfd_tef_obj_read(const struct mcp25xxfd_priv *priv,
		       struct mcp25xxfd_hw_tef_obj *hw_tef_obj,
		       const u8 offset, const u8 len)
{
	if (IS_ENABLED(CONFIG_CAN_MCP25XXFD_SANITY) &&
	    (offset > priv->tx.obj_num ||
	     len > priv->tx.obj_num ||
	     offset + len > priv->tx.obj_num)) {
		netdev_err(priv->ndev,
			   "Trying to read to many TEF objects (max=%d, offset=%d, len=%d).\n",
			   priv->tx.obj_num, offset, len);
		return -ERANGE;
	}

	return regmap_bulk_read(priv->map,
				mcp25xxfd_get_tef_obj_addr(priv, offset),
				hw_tef_obj,
				sizeof(*hw_tef_obj) / sizeof(u32) * len);
}

static int mcp25xxfd_handle_tefif(struct mcp25xxfd_priv *priv)
{
	struct mcp25xxfd_hw_tef_obj hw_tef_obj[MCP25XXFD_TX_OBJ_NUM_MAX];
	u8 tef_tail, len, l;
	int err, i;

	err = mcp25xxfd_tef_ring_update(priv);
	if (err)
		return err;

	tef_tail = mcp25xxfd_get_tef_tail(priv);
	len = mcp25xxfd_get_tef_len(priv);
	l = mcp25xxfd_get_tef_linear_len(priv);
	err = mcp25xxfd_tef_obj_read(priv, hw_tef_obj, tef_tail, l);
	if (err)
		return err;

	if (l < len) {
		err = mcp25xxfd_tef_obj_read(priv, &hw_tef_obj[l], 0, len - l);
		if (err)
			return err;
	}

	for (i = 0; i < len; i++) {
		err = mcp25xxfd_handle_tefif_one(priv, &hw_tef_obj[i]);
		/* -EAGAIN means the Sequence Number in the TEF
		 * doesn't match our tef_tail. This can happen if we
		 * read the TEF objects too early. Leave loop let the
		 * interrupt handler call us again.
		 */
		if (err == -EAGAIN)
			goto out_netif_wake_queue;
		if (err)
			return err;
	}

 out_netif_wake_queue:
	mcp25xxfd_log_wake(priv, hw_tef_obj->id);
	netif_wake_queue(priv->ndev);

	return 0;
}

static int mcp25xxfd_rx_ring_update(struct mcp25xxfd_priv *priv)
{
	u32 fifo_sta, new_head;
	u8 rx_ci;
	int err;

	err = regmap_read(priv->map,
			  MCP25XXFD_CAN_FIFOSTA(MCP25XXFD_RX_FIFO(0)),
			  &fifo_sta);
	if (err)
		return err;

	rx_ci = FIELD_GET(MCP25XXFD_CAN_FIFOSTA_FIFOCI_MASK, fifo_sta);
	new_head = round_down(priv->rx.head, priv->rx.obj_num) + rx_ci;

	if (new_head <= priv->rx.head)
		new_head += priv->rx.obj_num;

	priv->rx.head = new_head;

	return mcp25xxfd_check_rx_tail(priv);
}

static void
mcp25xxfd_hw_rx_obj_to_skb(const struct mcp25xxfd_priv *priv,
			   const struct mcp25xxfd_hw_rx_obj_canfd *hw_rx_obj,
			   struct sk_buff *skb)
{
	struct canfd_frame *cfd = (struct canfd_frame *)skb->data;

	if (hw_rx_obj->flags & MCP25XXFD_OBJ_FLAGS_IDE) {
		u32 sid, eid;

		eid = FIELD_GET(MCP25XXFD_OBJ_ID_EID_MASK, hw_rx_obj->id);
		sid = FIELD_GET(MCP25XXFD_OBJ_ID_SID_MASK, hw_rx_obj->id);

		cfd->can_id = CAN_EFF_FLAG |
			FIELD_PREP(MCP25XXFD_CAN_FRAME_EFF_EID_MASK, eid) |
			FIELD_PREP(MCP25XXFD_CAN_FRAME_EFF_SID_MASK, sid);
	} else {
		cfd->can_id = FIELD_GET(MCP25XXFD_OBJ_ID_SID_MASK,
					hw_rx_obj->id);
	}

	/* CANFD */
	if (hw_rx_obj->flags & MCP25XXFD_OBJ_FLAGS_FDF) {
		u8 dlc;

		if (hw_rx_obj->flags & MCP25XXFD_OBJ_FLAGS_ESI)
			cfd->flags |= CANFD_ESI;

		if (hw_rx_obj->flags & MCP25XXFD_OBJ_FLAGS_BRS)
			cfd->flags |= CANFD_BRS;

		dlc = FIELD_GET(MCP25XXFD_OBJ_FLAGS_DLC, hw_rx_obj->flags);
		cfd->len = can_dlc2len(get_canfd_dlc(dlc));
	} else {
		if (hw_rx_obj->flags & MCP25XXFD_OBJ_FLAGS_RTR)
			cfd->can_id |= CAN_RTR_FLAG;

		cfd->len = get_can_dlc(FIELD_GET(MCP25XXFD_OBJ_FLAGS_DLC,
						 hw_rx_obj->flags));
	}

	memcpy(cfd->data, hw_rx_obj->data, cfd->len);
}

static int
mcp25xxfd_handle_rxif_one(struct mcp25xxfd_priv *priv,
			  const struct mcp25xxfd_hw_rx_obj_canfd *hw_rx_obj)
{
	struct net_device_stats *stats = &priv->ndev->stats;
	struct sk_buff *skb;
	struct canfd_frame *cfd;
	int err;

	if (hw_rx_obj->flags & MCP25XXFD_OBJ_FLAGS_FDF)
		skb = alloc_canfd_skb(priv->ndev, &cfd);
	else
		skb = alloc_can_skb(priv->ndev, (struct can_frame **)&cfd);

	if (!cfd) {
		stats->rx_dropped++;
		return 0;
	}

	mcp25xxfd_hw_rx_obj_to_skb(priv, hw_rx_obj, skb);
	err = can_rx_offload_queue_sorted(&priv->offload, skb, hw_rx_obj->ts);
	if (err)
		stats->rx_fifo_errors++;

	priv->rx.tail++;

	/* finally increment the RX pointer */
	return regmap_update_bits(priv->map,
				  MCP25XXFD_CAN_FIFOCON(MCP25XXFD_RX_FIFO(0)),
				  GENMASK(15, 8),
				  MCP25XXFD_CAN_FIFOCON_UINC);
}

static inline int
mcp25xxfd_rx_obj_read(const struct mcp25xxfd_priv *priv,
		      struct mcp25xxfd_hw_rx_obj_canfd *hw_rx_obj,
		      const u8 offset, const u8 len)
{
	return regmap_bulk_read(priv->map,
				mcp25xxfd_get_rx_obj_addr(priv, offset),
				hw_rx_obj,
				len * priv->rx.obj_size / sizeof(u32));
}

static int mcp25xxfd_handle_rxif(struct mcp25xxfd_priv *priv)
{
	struct mcp25xxfd_hw_rx_obj_canfd *hw_rx_obj = priv->rx.obj;
	u8 rx_tail, len, l;
	int err, i;

	err = mcp25xxfd_rx_ring_update(priv);
	if (err)
		return err;

	rx_tail = mcp25xxfd_get_rx_tail(priv);
	len = mcp25xxfd_get_rx_len(priv);
	l = mcp25xxfd_get_rx_linear_len(priv);
	err = mcp25xxfd_rx_obj_read(priv, hw_rx_obj, rx_tail, l);
	if (err)
		return err;

	if (l < len) {
		err = mcp25xxfd_rx_obj_read(priv, (void *)hw_rx_obj +
					    l * priv->rx.obj_size, 0, len - l);
		if (err)
			return err;
	}

	for (i = 0; i < len; i++) {
		err = mcp25xxfd_handle_rxif_one(priv, (void *)hw_rx_obj +
						i * priv->rx.obj_size);
		if (err)
			return err;
	}

	return 0;
}

static inline int mcp25xxfd_get_timestamp(const struct mcp25xxfd_priv *priv,
					  u32 *timestamp)
{
	return regmap_read(priv->map, MCP25XXFD_CAN_TBC, timestamp);
}

static struct sk_buff *
mcp25xxfd_alloc_can_err_skb(const struct mcp25xxfd_priv *priv,
			    struct can_frame **cf, u32 *timestamp)
{
	int err;

	err = mcp25xxfd_get_timestamp(priv, timestamp);
	if (err)
		return NULL;

	return alloc_can_err_skb(priv->ndev, cf);
}

static int mcp25xxfd_handle_rxovif(struct mcp25xxfd_priv *priv)
{
	struct net_device_stats *stats = &priv->ndev->stats;
	struct sk_buff *skb;
	struct can_frame *cf;
	u32 timestamp, rxovif;
	int err, i;

	stats->rx_over_errors++;
	stats->rx_errors++;

	err = regmap_read(priv->map, MCP25XXFD_CAN_RXOVIF, &rxovif);
	if (err)
		return err;

	for (i = 0; i < MCP25XXFD_RX_FIFO_NUM; i++) {
		const u8 rx_fifo = MCP25XXFD_RX_FIFO(i);

		if (rxovif & BIT(rx_fifo)) {
			netdev_warn(priv->ndev,
				   "RX-FIFO overflow in FIFO %d.\n", rx_fifo);

			err = regmap_update_bits(priv->map,
						 MCP25XXFD_CAN_FIFOSTA(rx_fifo),
						 MCP25XXFD_CAN_FIFOSTA_RXOVIF,
						 0x0);
			if (err)
				return err;
		}
	}

	skb = mcp25xxfd_alloc_can_err_skb(priv, &cf, &timestamp);
	if (!skb)
		return 0;

	cf->can_id |= CAN_ERR_CRTL;
	cf->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;

	err = can_rx_offload_queue_sorted(&priv->offload, skb, timestamp);
	if (err)
		stats->rx_fifo_errors++;

	return 0;
}

static int mcp25xxfd_handle_txatif(struct mcp25xxfd_priv *priv)
{
	netdev_info(priv->ndev, "%s\n", __func__);

	return 0;
}

static int mcp25xxfd_handle_ivmif(struct mcp25xxfd_priv *priv)
{
	struct net_device_stats *stats = &priv->ndev->stats;
	u32 bdiag1, timestamp;
	struct sk_buff *skb;
	struct can_frame *cf = NULL;
	int err;

	err = mcp25xxfd_get_timestamp(priv, &timestamp);
	if (err)
		return err;

	err = regmap_read(priv->map, MCP25XXFD_CAN_BDIAG1, &bdiag1);
	if (err)
		return err;

	/* Write 0s to clear error bits, don't write 1s to non active
	 * bits, as they will be set.
	 */
	err = regmap_write(priv->map, MCP25XXFD_CAN_BDIAG1, 0x0);
	if (err)
		return err;

	priv->can.can_stats.bus_error++;

	skb = alloc_can_err_skb(priv->ndev, &cf);
	if (cf)
		cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;

	/* Controller misconfiguration */
	if (WARN_ON(bdiag1 & MCP25XXFD_CAN_BDIAG1_DLCMM))
		netdev_err(priv->ndev,
			   "recv'd DLC is larger than PLSIZE of FIFO element.");

	/* RX errors */
	if (bdiag1 & (MCP25XXFD_CAN_BDIAG1_DCRCERR |
		      MCP25XXFD_CAN_BDIAG1_NCRCERR)) {
		stats->rx_errors++;
		if (cf)
			cf->data[3] |= CAN_ERR_PROT_LOC_CRC_SEQ;
	}
	if (bdiag1 & (MCP25XXFD_CAN_BDIAG1_DSTUFERR |
		      MCP25XXFD_CAN_BDIAG1_NSTUFERR)) {
		stats->rx_errors++;
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_STUFF;
	}
	if (bdiag1 & (MCP25XXFD_CAN_BDIAG1_DFORMERR |
		      MCP25XXFD_CAN_BDIAG1_NFORMERR)) {
		stats->rx_errors++;
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_FORM;
	}

	/* TX errors */
	if (bdiag1 & MCP25XXFD_CAN_BDIAG1_NACKERR) {
		stats->tx_errors++;
		if (cf) {
			cf->can_id |= CAN_ERR_ACK;
			cf->data[2] |= CAN_ERR_PROT_TX;
		}
	}
	if (bdiag1 & (MCP25XXFD_CAN_BDIAG1_DBIT1ERR |
		      MCP25XXFD_CAN_BDIAG1_NBIT1ERR)) {
		stats->tx_errors++;
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_TX | CAN_ERR_PROT_BIT1;
	}
	if (bdiag1 & (MCP25XXFD_CAN_BDIAG1_DBIT0ERR |
		      MCP25XXFD_CAN_BDIAG1_NBIT0ERR)) {
		stats->tx_errors++;
		if (cf)
			cf->data[2] |= CAN_ERR_PROT_TX | CAN_ERR_PROT_BIT0;
	}

	if (!cf)
		return 0;

	err = can_rx_offload_queue_sorted(&priv->offload, skb, timestamp);
	if (err)
		stats->rx_fifo_errors++;

	return 0;
}

static int mcp25xxfd_handle_cerrif(struct mcp25xxfd_priv *priv)
{
	struct net_device_stats *stats = &priv->ndev->stats;
	struct sk_buff *skb;
	struct can_frame *cf = NULL;
	enum can_state new_state, rx_state, tx_state;
	u32 trec, timestamp;
	int err;

	/* The skb allocation might fail, but can_change_state()
	 * handles cf == NULL.
	 */
	skb = mcp25xxfd_alloc_can_err_skb(priv, &cf, &timestamp);

	err = regmap_read(priv->map, MCP25XXFD_CAN_TREC, &trec);
	if (err)
		return err;

	if (trec & MCP25XXFD_CAN_TREC_TXBO)
		tx_state = CAN_STATE_BUS_OFF;
	else if (trec & MCP25XXFD_CAN_TREC_TXBP)
		tx_state = CAN_STATE_ERROR_PASSIVE;
	else if (trec & MCP25XXFD_CAN_TREC_TXWARN)
		tx_state = CAN_STATE_ERROR_WARNING;
	else
		tx_state = CAN_STATE_ERROR_ACTIVE;

	if (trec & MCP25XXFD_CAN_TREC_RXBP)
		rx_state = CAN_STATE_ERROR_PASSIVE;
	else if (trec & MCP25XXFD_CAN_TREC_RXWARN)
		rx_state = CAN_STATE_ERROR_WARNING;
	else
		rx_state = CAN_STATE_ERROR_ACTIVE;

	new_state = max(tx_state, rx_state);
	if (new_state == priv->can.state)
		return 0;

	can_change_state(priv->ndev, cf, tx_state, rx_state);

	if (new_state == CAN_STATE_BUS_OFF) {
		mcp25xxfd_chip_stop(priv, CAN_STATE_BUS_OFF);
		can_bus_off(priv->ndev);
	}

	if (!skb)
		return 0;

	if (new_state != CAN_STATE_BUS_OFF) {
		struct can_berr_counter bec;

		err = mcp25xxfd_get_berr_counter(priv->ndev, &bec);
		cf->data[6] = bec.txerr;
		cf->data[7] = bec.rxerr;
	}

	err = can_rx_offload_queue_sorted(&priv->offload, skb, timestamp);
	if (err)
		stats->rx_fifo_errors++;

	return 0;
}

static int mcp25xxfd_handle_modif(const struct mcp25xxfd_priv *priv)
{
	const u8 mode_reference = mcp25xxfd_get_normal_mode(priv);
	u8 mode;
	int err;

	err = mcp25xxfd_chip_get_mode(priv, &mode);
	if (err)
		return err;

	if (mode == mode_reference)
		return 0;

	/* According to MCP2517FD errata DS80000792B, during a TX MAB
	 * underflow, the controller will transition to Restricted
	 * Operation Mode or Listen Only Mode (depending on SERR2LOM).
	 *
	 * However this is not always the case. When SERR2LOM is
	 * configured for Restricted Operation Mode (SERR2LOM not set)
	 * the MCP2517FD will sometimes transition to Listen Only Mode
	 * first. When polling this bit we see that it will transition
	 * to Restricted Operation Mode shortly after.
	 */
	if (mode == MCP25XXFD_CAN_CON_MODE_RESTRICTED ||
	    mode == MCP25XXFD_CAN_CON_MODE_LISTENONLY)
		netdev_dbg(priv->ndev,
			    "Controller changed into %s Mode (%u).\n",
			    mcp25xxfd_get_mode_str(mode), mode);
	else
		netdev_err(priv->ndev,
			   "Controller changed into %s Mode (%u).\n",
			   mcp25xxfd_get_mode_str(mode), mode);

	/* After the application requests Normal mode, the CAN FD
	 * Controller will automatically attempt to retransmit the
	 * message that caused the TX MAB underflow.
	 */
	return mcp25xxfd_chip_set_normal_mode_nowait(priv);
}

static int mcp25xxfd_handle_serrif(struct mcp25xxfd_priv *priv)
{
	struct net_device_stats *stats = &priv->ndev->stats;

	/* TX MAB underflow
	 *
	 * According to the MCP2517FD Errata DS80000792B a TX MAB
	 * underflow is indicated by SERRIF and MODIF.
	 *
	 * Due to the corresponding Bus Errors, a IVMIF can be seen as
	 * well.
	 */
	if ((priv->intf & MCP25XXFD_CAN_INT_MODIF) &&
	    (priv->intf & MCP25XXFD_CAN_INT_IVMIF)) {
		stats->tx_aborted_errors++;
		stats->tx_errors++;

		return 0;
	}

	/* RX MAB overflow
	 *
	 * According to the MCP2517FD Errata DS80000792B a RX MAB
	 * overflow is indicated by SERRIF.
	 */
	if (priv->intf & MCP25XXFD_CAN_INT_RXIF) {
		stats->rx_dropped++;
		stats->rx_errors++;

		return 0;
	}

	return 0;
}

static int mcp25xxfd_handle_eccif(struct mcp25xxfd_priv *priv)
{
	int err;
	u32 ecc_stat;

	err = regmap_read(priv->map, MCP25XXFD_ECCSTAT, &ecc_stat);
	if (err)
		return err;

	err = regmap_update_bits(priv->map, MCP25XXFD_ECCSTAT,
				 MCP25XXFD_ECCSTAT_IF_MASK,
				 ~ecc_stat);
	if (err)
		return err;

	if (ecc_stat & MCP25XXFD_ECCSTAT_SECIF)
		netdev_info(priv->ndev,
			    "Single ECC Error corrected at address 0x%04lx.\n",
			    FIELD_GET(MCP25XXFD_ECCSTAT_ERRADDR_MASK,
				      ecc_stat));
	else if (ecc_stat & MCP25XXFD_ECCSTAT_DEDIF)
		netdev_notice(priv->ndev,
			      "Double ECC Error detected at address 0x%04lx.\n",
			      FIELD_GET(MCP25XXFD_ECCSTAT_ERRADDR_MASK,
					ecc_stat));

	return 0;
}

static int mcp25xxfd_handle_spicrcif(struct mcp25xxfd_priv *priv)
{
	int err;
	u32 crc;

	err = regmap_read(priv->map, MCP25XXFD_CRC, &crc);
	if (err)
		return err;

	err = regmap_update_bits(priv->map, MCP25XXFD_CRC,
				 MCP25XXFD_CRC_IF_MASK,
				 ~crc);
	if (err)
		return err;

	if (crc & MCP25XXFD_CRC_FERRIF)
		netdev_info(priv->ndev, "CRC Command Format Error.\n");
	else if (crc & MCP25XXFD_CRC_CRCERRIF)
		netdev_notice(priv->ndev, "CRC Error detected. CRC=0x%04lx.\n",
			      FIELD_GET(MCP25XXFD_CRC_MASK, crc));

	return 0;
}

#define mcp25xxfd_handle(priv, irq, ...) \
({ \
	int err; \
\
	err = mcp25xxfd_handle_##irq(priv, ## __VA_ARGS__); \
	if (err) \
		netdev_err(priv->ndev, \
			"IRQ handler mcp25xxfd_handle_%s() returned %d.\n", \
			__stringify(irq), err); \
	err; \
})

static irqreturn_t mcp25xxfd_irq(int irq, void *dev_id)
{
	struct mcp25xxfd_priv *priv = dev_id;
	irqreturn_t handled = IRQ_NONE;
	int err;

	if (priv->rx_int)
		do {
			int rx_pending;

			rx_pending = gpiod_get_value_cansleep(priv->rx_int);
			if (!rx_pending)
				break;

			err = mcp25xxfd_handle(priv, rxif);
			if (err)
				goto out_fail;

			handled = IRQ_HANDLED;
		} while (1);

	do {
		u32 intf_pending, intf_pending_clearable;

		err = regmap_read(priv->map, MCP25XXFD_CAN_INT, &priv->intf);
		if (err)
			goto out_fail;

		intf_pending = FIELD_GET(MCP25XXFD_CAN_INT_IF_MASK, priv->intf) &
			FIELD_GET(MCP25XXFD_CAN_INT_IE_MASK, priv->intf);

		if (!(intf_pending))
			return handled;

		/* Some interrupts must be ACKed in the
		 * MCP25XXFD_CAN_INT register.
		 * - First ACK then handle, to avoid lost-IRQ race
		 *   condition on fast re-occuring interrupts.
		 * - Write "0" to clear active IRQs, "1" to all other,
		 *   to avoid r/m/w race condition on the
		 *   MCP25XXFD_CAN_INT register.
		 */
		intf_pending_clearable = intf_pending &
			MCP25XXFD_CAN_INT_IF_CLEARABLE_MASK;
		if (intf_pending_clearable) {
			err = regmap_update_bits(priv->map, MCP25XXFD_CAN_INT,
						 MCP25XXFD_CAN_INT_IF_MASK,
						 ~intf_pending_clearable);
			if (err)
				goto out_fail;
		}

		if (intf_pending & MCP25XXFD_CAN_INT_MODIF) {
			err = mcp25xxfd_handle(priv, modif);
			if (err)
				goto out_fail;
		}

		if (intf_pending & MCP25XXFD_CAN_INT_RXIF) {
			err = mcp25xxfd_handle(priv, rxif);
			if (err)
				goto out_fail;
		}

		if (intf_pending & MCP25XXFD_CAN_INT_TEFIF) {
			err = mcp25xxfd_handle(priv, tefif);
			if (err)
				goto out_fail;
		}

		if (intf_pending & MCP25XXFD_CAN_INT_RXOVIF) {
			err = mcp25xxfd_handle(priv, rxovif);
			if (err)
				goto out_fail;
		}

		if (intf_pending & MCP25XXFD_CAN_INT_TXATIF) {
			err = mcp25xxfd_handle(priv, txatif);
			if (err)
				goto out_fail;
		}

		if (intf_pending & MCP25XXFD_CAN_INT_IVMIF) {
			err = mcp25xxfd_handle(priv, ivmif);
			if (err)
				goto out_fail;
		}

		/* On the MCP2527FD and MCP2518FD, we don't get a
		 * CERRIF IRQ on the transition TX ERROR_WARNING -> TX
		 * ERROR_ACTIVE.
		 */
		if (intf_pending & MCP25XXFD_CAN_INT_CERRIF ||
		    priv->can.state > CAN_STATE_ERROR_ACTIVE) {
			err = mcp25xxfd_handle(priv, cerrif);
			if (err)
				goto out_fail;
		}

		if (intf_pending & MCP25XXFD_CAN_INT_SERRIF) {
			err = mcp25xxfd_handle(priv, serrif);
			if (err)
				goto out_fail;
		}

		if (intf_pending & MCP25XXFD_CAN_INT_ECCIF) {
			err = mcp25xxfd_handle(priv, eccif);
			if (err)
				goto out_fail;
		}

		if (intf_pending & MCP25XXFD_CAN_INT_SPICRCIE) {
			err = mcp25xxfd_handle(priv, spicrcif);
			if (err)
				goto out_fail;
		}

		handled = IRQ_HANDLED;
	} while (1);

 out_fail:
	netdev_err(priv->ndev, "IRQ handler returned %d.\n", err);
	mcp25xxfd_dump(priv);
	mcp25xxfd_log_dump(priv);
	mcp25xxfd_chip_interrupts_disable(priv);

	return handled;
}

static inline struct
mcp25xxfd_tx_obj *mcp25xxfd_get_tx_obj_next(struct mcp25xxfd_priv *priv)
{
	u8 tx_head;

	tx_head = mcp25xxfd_get_tx_head(priv);

	return &priv->tx.obj[tx_head];
}

static void
mcp25xxfd_tx_obj_from_skb(const struct mcp25xxfd_priv *priv,
			  struct mcp25xxfd_tx_obj *tx_obj,
			  const struct sk_buff *skb)
{
	const struct canfd_frame *cfd = (struct canfd_frame *)skb->data;
	struct mcp25xxfd_hw_tx_obj_raw *hw_tx_obj = &tx_obj->load.buf.hw_tx_obj;
	u32 id, flags, len;

	if (cfd->can_id & CAN_EFF_FLAG) {
		u32 sid, eid;

		sid = FIELD_GET(MCP25XXFD_CAN_FRAME_EFF_SID_MASK, cfd->can_id);
		eid = FIELD_GET(MCP25XXFD_CAN_FRAME_EFF_EID_MASK, cfd->can_id);

		id = FIELD_PREP(MCP25XXFD_OBJ_ID_EID_MASK, eid) |
			FIELD_PREP(MCP25XXFD_OBJ_ID_SID_MASK, sid);

		flags = MCP25XXFD_OBJ_FLAGS_IDE;
	} else {
		id = FIELD_PREP(MCP25XXFD_OBJ_ID_SID_MASK, cfd->can_id);
		flags = 0;
	}

	/* Use the MCP2518FD mask even on the MCP2517FD. It doesn't
	 * harm, only the lower 7 bits will be transferred into the
	 * TEF object.
	 */
	flags |= FIELD_PREP(MCP25XXFD_OBJ_FLAGS_SEQ_MCP2518FD_MASK,
			    priv->tx.head) |
		FIELD_PREP(MCP25XXFD_OBJ_FLAGS_DLC, can_len2dlc(cfd->len));

	if (cfd->can_id & CAN_RTR_FLAG)
		flags |= MCP25XXFD_OBJ_FLAGS_RTR;

	/* CANFD */
	if (can_is_canfd_skb(skb)) {
		if (cfd->flags & CANFD_ESI)
			flags |= MCP25XXFD_OBJ_FLAGS_ESI;

		flags |= MCP25XXFD_OBJ_FLAGS_FDF;

		if (cfd->flags & CANFD_BRS)
			flags |= MCP25XXFD_OBJ_FLAGS_BRS;
	}

	put_unaligned_le32(id, &hw_tx_obj->id);
	put_unaligned_le32(flags, &hw_tx_obj->flags);

	// FIXME: what does the controller send in CANFD if can_dlc2len(can_len2dlc(cfd->len)) > cfd->len?
	memset(hw_tx_obj->data + round_down(cfd->len, sizeof(u32)),
	       0x0, sizeof(u32));
	memcpy(hw_tx_obj->data, cfd->data, cfd->len);

	len = sizeof(tx_obj->load.buf.cmd);
	len += sizeof(hw_tx_obj->id) + sizeof(hw_tx_obj->flags);
	len += round_up(cfd->len, sizeof(u32));

	tx_obj->load.xfer.len = len;
}

static int mcp25xxfd_tx_obj_write(const struct mcp25xxfd_priv *priv,
				  struct mcp25xxfd_tx_obj *tx_obj)
{
	int err;

	err = spi_async(priv->spi, &tx_obj->load.msg);
	if (err)
		return err;

	return spi_async(priv->spi, &tx_obj->trigger.msg);
}

static netdev_tx_t mcp25xxfd_start_xmit(struct sk_buff *skb,
					struct net_device *ndev)
{
	struct mcp25xxfd_priv *priv = netdev_priv(ndev);
	struct mcp25xxfd_tx_obj *tx_obj;
	const canid_t can_id = ((struct canfd_frame *)skb->data)->can_id;
	u8 tx_head;
	int err;

	if (can_dropped_invalid_skb(ndev, skb))
		return NETDEV_TX_OK;

	mcp25xxfd_log(priv, can_id);

	if (priv->tx.head - priv->tx.tail >= priv->tx.obj_num) {
		netdev_info(priv->ndev,
			   "Stopping tx-queue (tx_head=0x%08x, tx_tail=0x%08x, len=%d).\n",
			   priv->tx.head, priv->tx.tail,
			   priv->tx.head - priv->tx.tail);

		mcp25xxfd_log_busy(priv, can_id);
		netif_stop_queue(ndev);

		return NETDEV_TX_BUSY;
	}

	tx_obj = mcp25xxfd_get_tx_obj_next(priv);
	mcp25xxfd_tx_obj_from_skb(priv, tx_obj, skb);

	// FIXME:
	// if (!netdev_xmit_more() ||
	//	netif_xmit_stopped(netdev_get_tx_queue(netdev, 0)))

	/* Stop queue if we occupy the complete TX FIFO */
	tx_head = mcp25xxfd_get_tx_head(priv);
	priv->tx.head++;
	if (priv->tx.head - priv->tx.tail >= priv->tx.obj_num) {
		mcp25xxfd_log_stop(priv, can_id);
		netif_stop_queue(ndev);
	}

	can_put_echo_skb(skb, ndev, tx_head);

	err = mcp25xxfd_tx_obj_write(priv, tx_obj);
	if (err)
		goto out_err;

	return NETDEV_TX_OK;

 out_err:
	netdev_err(priv->ndev, "ERROR in %s: %d\n", __func__, err);
	mcp25xxfd_dump(priv);
	mcp25xxfd_log_dump(priv);

	return NETDEV_TX_OK;
}

static int mcp25xxfd_open(struct net_device *ndev)
{
	struct mcp25xxfd_priv *priv = netdev_priv(ndev);
	const struct spi_device *spi = priv->spi;
	int err;

	err = pm_runtime_get_sync(ndev->dev.parent);
	if (err < 0)
		return err;

	err = open_candev(ndev);
	if (err)
		goto out_pm_runtime_put;

	err = request_threaded_irq(spi->irq, NULL, mcp25xxfd_irq,
				   IRQF_ONESHOT, dev_name(&spi->dev),
				   priv);
	if (err)
		goto out_close;

	err = mcp25xxfd_transceiver_enable(priv);
	if (err)
		goto out_free_irq;

	can_rx_offload_enable(&priv->offload);

	err = mcp25xxfd_chip_start(priv);
	if (err)
		goto out_can_rx_offload_disable;

	netif_start_queue(ndev);

	return 0;

 out_can_rx_offload_disable:
	can_rx_offload_disable(&priv->offload);
	mcp25xxfd_transceiver_disable(priv);
 out_free_irq:
	free_irq(spi->irq, priv);
 out_close:
	close_candev(ndev);
 out_pm_runtime_put:
	mcp25xxfd_chip_stop(priv, CAN_STATE_STOPPED);
	pm_runtime_put(ndev->dev.parent);

	return err;
}

static int mcp25xxfd_stop(struct net_device *ndev)
{
	struct mcp25xxfd_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	mcp25xxfd_chip_stop(priv, CAN_STATE_STOPPED);
	can_rx_offload_disable(&priv->offload);
	mcp25xxfd_transceiver_disable(priv);
	free_irq(ndev->irq, priv);
	close_candev(ndev);

	pm_runtime_put(ndev->dev.parent);

	return 0;
}

static const struct net_device_ops mcp25xxfd_netdev_ops = {
	.ndo_open = mcp25xxfd_open,
	.ndo_stop = mcp25xxfd_stop,
	.ndo_start_xmit	= mcp25xxfd_start_xmit,
	.ndo_change_mtu = can_change_mtu,
};

static int mcp25xxfd_register_chip_detect(struct mcp25xxfd_priv *priv)
{
	const struct net_device *ndev = priv->ndev;
	u32 osc, osc_reference;
	enum mcp25xxfd_model model;
	int err;

	osc_reference = MCP25XXFD_OSC_OSCRDY |
		FIELD_PREP(MCP25XXFD_OSC_CLKODIV_MASK,
			   MCP25XXFD_OSC_CLKODIV_10);

	/* check reset defaults of OSC reg */
	err = regmap_read(priv->map, MCP25XXFD_OSC, &osc);
	if (err)
		return err;

	if (osc != osc_reference) {
		netdev_err(ndev,
			   "Chip failed to soft reset. osc=0x%08x, reference value=0x%08x\n",
			   osc, osc_reference);
		return -ENODEV;
	}

	/* The OSC_LPMEN is only supported on MCP2518FD, so use it to
	 * autodetect the model.
	 */
	err = regmap_update_bits(priv->map, MCP25XXFD_OSC,
				 MCP25XXFD_OSC_LPMEN, MCP25XXFD_OSC_LPMEN);
	if (err)
		return err;

	err = regmap_read(priv->map, MCP25XXFD_OSC, &osc);
	if (err)
		return err;

	if (osc & MCP25XXFD_OSC_LPMEN)
		model = CAN_MCP2518FD;
	else
		model = CAN_MCP2517FD;

	if (priv->model != CAN_MCP25XXFD &&
	    priv->model != model) {
		netdev_info(ndev,
			    "Detected MCP%xFD, but firmware specifies a MCP%xFD. Fixing up.",
			    model, priv->model);
	}
	priv->model = model;

	return 0;
}

static int mcp25xxfd_register_check_rx_int(struct mcp25xxfd_priv *priv)
{
	int err, rx_pending;

	if (!priv->rx_int)
		return 0;

	err = mcp25xxfd_chip_pinctrl_init(priv);
	if (err)
		return err;

	/* Check if RX_INT is properly working. The RX-INT should not
	 * be active after a softreset.
	 */
	rx_pending = gpiod_get_value_cansleep(priv->rx_int);
	if (!rx_pending)
		return 0;

	netdev_info(priv->ndev,
		   "RX-INT active after softreset, disabling RX-INT support.");
	devm_gpiod_put(&priv->spi->dev, priv->rx_int);
	priv->rx_int = NULL;

	return 0;
}

static int mcp25xxfd_register(struct mcp25xxfd_priv *priv)
{
	struct net_device *ndev = priv->ndev;
	int err;

	err = mcp25xxfd_clks_and_vdd_enable(priv);
	if (err)
		return err;

	pm_runtime_get_noresume(ndev->dev.parent);
	err = pm_runtime_set_active(ndev->dev.parent);
	if (err)
		goto out_runtime_put_noidle;
	pm_runtime_enable(ndev->dev.parent);

	/* Wait for oscillator startup timer after power up */
	mdelay(MCP25XXFD_OSC_DELAY_MS);

	err = mcp25xxfd_chip_softreset(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	err = mcp25xxfd_register_chip_detect(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	err = mcp25xxfd_register_check_rx_int(priv);
	if (err)
		goto out_chip_set_mode_sleep;

	err = register_candev(ndev);
	if (err)
		goto out_chip_set_mode_sleep;

	if (priv->model == CAN_MCP2517FD) {
		netdev_info(ndev, "MCP%xFD %ssuccessfully initialized.\n",
			    priv->model, priv->rx_int ? "(+RX-INT) " : "");
	} else {
		u32 devid;

		err = regmap_read(priv->map, MCP25XXFD_DEVID, &devid);
		if (err)
			goto out_unregister_candev;

		netdev_info(ndev, "MCP%xFD rev%lu.%lu %ssuccessfully initialized.\n",
			    priv->model,
			    FIELD_GET(MCP25XXFD_DEVID_ID_MASK, devid),
			    FIELD_GET(MCP25XXFD_DEVID_REV_MASK, devid),
			    priv->rx_int ? "(+RX-INT) " : "");
	}

	/* Put core into sleep mode and let pm_runtime_put() disable
	 * the clocks and vdd. If CONFIG_PM is not enabled, the clocks
	 * and vdd will stay powered.
	 */
	err = mcp25xxfd_chip_set_mode(priv, MCP25XXFD_CAN_CON_MODE_SLEEP);
	if (err)
		goto out_unregister_candev;

	pm_runtime_put(ndev->dev.parent);

	return 0;

 out_unregister_candev:
	unregister_candev(ndev);
 out_chip_set_mode_sleep:
	mcp25xxfd_chip_set_mode(priv, MCP25XXFD_CAN_CON_MODE_SLEEP);
	pm_runtime_disable(ndev->dev.parent);
 out_runtime_put_noidle:
	pm_runtime_put_noidle(ndev->dev.parent);
	mcp25xxfd_clks_and_vdd_disable(priv);

	return err;
}

static inline void mcp25xxfd_unregister(struct mcp25xxfd_priv *priv)
{
	struct net_device *ndev	= priv->ndev;

	unregister_candev(ndev);

	pm_runtime_get_sync(ndev->dev.parent);
	pm_runtime_put_noidle(ndev->dev.parent);
	mcp25xxfd_clks_and_vdd_disable(priv);
	pm_runtime_disable(ndev->dev.parent);
}

static const struct of_device_id mcp25xxfd_of_match[] = {
	{
		.compatible = "microchip,mcp2517fd",
		.data = (void *)CAN_MCP2517FD,
	}, {
		.compatible = "microchip,mcp2518fd",
		.data = (void *)CAN_MCP2518FD,
	}, {
		.compatible = "microchip,mcp25xxfd",
		.data = (void *)CAN_MCP25XXFD,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mcp25xxfd_of_match);

static const struct spi_device_id mcp25xxfd_id_table[] = {
	{
		.name = "mcp2517fd",
		.driver_data = (kernel_ulong_t)CAN_MCP2517FD,
	}, {
		.name = "mcp2518fd",
		.driver_data = (kernel_ulong_t)CAN_MCP2518FD,
	}, {
		.name = "mcp25xxfd",
		.driver_data = (kernel_ulong_t)CAN_MCP25XXFD,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(spi, mcp25xxfd_id_table);

static int mcp25xxfd_probe(struct spi_device *spi)
{
	const void *match;
	struct net_device *ndev;
	struct mcp25xxfd_priv *priv;
	struct gpio_desc *rx_int;
	struct regulator *reg_vdd, *reg_xceiver;
	struct clk *clk;
	u32 freq;
	int err;

	rx_int = devm_gpiod_get_optional(&spi->dev, "rx-int", GPIOD_IN);
	if (PTR_ERR(rx_int) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	else if (IS_ERR(rx_int))
		return PTR_ERR(rx_int);

	reg_vdd = devm_regulator_get_optional(&spi->dev, "vdd");
	if (PTR_ERR(reg_vdd) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	else if (PTR_ERR(reg_vdd) == -ENODEV)
		reg_vdd = NULL;
	else if (IS_ERR(reg_vdd))
		return PTR_ERR(reg_vdd);

	reg_xceiver = devm_regulator_get_optional(&spi->dev, "xceiver");
	if (PTR_ERR(reg_xceiver) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	else if (PTR_ERR(reg_xceiver) == -ENODEV)
		reg_xceiver = NULL;
	else if (IS_ERR(reg_xceiver))
		return PTR_ERR(reg_xceiver);

	clk = devm_clk_get(&spi->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&spi->dev, "No Oscillator (clock) defined.\n");
		return PTR_ERR(clk);
	}
	freq = clk_get_rate(clk);

	/* Sanity check */
	if (freq < MCP25XXFD_SYSCLOCK_HZ_MIN ||
	    freq > MCP25XXFD_SYSCLOCK_HZ_MAX) {
		dev_err(&spi->dev, "Oscillator frequency is too low.\n");
		return -ERANGE;
	}

	if (freq < MCP25XXFD_SYSCLOCK_HZ_MAX /
	    MCP25XXFD_OSC_PLL_MULTIPLIER) {
		dev_err(&spi->dev,
			"Oscillator frequency is too low and PLL in not supported.\n");
		return -ERANGE;
	}

	ndev = alloc_candev(sizeof(struct mcp25xxfd_priv),
			    MCP25XXFD_TX_OBJ_NUM_MAX);
	if (!ndev)
		return -ENOMEM;

	SET_NETDEV_DEV(ndev, &spi->dev);

	ndev->netdev_ops = &mcp25xxfd_netdev_ops;
	ndev->irq = spi->irq;
	ndev->flags |= IFF_ECHO;

	priv = netdev_priv(ndev);
	spi_set_drvdata(spi, priv);
	priv->can.clock.freq = freq;
	priv->can.do_set_mode = mcp25xxfd_set_mode;
	priv->can.do_get_berr_counter = mcp25xxfd_get_berr_counter;
	priv->can.bittiming_const = &mcp25xxfd_bittiming_const;
	priv->can.data_bittiming_const = &mcp25xxfd_data_bittiming_const;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_ONE_SHOT | CAN_CTRLMODE_FD |
		CAN_CTRLMODE_BERR_REPORTING;
	priv->ndev = ndev;
	priv->spi = spi;
	priv->rx_int = rx_int;
	priv->clk = clk;
	priv->reg_vdd = reg_vdd;
	priv->reg_xceiver = reg_xceiver;
	atomic_set(&priv->cnt, 0);

	match = device_get_match_data(&spi->dev);
	if (match)
		priv->model = (enum mcp25xxfd_model)match;
	else
		priv->model = spi_get_device_id(spi)->driver_data;

	spi->bits_per_word = 8;
	/* SPI clock must be less or equal SYSCLOCK / 2 */
	spi->max_speed_hz = min(spi->max_speed_hz, freq / 2);
	err = spi_setup(spi);
	if (err)
		goto out_free_candev;

	err = mcp25xxfd_regmap_init(priv);
	if (err)
		goto out_free_candev;

	err = can_rx_offload_add_manual(ndev, &priv->offload,
					MCP25XXFD_NAPI_WEIGHT);
	if (err)
		goto out_free_candev;

	err = mcp25xxfd_register(priv);
	if (err)
		goto out_free_candev;

	return 0;

 out_free_candev:
	free_candev(ndev);

	return err;
}

static int mcp25xxfd_remove(struct spi_device *spi)
{
	struct mcp25xxfd_priv *priv = spi_get_drvdata(spi);
	struct net_device *ndev = priv->ndev;

	can_rx_offload_del(&priv->offload);
	mcp25xxfd_unregister(priv);
	free_candev(ndev);

	return 0;
}

static int __maybe_unused mcp25xxfd_runtime_suspend(struct device *device)
{
	const struct mcp25xxfd_priv *priv = dev_get_drvdata(device);

	return mcp25xxfd_clks_and_vdd_disable(priv);
}

static int __maybe_unused mcp25xxfd_runtime_resume(struct device *device)
{
	const struct mcp25xxfd_priv *priv = dev_get_drvdata(device);

	return mcp25xxfd_clks_and_vdd_enable(priv);
}

static const struct dev_pm_ops mcp25xxfd_pm_ops = {
	SET_RUNTIME_PM_OPS(mcp25xxfd_runtime_suspend,
			   mcp25xxfd_runtime_resume, NULL)
};

static struct spi_driver mcp25xxfd_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.pm = &mcp25xxfd_pm_ops,
		.of_match_table = mcp25xxfd_of_match,
	},
	.probe = mcp25xxfd_probe,
	.remove = mcp25xxfd_remove,
	.id_table = mcp25xxfd_id_table,
};
module_spi_driver(mcp25xxfd_driver);

MODULE_AUTHOR("Marc Kleine-Budde <mkl@pengutornix.de>");
MODULE_DESCRIPTION("Microchip MCP25xxFD Family CAN controller driver");
MODULE_LICENSE("GPL v2");
