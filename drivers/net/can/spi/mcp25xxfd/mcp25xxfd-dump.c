// SPDX-License-Identifier: GPL-2.0
//
// mcp25xxfd - Microchip MCP25xxFD Family CAN controller driver
//
// Copyright (c) 2019, 2020 Pengutronix,
//                          Marc Kleine-Budde <kernel@pengutronix.de>
//

#include <linux/bitfield.h>
#include <linux/kernel.h>

#include "mcp25xxfd.h"

#define __dump_bit(val, prefix, bit, desc) \
	pr_info("%16s   %s\t\t%s\n", __stringify(bit), \
		(val) & prefix##_##bit ? "x" : " ", desc)

#define __dump_mask(val, prefix, mask, fmt, desc) \
	pr_info("%16s = " fmt "\t\t%s\n", \
		__stringify(mask), \
		FIELD_GET(prefix##_##mask##_MASK, (val)), \
		desc)

static void mcp25xxfd_dump_reg_con(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("CON: con(0x%03x)=0x%08x\n", addr, val);

	__dump_mask(val, MCP25XXFD_CAN_CON, TXBWS, "0x%02lx", "Transmit Bandwidth Sharing");
	__dump_bit(val, MCP25XXFD_CAN_CON, ABAT, "Abort All Pending Transmissions");
	__dump_mask(val, MCP25XXFD_CAN_CON, REQOP, "0x%02lx", "Request Operation Mode");
	__dump_mask(val, MCP25XXFD_CAN_CON, OPMOD, "0x%02lx", "Operation Mode Status");
	__dump_bit(val, MCP25XXFD_CAN_CON, TXQEN, "Enable Transmit Queue");
	__dump_bit(val, MCP25XXFD_CAN_CON, STEF, "Store in Transmit Event FIFO");
	__dump_bit(val, MCP25XXFD_CAN_CON, SERR2LOM, "Transition to Listen Only Mode on System Error");
	__dump_bit(val, MCP25XXFD_CAN_CON, ESIGM, "Transmit ESI in Gateway Mode");
	__dump_bit(val, MCP25XXFD_CAN_CON, RTXAT, "Restrict Retransmission Attempts");
	__dump_bit(val, MCP25XXFD_CAN_CON, BRSDIS, "Bit Rate Switching Disable");
	__dump_bit(val, MCP25XXFD_CAN_CON, BUSY, "CAN Module is Busy");
	__dump_mask(val, MCP25XXFD_CAN_CON, WFT, "0x%02lx", "Selectable Wake-up Filter Time");
	__dump_bit(val, MCP25XXFD_CAN_CON, WAKFIL, "Enable CAN Bus Line Wake-up Filter");
	__dump_bit(val, MCP25XXFD_CAN_CON, PXEDIS, "Protocol Exception Event Detection Disabled");
	__dump_bit(val, MCP25XXFD_CAN_CON, ISOCRCEN, "Enable ISO CRC in CAN FD Frames");
	__dump_mask(val, MCP25XXFD_CAN_CON, DNCNT, "0x%02lx", "Device Net Filter Bit Number");
}

static void mcp25xxfd_dump_reg_tbc(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("TBC: tbc(0x%03x)=0x%08x\n", addr, val);
}

static void mcp25xxfd_dump_reg_vec(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	u8 rx_code, tx_code, i_code;

	pr_info("VEC: vec(0x%03x)=0x%08x\n", addr, val);

	rx_code = FIELD_GET(MCP25XXFD_CAN_VEC_RXCODE_MASK, val);
	tx_code = FIELD_GET(MCP25XXFD_CAN_VEC_TXCODE_MASK, val);
	i_code = FIELD_GET(MCP25XXFD_CAN_VEC_ICODE_MASK, val);

	pr_info("\trxcode: ");
	if (rx_code == 0x40)
		pr_cont("No Interrupt");
	else if (rx_code < 0x20)
		pr_cont("FIFO %u", rx_code);
	else
		pr_cont("Reserved");
	pr_cont(" (0x%02x)\n", rx_code);

	pr_info("\ttxcode: ");
	if (tx_code == 0x40)
		pr_cont("No Interrupt");
	else if (tx_code < 0x20)
		pr_cont("FIFO %u", tx_code);
	else
		pr_cont("Reserved");
	pr_cont(" (0x%02x)\n", tx_code);

	pr_info("\ticode: ");
	if (i_code == 0x4a)
		pr_cont("Transmit Attempt Interrupt");
	else if (i_code == 0x49)
		pr_cont("Transmit Event FIFO Interrupt");
	else if (i_code == 0x48)
		pr_cont("Invalid Message Occurred");
	else if (i_code == 0x47)
		pr_cont("Operation Mode Changed");
	else if (i_code == 0x46)
		pr_cont("TBC Overflow");
	else if (i_code == 0x45)
		pr_cont("RX/TX MAB Overflow/Underflow");
	else if (i_code == 0x44)
		pr_cont("Address Error Interrupt");
	else if (i_code == 0x43)
		pr_cont("Receive FIFO Overflow Interrupt");
	else if (i_code == 0x42)
		pr_cont("Wake-up Interrupt");
	else if (i_code == 0x41)
		pr_cont("Error Interrupt");
	else if (i_code == 0x40)
		pr_cont("No Interrupt");
	else if (i_code < 0x20)
		pr_cont("FIFO %u", i_code);
	else
		pr_cont("Reserved");
	pr_cont(" (0x%02x)\n", i_code);
}

#define __dump_int(val, bit, desc) \
	pr_info("\t" __stringify(bit) "\t%s\t%s\t%s\t%s\n", \
		 (val) & MCP25XXFD_CAN_INT_##bit##E ? "x" : "", \
		 (val) & MCP25XXFD_CAN_INT_##bit##F ? "x" : "", \
		 FIELD_GET(MCP25XXFD_CAN_INT_IF_MASK, val) & \
		 FIELD_GET(MCP25XXFD_CAN_INT_IE_MASK, val) & \
		 MCP25XXFD_CAN_INT_##bit##F ? "x" : "", \
		 desc)

static void mcp25xxfd_dump_reg_intf(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("INT: intf(0x%03x)=0x%08x\n", addr, val);

	pr_info("\t\tIE\tIF\tIE & IF\n");
	__dump_int(val, IVMI, "Invalid Message Interrupt");
	__dump_int(val, WAKI, "Bus Wake Up Interrupt");
	__dump_int(val, CERRI, "CAN Bus Error Interrupt");
	__dump_int(val, SERRI, "System Error Interrupt");
	__dump_int(val, RXOVI, "Receive FIFO Overflow Interrupt");
	__dump_int(val, TXATI, "Transmit Attempt Interrupt");
	__dump_int(val, SPICRCI, "SPI CRC Error Interrupt");
	__dump_int(val, ECCI, "ECC Error Interrupt");
	__dump_int(val, TEFI, "Transmit Event FIFO Interrupt");
	__dump_int(val, MODI, "Mode Change Interrupt");
	__dump_int(val, TBCI, "Time Base Counter Interrupt");
	__dump_int(val, RXI, "Receive FIFO Interrupt");
	__dump_int(val, TXI, "Transmit FIFO Interrupt");
}

#undef __dump_int

#define __create_dump_fifo_bitmask(fifo, name, description) \
static void mcp25xxfd_dump_reg_##fifo(const struct mcp25xxfd_priv *priv, u32 val, u16 addr) \
{ \
	int i; \
\
	pr_info(__stringify(name) ": " __stringify(fifo) "(0x%03x)=0x%08x\n", addr, val); \
	pr_info(description ":\n"); \
	if (!val) { \
		pr_info("\t\t-none-\n"); \
		return; \
	} \
\
	pr_info("\t\t"); \
	for (i = 0; i < sizeof(val); i++) { \
		if (val & BIT(i)) \
			pr_cont("%d ", i); \
	} \
\
	pr_cont("\n"); \
}

__create_dump_fifo_bitmask(rxif, RXIF, "Receive FIFO Interrupt Pending bits");
__create_dump_fifo_bitmask(rxovif, RXOVIF, "Receive FIFO Overflow Interrupt Pending bits");
__create_dump_fifo_bitmask(txif, TXIF, "Transmit FIFO Interrupt Pending bits");
__create_dump_fifo_bitmask(txatif, TXATIF, "Transmit FIFO Attempt Interrupt Pending bits");
__create_dump_fifo_bitmask(txreq, TXREQ, "Message Send Request bits");

#undef __create_dump_fifo_bitmask

static void mcp25xxfd_dump_reg_trec(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("TREC: trec(0x%03x)=0x%08x\n", addr, val);

	__dump_bit(val, MCP25XXFD_CAN_TREC, TXBO, "Transmitter in Bus Off State");
	__dump_bit(val, MCP25XXFD_CAN_TREC, TXBP, "Transmitter in Error Passive State");
	__dump_bit(val, MCP25XXFD_CAN_TREC, RXBP, "Receiver in Error Passive State");
	__dump_bit(val, MCP25XXFD_CAN_TREC, TXWARN, "Transmitter in Error Warning State");
	__dump_bit(val, MCP25XXFD_CAN_TREC, RXWARN, "Receiver in Error Warning State");
	__dump_bit(val, MCP25XXFD_CAN_TREC, EWARN, "Transmitter or Receiver is in Error Warning State");

	__dump_mask(val, MCP25XXFD_CAN_TREC, TEC, "%3lu", "Transmit Error Counter");
	__dump_mask(val, MCP25XXFD_CAN_TREC, REC, "%3lu", "Receive Error Counter");
}

static void mcp25xxfd_dump_reg_bdiag0(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("BDIAG0: bdiag0(0x%03x)=0x%08x\n", addr, val);

	__dump_mask(val, MCP25XXFD_CAN_BDIAG0, DTERRCNT, "%3lu", "Data Bit Rate Transmit Error Counter");
	__dump_mask(val, MCP25XXFD_CAN_BDIAG0, DRERRCNT, "%3lu", "Data Bit Rate Receive Error Counter");
	__dump_mask(val, MCP25XXFD_CAN_BDIAG0, NTERRCNT, "%3lu", "Nominal Bit Rate Transmit Error Counter");
	__dump_mask(val, MCP25XXFD_CAN_BDIAG0, NRERRCNT, "%3lu", "Nominal Bit Rate Receive Error Counter");
}

static void mcp25xxfd_dump_reg_bdiag1(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("BDIAG1: bdiag1(0x%03x)=0x%08x\n", addr, val);

	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, DLCMM, "DLC Mismatch");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, ESI, "ESI flag of a received CAN FD message was set");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, DCRCERR, "Data CRC Error");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, DSTUFERR, "Data Bit Stuffing Error");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, DFORMERR, "Data Format Error");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, DBIT1ERR, "Data BIT1 Error");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, DBIT0ERR, "Data BIT0 Error");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, TXBOERR, "Device went to bus-off (and auto-recovered)");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, NCRCERR, "CRC Error");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, NSTUFERR, "Bit Stuffing Error");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, NFORMERR, "Format Error");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, NACKERR, "Transmitted message was not acknowledged");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, NBIT1ERR, "Bit1 Error");
	__dump_bit(val, MCP25XXFD_CAN_BDIAG1, NBIT0ERR, "Bit0 Error");
	__dump_mask(val, MCP25XXFD_CAN_BDIAG1, EFMSGCNT, "%3lu", "Error Free Message Counter bits");
}

static void mcp25xxfd_dump_reg_osc(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("OSC: osc(0x%03x)=0x%08x\n", addr, val);

	__dump_bit(val, MCP25XXFD_OSC, SCLKRDY, "Synchronized SCLKDIV");
	__dump_bit(val, MCP25XXFD_OSC, OSCRDY, "Clock Ready");
	__dump_bit(val, MCP25XXFD_OSC, PLLRDY, "PLL Ready");
	__dump_mask(val, MCP25XXFD_OSC, CLKODIV, "%2lu", "Clock Output Divisor");
	__dump_bit(val, MCP25XXFD_OSC, SCLKDIV, "System Clock Divisor");
	__dump_bit(val, MCP25XXFD_OSC, LPMEN, "Low Power Mode (LPM) Enable (MCP2518FD only)");
	__dump_bit(val, MCP25XXFD_OSC, OSCDIS, "Clock (Oscillator) Disable");
	__dump_bit(val, MCP25XXFD_OSC, PLLEN, "PLL Enable");
}

static void mcp25xxfd_dump_reg_tefcon(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("TEFCON: tefcon(0x%03x)=0x%08x\n", addr, val);

	__dump_mask(val, MCP25XXFD_CAN_TEFCON, FSIZE, "%3lu", "FIFO Size");
	__dump_bit(val, MCP25XXFD_CAN_TEFCON, FRESET, "FIFO Reset");
	__dump_bit(val, MCP25XXFD_CAN_TEFCON, UINC, "Increment Tail");
	__dump_bit(val, MCP25XXFD_CAN_TEFCON, TEFTSEN, "Transmit Event FIFO Time Stamp Enable");
	__dump_bit(val, MCP25XXFD_CAN_TEFCON, TEFOVIE, "Transmit Event FIFO Overflow Interrupt Enable");
	__dump_bit(val, MCP25XXFD_CAN_TEFCON, TEFFIE, "Transmit Event FIFO Full Interrupt Enable");
	__dump_bit(val, MCP25XXFD_CAN_TEFCON, TEFHIE, "Transmit Event FIFO Half Full Interrupt Enable");
	__dump_bit(val, MCP25XXFD_CAN_TEFCON, TEFNEIE, "Transmit Event FIFO Not Empty Interrupt Enable");
}

static void mcp25xxfd_dump_reg_tefsta(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("TEFSTA: tefsta(0x%03x)=0x%08x\n", addr, val);

	__dump_bit(val, MCP25XXFD_CAN_TEFSTA, TEFOVIF, "Transmit Event FIFO Overflow Interrupt Flag");
	__dump_bit(val, MCP25XXFD_CAN_TEFSTA, TEFFIF, "Transmit Event FIFO Full Interrupt Flag (0 = not full)");
	__dump_bit(val, MCP25XXFD_CAN_TEFSTA, TEFHIF, "Transmit Event FIFO Half Full Interrupt Flag (0= < half full)");
	__dump_bit(val, MCP25XXFD_CAN_TEFSTA, TEFNEIF, "Transmit Event FIFO Not Empty Interrupt Flag (0=empty)");
}

static void mcp25xxfd_dump_reg_tefua(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("TEFUA: tefua(0x%03x)=0x%08x\n", addr, val);
}

static void mcp25xxfd_dump_reg_fifocon(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("FIFOCON: fifocon(0x%03x)=0x%08x\n", addr, val);

	__dump_mask(val, MCP25XXFD_CAN_FIFOCON, PLSIZE, "%3lu", "Payload Size");
	__dump_mask(val, MCP25XXFD_CAN_FIFOCON, FSIZE, "%3lu", "FIFO Size");
	__dump_mask(val, MCP25XXFD_CAN_FIFOCON, TXAT, "%3lu", "Retransmission Attempts");
	__dump_mask(val, MCP25XXFD_CAN_FIFOCON, TXPRI, "%3lu", "Message Transmit Priority");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, FRESET, "FIFO Reset");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, TXREQ, "Message Send Request");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, UINC, "Increment Head/Tail");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, TXEN, "TX/RX FIFO Selection (0=RX, 1=TX)");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, RTREN, "Auto RTR Enable");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, RXTSEN, "Received Message Time Stamp Enable");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, TXATIE, "Transmit Attempts Exhausted Interrupt Enable");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, RXOVIE, "Overflow Interrupt Enable");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, TFERFFIE, "Transmit/Receive FIFO Empty/Full Interrupt Enable");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, TFHRFHIE, "Transmit/Receive FIFO Half Empty/Half Full Interrupt Enable");
	__dump_bit(val, MCP25XXFD_CAN_FIFOCON, TFNRFNIE, "Transmit/Receive FIFO Not Full/Not Empty Interrupt Enable");
}

static void mcp25xxfd_dump_reg_fifosta(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("FIFOSTA: fifosta(0x%03x)=0x%08x\n", addr, val);

	__dump_mask(val, MCP25XXFD_CAN_FIFOSTA, FIFOCI, "%3lu", "FIFO Message Index");
	__dump_bit(val, MCP25XXFD_CAN_FIFOSTA, TXABT, "Message Aborted Status (1=aborted, 0=completed successfully)");
	__dump_bit(val, MCP25XXFD_CAN_FIFOSTA, TXLARB, "Message Lost Arbitration Status");
	__dump_bit(val, MCP25XXFD_CAN_FIFOSTA, TXERR, "Error Detected During Transmission");
	__dump_bit(val, MCP25XXFD_CAN_FIFOSTA, TXATIF, "Transmit Attempts Exhausted Interrupt Pending");
	__dump_bit(val, MCP25XXFD_CAN_FIFOSTA, RXOVIF, "Receive FIFO Overflow Interrupt Flag");
	__dump_bit(val, MCP25XXFD_CAN_FIFOSTA, TFERFFIF, "Transmit/Receive FIFO Empty/Full Interrupt Flag");
	__dump_bit(val, MCP25XXFD_CAN_FIFOSTA, TFHRFHIF, "Transmit/Receive FIFO Half Empty/Half Full Interrupt Flag");
	__dump_bit(val, MCP25XXFD_CAN_FIFOSTA, TFNRFNIF, "Transmit/Receive FIFO Not Full/Not Empty Interrupt Flag");
}

static void mcp25xxfd_dump_reg_fifoua(const struct mcp25xxfd_priv *priv, u32 val, u16 addr)
{
	pr_info("FIFOUA: fifoua(0x%03x)=0x%08x\n", addr, val);
}

#define __dump_call(regs, val) \
do { \
	mcp25xxfd_dump_reg_##val(priv, (regs)->val, (u16)offsetof(typeof(*(regs)), val)); \
	pr_info("\n"); \
} while (0)

#define __dump_call_fifo(reg, val) \
do { \
	mcp25xxfd_dump_reg_##reg(priv, regs->val, (u16)offsetof(typeof(*regs), val)); \
	pr_info("\n"); \
} while (0)

static void
mcp25xxfd_dump_regs(const struct mcp25xxfd_priv *priv,
		    const struct mcp25xxfd_dump_regs *regs,
		    const struct mcp25xxfd_dump_regs_mcp25xxfd *regs_mcp25xxfd)
{
	netdev_info(priv->ndev, "-------------------- register dump --------------------\n");
	__dump_call(regs_mcp25xxfd, osc);
	__dump_call(regs, con);
	__dump_call(regs, tbc);
	__dump_call(regs, vec);
	__dump_call(regs, intf);
	__dump_call(regs, rxif);
	__dump_call(regs, rxovif);
	__dump_call(regs, txif);
	__dump_call(regs, txatif);
	__dump_call(regs, txreq);
	__dump_call(regs, trec);
	__dump_call(regs, bdiag0);
	__dump_call(regs, bdiag1);
	pr_info("-------------------- TEF --------------------\n");
	__dump_call(regs, tefcon);
	__dump_call(regs, tefsta);
	__dump_call(regs, tefua);
	pr_info("-------------------- TX_FIFO --------------------\n");
	__dump_call_fifo(fifocon, fifo[MCP25XXFD_TX_FIFO].con);
	__dump_call_fifo(fifosta, fifo[MCP25XXFD_TX_FIFO].sta);
	__dump_call_fifo(fifoua, fifo[MCP25XXFD_TX_FIFO].ua);
	pr_info(" -------------------- RX_FIFO --------------------\n");
	__dump_call_fifo(fifocon, fifo[MCP25XXFD_RX_FIFO(0)].con);
	__dump_call_fifo(fifosta, fifo[MCP25XXFD_RX_FIFO(0)].sta);
	__dump_call_fifo(fifoua, fifo[MCP25XXFD_RX_FIFO(0)].ua);
	netdev_info(priv->ndev, "------------------------- end -------------------------\n");
}

#undef __dump_call
#undef __dump_call_fifo

static u8 mcp25xxfd_dump_get_fifo_size(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, u32 fifo_con)
{
	u8 obj_size;

	obj_size = FIELD_GET(MCP25XXFD_CAN_FIFOCON_PLSIZE_MASK, fifo_con);
	switch (obj_size) {
	case MCP25XXFD_CAN_FIFOCON_PLSIZE_8:
		return 8;
	case MCP25XXFD_CAN_FIFOCON_PLSIZE_12:
		return 12;
	case MCP25XXFD_CAN_FIFOCON_PLSIZE_16:
		return 16;
	case MCP25XXFD_CAN_FIFOCON_PLSIZE_20:
		return 20;
	case MCP25XXFD_CAN_FIFOCON_PLSIZE_24:
		return 24;
	case MCP25XXFD_CAN_FIFOCON_PLSIZE_32:
		return 32;
	case MCP25XXFD_CAN_FIFOCON_PLSIZE_48:
		return 48;
	case MCP25XXFD_CAN_FIFOCON_PLSIZE_64:
		return 64;
	}

	return 0;
}

static u8 mcp25xxfd_dump_get_fifo_obj_num(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, u32 fifo_con)
{
	u8 obj_num;

	obj_num = FIELD_GET(MCP25XXFD_CAN_FIFOCON_FSIZE_MASK, fifo_con);

	return obj_num + 1;
}

static void mcp25xxfd_dump_ram_fifo_obj_data(const struct mcp25xxfd_priv *priv, const u8 *data, u8 dlc)
{
	int i;
	u8 len;

	len = can_dlc2len(get_canfd_dlc(dlc));

	if (!len) {
		pr_info("%16s = -none-\n", "data");
		return;
	}

	for (i = 0; i < len; i++) {
		if ((i % 8) == 0) {
			if (i == 0)
				pr_info("%16s = %02x", "data", data[i]);
			else
				pr_info("                   %02x", data[i]);
		} else if ((i % 4) == 0) {
			pr_cont("  %02x", data[i]);
		} else if ((i % 8) == 7) {
			pr_cont(" %02x\n", data[i]);
		} else {
			pr_cont(" %02x", data[i]);
		}
	}

	if (i % 8)
		pr_cont("\n");
}

/* TEF */

static u8 mcp25xxfd_dump_get_tef_obj_num(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return mcp25xxfd_dump_get_fifo_obj_num(priv, regs, regs->tef.con);
}

static u8 mcp25xxfd_dump_get_tef_tail(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return regs->tefua / sizeof(struct mcp25xxfd_hw_tef_obj);
}

static u16 mcp25xxfd_dump_get_tef_obj_rel_addr(const struct mcp25xxfd_priv *priv, u8 n)
{
	return sizeof(struct mcp25xxfd_hw_tef_obj) * n;
}

static u16 mcp25xxfd_dump_get_tef_obj_addr(const struct mcp25xxfd_priv *priv, u8 n)
{
	return mcp25xxfd_dump_get_tef_obj_rel_addr(priv, n) + MCP25XXFD_RAM_START;
}

/* TX */

static u8 mcp25xxfd_dump_get_tx_obj_size(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return sizeof(struct mcp25xxfd_hw_tx_obj_can) -
		FIELD_SIZEOF(struct mcp25xxfd_hw_tx_obj_can, data) +
		mcp25xxfd_dump_get_fifo_size(priv, regs, regs->tx_fifo.con);
}

static u8 mcp25xxfd_dump_get_tx_obj_num(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return mcp25xxfd_dump_get_fifo_obj_num(priv, regs, regs->tx_fifo.con);
}

static u16 mcp25xxfd_dump_get_tx_obj_rel_addr(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, u8 n)
{
	return mcp25xxfd_dump_get_tef_obj_rel_addr(priv, mcp25xxfd_dump_get_tef_obj_num(priv, regs)) +
		mcp25xxfd_dump_get_tx_obj_size(priv, regs) * n;
}

static u16 mcp25xxfd_dump_get_tx_obj_addr(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, u8 n)
{
	return mcp25xxfd_dump_get_tx_obj_rel_addr(priv, regs, n) + MCP25XXFD_RAM_START;
}

static u8 mcp25xxfd_dump_get_tx_tail(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return (regs->fifo[MCP25XXFD_TX_FIFO].ua -
		mcp25xxfd_dump_get_tx_obj_rel_addr(priv, regs, 0)) /
		mcp25xxfd_dump_get_tx_obj_size(priv, regs);
}

static u8 mcp25xxfd_dump_get_tx_head(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return FIELD_GET(MCP25XXFD_CAN_FIFOSTA_FIFOCI_MASK, regs->fifo[MCP25XXFD_TX_FIFO].sta);
}

/* RX */

static u8 mcp25xxfd_dump_get_rx_obj_size(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return sizeof(struct mcp25xxfd_hw_rx_obj_can) -
		FIELD_SIZEOF(struct mcp25xxfd_hw_rx_obj_can, data) +
		mcp25xxfd_dump_get_fifo_size(priv, regs, regs->rx_fifo.con);
}

static u8 mcp25xxfd_dump_get_rx_obj_num(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return mcp25xxfd_dump_get_fifo_obj_num(priv, regs, regs->rx_fifo.con);
}

static u16 mcp25xxfd_dump_get_rx_obj_rel_addr(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, u8 n)
{
	return mcp25xxfd_dump_get_tx_obj_rel_addr(priv, regs, mcp25xxfd_dump_get_tx_obj_num(priv, regs)) +
		mcp25xxfd_dump_get_rx_obj_size(priv, regs) * n;
}

static u16 mcp25xxfd_dump_get_rx_obj_addr(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, u8 n)
{
	return mcp25xxfd_dump_get_rx_obj_rel_addr(priv, regs, n) + MCP25XXFD_RAM_START;
}

static u8 mcp25xxfd_dump_get_rx_tail(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return (regs->fifo[MCP25XXFD_RX_FIFO(0)].ua -
		mcp25xxfd_dump_get_rx_obj_rel_addr(priv, regs, 0)) /
		mcp25xxfd_dump_get_rx_obj_size(priv, regs);
}

static u8 mcp25xxfd_dump_get_rx_head(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs)
{
	return FIELD_GET(MCP25XXFD_CAN_FIFOSTA_FIFOCI_MASK, regs->fifo[MCP25XXFD_RX_FIFO(0)].sta);
}

/* dump TEF */

static void mcp25xxfd_dump_ram_tef_obj_one(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, const struct mcp25xxfd_hw_tef_obj *hw_tef_obj, u8 n)
{
	pr_info("TEF Object: 0x%02x (0x%03x)%s%s%s%s%s\n",
		n, mcp25xxfd_dump_get_tef_obj_addr(priv, n),
		mcp25xxfd_get_tef_head(priv) == n ? "  priv-HEAD" : "",
		mcp25xxfd_dump_get_tef_tail(priv, regs) == n ? "  chip-TAIL" : "",
		mcp25xxfd_get_tef_tail(priv) == n ? "  priv-TAIL" : "",
		(mcp25xxfd_dump_get_tef_tail(priv, regs) == n ?
		 ((regs->tef.sta & MCP25XXFD_CAN_TEFSTA_TEFFIF) ? "  chip-FIFO-full" :
		  !(regs->tef.sta & MCP25XXFD_CAN_TEFSTA_TEFNEIF) ? "  chip-FIFO-empty" : "") :
		 ("")),
		(mcp25xxfd_get_tef_head(priv) == mcp25xxfd_get_tef_tail(priv) &&
		 mcp25xxfd_get_tef_tail(priv) == n ?
		 (priv->tef.head == priv->tef.tail ? "  priv-FIFO-empty" : "  priv-FIFO-full") :
		 ("")));
	pr_info("%16s = 0x%08x\n", "id", hw_tef_obj->id);
	pr_info("%16s = 0x%08x\n", "flags", hw_tef_obj->flags);
	pr_info("%16s = 0x%08x\n", "ts", hw_tef_obj->ts);
	__dump_mask(hw_tef_obj->flags, MCP25XXFD_OBJ_FLAGS, SEQ, "0x%06lx", "Sequence");
	pr_info("\n");
}

static void mcp25xxfd_dump_ram_tef_obj(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, const struct mcp25xxfd_dump_ram *ram)
{
	int i;

	pr_info("\nTEF Overview:\n");
	pr_info("%16s =        0x%02x    0x%08x\n", "head (p)", mcp25xxfd_get_tef_head(priv), priv->tef.head);
	pr_info("%16s = 0x%02x   0x%02x    0x%08x\n", "tail (c/p)", mcp25xxfd_dump_get_tef_tail(priv, regs), mcp25xxfd_get_tef_tail(priv), priv->tef.tail);
	pr_info("\n");

	for (i = 0; i < mcp25xxfd_dump_get_tef_obj_num(priv, regs); i++) {
		const struct mcp25xxfd_hw_tef_obj *hw_tef_obj;
		u16 hw_tef_obj_rel_addr;

		hw_tef_obj_rel_addr = mcp25xxfd_dump_get_tef_obj_rel_addr(priv, i);

		hw_tef_obj = (const struct mcp25xxfd_hw_tef_obj *)&ram->ram[hw_tef_obj_rel_addr];
		mcp25xxfd_dump_ram_tef_obj_one(priv, regs, hw_tef_obj, i);
	}
}

/* dump TX */

static void mcp25xxfd_dump_ram_tx_obj_one(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, const struct mcp25xxfd_hw_tx_obj_canfd *hw_tx_obj, u8 n)
{
	pr_info("TX Object: 0x%02x (0x%03x)%s%s%s%s%s%s\n",
		n, mcp25xxfd_dump_get_tx_obj_addr(priv, regs, n),
		mcp25xxfd_dump_get_tx_head(priv, regs) == n ? "  chip-HEAD" : "",
		mcp25xxfd_get_tx_head(priv) == n ? "  priv-HEAD" : "",
		mcp25xxfd_dump_get_tx_tail(priv, regs) == n ? "  chip-TAIL" : "",
		mcp25xxfd_get_tx_tail(priv) == n ? "  priv-TAIL" : "",
		mcp25xxfd_dump_get_tx_tail(priv, regs) == n ?
		(!(regs->tx_fifo.sta & MCP25XXFD_CAN_FIFOSTA_TFNRFNIF) ? "  chip-FIFO-full" :
		 (regs->tx_fifo.sta & MCP25XXFD_CAN_FIFOSTA_TFERFFIF) ? "  chip-FIFO-empty" : "") :
		(""),
		(mcp25xxfd_get_tx_head(priv) == mcp25xxfd_get_tx_tail(priv) &&
		 mcp25xxfd_get_tx_tail(priv) == n ?
		 (priv->tx.head == priv->tx.tail ? "  priv-FIFO-empty" : "  priv-FIFO-full") :
		 ("")));
	pr_info("%16s = 0x%08x\n", "id", hw_tx_obj->id);
	pr_info("%16s = 0x%08x\n", "flags", hw_tx_obj->flags);
	__dump_mask(hw_tx_obj->flags, MCP25XXFD_OBJ_FLAGS, SEQ_MCP2517FD, "0x%06lx", "Sequence (MCP2517)");
	__dump_mask(hw_tx_obj->flags, MCP25XXFD_OBJ_FLAGS, SEQ_MCP2518FD, "0x%06lx", "Sequence (MCP2518)");
	mcp25xxfd_dump_ram_fifo_obj_data(priv, hw_tx_obj->data, FIELD_GET(MCP25XXFD_OBJ_FLAGS_DLC, hw_tx_obj->flags));
	pr_info("\n");
}

static void mcp25xxfd_dump_ram_tx_obj(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, const struct mcp25xxfd_dump_ram *ram)
{
	int i;

	pr_info("\nTX Overview:\n");
	pr_info("%16s = 0x%02x    0x%02x    0x%08x\n", "head (c/p)", mcp25xxfd_dump_get_tx_head(priv, regs), mcp25xxfd_get_tx_head(priv), priv->tx.head);
	pr_info("%16s = 0x%02x    0x%02x    0x%08x\n", "tail (c/p)", mcp25xxfd_dump_get_tx_tail(priv, regs), mcp25xxfd_get_tx_tail(priv), priv->tx.tail);
	pr_info("\n");

	for (i = 0; i < mcp25xxfd_dump_get_tx_obj_num(priv, regs); i++) {
		const struct mcp25xxfd_hw_tx_obj_canfd *hw_tx_obj;
		u16 hw_tx_obj_rel_addr;

		hw_tx_obj_rel_addr = mcp25xxfd_dump_get_tx_obj_rel_addr(priv, regs, i);
		hw_tx_obj = (const struct mcp25xxfd_hw_tx_obj_canfd *)&ram->ram[hw_tx_obj_rel_addr];

		mcp25xxfd_dump_ram_tx_obj_one(priv, regs, hw_tx_obj, i);
	}
}

/* dump RX */

static void mcp25xxfd_dump_ram_rx_obj_one(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, const struct mcp25xxfd_hw_rx_obj_canfd *hw_rx_obj, u8 n)
{
	pr_info("RX Object: 0x%02x (0x%03x)%s%s%s%s%s%s\n",
		n, mcp25xxfd_dump_get_rx_obj_addr(priv, regs, n),
		mcp25xxfd_dump_get_rx_head(priv, regs) == n ? "  chip-HEAD" : "",
		mcp25xxfd_get_rx_head(priv) == n ? "  priv-HEAD" : "",
		mcp25xxfd_dump_get_rx_tail(priv, regs) == n ? "  chip-TAIL" : "",
		mcp25xxfd_get_rx_tail(priv) == n ? "  priv-TAIL" : "",
		mcp25xxfd_dump_get_rx_tail(priv, regs) == n ?
		((regs->rx_fifo.sta & MCP25XXFD_CAN_FIFOSTA_TFERFFIF) ? "  chip-FIFO-full" :
		 !(regs->rx_fifo.sta & MCP25XXFD_CAN_FIFOSTA_TFNRFNIF) ? "  chip-FIFO-empty" : "") :
		(""),
		(mcp25xxfd_get_rx_head(priv) == mcp25xxfd_get_rx_tail(priv) &&
		 mcp25xxfd_get_rx_tail(priv) == n ?
		 (priv->rx.head == priv->rx.tail ? "  priv-FIFO-empty" : "  priv-FIFO-full") :
		 ("")));
	pr_info("%16s = 0x%08x\n", "id", hw_rx_obj->id);
	pr_info("%16s = 0x%08x\n", "flags", hw_rx_obj->flags);
	pr_info("%16s = 0x%08x\n", "ts", hw_rx_obj->ts);
	mcp25xxfd_dump_ram_fifo_obj_data(priv, hw_rx_obj->data, FIELD_GET(MCP25XXFD_OBJ_FLAGS_DLC, hw_rx_obj->flags));
	pr_info("\n");
}

static void mcp25xxfd_dump_ram_rx_obj(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, const struct mcp25xxfd_dump_ram *ram)
{
	int i;

	pr_info("\nRX Overview:\n");
	pr_info("%16s = 0x%02x    0x%02x    0x%08x\n", "head (c/p)", mcp25xxfd_dump_get_rx_head(priv, regs), mcp25xxfd_get_rx_head(priv), priv->rx.head);
	pr_info("%16s = 0x%02x    0x%02x    0x%08x\n", "tail (c/p)", mcp25xxfd_dump_get_rx_tail(priv, regs), mcp25xxfd_get_rx_tail(priv), priv->rx.tail);
	pr_info("\n");

	for (i = 0; i < mcp25xxfd_dump_get_rx_obj_num(priv, regs); i++) {
		const struct mcp25xxfd_hw_rx_obj_canfd *hw_rx_obj;
		u16 hw_rx_obj_rel_addr;

		hw_rx_obj_rel_addr = mcp25xxfd_dump_get_rx_obj_rel_addr(priv, regs, i);
		hw_rx_obj = (const struct mcp25xxfd_hw_rx_obj_canfd *)&ram->ram[hw_rx_obj_rel_addr];

		mcp25xxfd_dump_ram_rx_obj_one(priv, regs, hw_rx_obj, i);
	}
}

#undef __dump_mask
#undef __dump_bit

static void mcp25xxfd_dump_ram(const struct mcp25xxfd_priv *priv, const struct mcp25xxfd_dump_regs *regs, const struct mcp25xxfd_dump_ram *ram)
{
	netdev_info(priv->ndev, "----------------------- RAM dump ----------------------\n");
	mcp25xxfd_dump_ram_tef_obj(priv, regs, ram);
	mcp25xxfd_dump_ram_tx_obj(priv, regs, ram);
	mcp25xxfd_dump_ram_rx_obj(priv, regs, ram);
	netdev_info(priv->ndev, "------------------------- end -------------------------\n");
}

void mcp25xxfd_dump(struct mcp25xxfd_priv *priv)
{
	struct mcp25xxfd_dump_regs *regs = &priv->dump.regs;
	struct mcp25xxfd_dump_ram *ram = &priv->dump.ram;
	struct mcp25xxfd_dump_regs_mcp25xxfd *regs_mcp25xxfd = &priv->dump.regs_mcp25xxfd;
	int err;

	BUILD_BUG_ON(sizeof(struct mcp25xxfd_dump_regs) !=
		     MCP25XXFD_CAN_FIFOUA(31) - MCP25XXFD_CAN_CON + 4);

	err = regmap_bulk_read(priv->map, MCP25XXFD_CAN_CON,
			       regs, sizeof(*regs) / sizeof(u32));
	if (err)
		return;

	err = regmap_bulk_read(priv->map, MCP25XXFD_RAM_START,
			       ram, sizeof(*ram) / sizeof(u32));
	if (err)
		return;

	err = regmap_bulk_read(priv->map, MCP25XXFD_OSC,
			       regs_mcp25xxfd, sizeof(*regs_mcp25xxfd) / sizeof(u32));
	if (err)
		return;

	mcp25xxfd_dump_regs(priv, regs, regs_mcp25xxfd);
	mcp25xxfd_dump_ram(priv, regs, ram);
}
