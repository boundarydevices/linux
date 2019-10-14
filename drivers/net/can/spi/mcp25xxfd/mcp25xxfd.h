/* SPDX-License-Identifier: GPL-2.0
 *
 * mcp25xxfd - Microchip MCP25xxFD Family CAN controller driver
 *
 * Copyright (c) 2019 Pengutronix,
 *                    Marc Kleine-Budde <kernel@pengutronix.de>
 * Copyright (c) 2019 Martin Sperl <kernel@martin.sperl.org>
 */

#ifndef _MCP25XXFD_H
#define _MCP25XXFD_H

#include <linux/can/core.h>
#include <linux/can/dev.h>
#include <linux/can/rx-offload.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

/* MPC25xx registers */

/* CAN FD Controller Module SFR */
#define MCP25XXFD_CAN_CON 0x00
#define MCP25XXFD_CAN_CON_TXBWS_MASK GENMASK(31, 28)
#define MCP25XXFD_CAN_CON_ABAT BIT(27)
#define MCP25XXFD_CAN_CON_REQOP_MASK GENMASK(26, 24)
#define MCP25XXFD_CAN_CON_MODE_MIXED 0
#define MCP25XXFD_CAN_CON_MODE_SLEEP 1
#define MCP25XXFD_CAN_CON_MODE_INT_LOOPBACK 2
#define MCP25XXFD_CAN_CON_MODE_LISTENONLY 3
#define MCP25XXFD_CAN_CON_MODE_CONFIG 4
#define MCP25XXFD_CAN_CON_MODE_EXT_LOOPBACK 5
#define MCP25XXFD_CAN_CON_MODE_CAN2_0 6
#define MCP25XXFD_CAN_CON_MODE_RESTRICTED 7
#define MCP25XXFD_CAN_CON_OPMOD_MASK GENMASK(23, 21)
#define MCP25XXFD_CAN_CON_TXQEN BIT(20)
#define MCP25XXFD_CAN_CON_STEF BIT(19)
#define MCP25XXFD_CAN_CON_SERR2LOM BIT(18)
#define MCP25XXFD_CAN_CON_ESIGM BIT(17)
#define MCP25XXFD_CAN_CON_RTXAT BIT(16)
#define MCP25XXFD_CAN_CON_BRSDIS BIT(12)
#define MCP25XXFD_CAN_CON_BUSY BIT(11)
#define MCP25XXFD_CAN_CON_WFT_MASK GENMASK(10, 9)
#define MCP25XXFD_CAN_CON_WFT_T00FILTER 0x0
#define MCP25XXFD_CAN_CON_WFT_T01FILTER 0x1
#define MCP25XXFD_CAN_CON_WFT_T10FILTER 0x2
#define MCP25XXFD_CAN_CON_WFT_T11FILTER 0x3
#define MCP25XXFD_CAN_CON_WAKFIL BIT(8)
#define MCP25XXFD_CAN_CON_PXEDIS BIT(6)
#define MCP25XXFD_CAN_CON_ISOCRCEN BIT(5)
#define MCP25XXFD_CAN_CON_DNCNT_MASK GENMASK(4, 0)

#define MCP25XXFD_CAN_NBTCFG 0x04
#define MCP25XXFD_CAN_NBTCFG_BRP_MASK GENMASK(31, 24)
#define MCP25XXFD_CAN_NBTCFG_TSEG1_MASK GENMASK(23, 16)
#define MCP25XXFD_CAN_NBTCFG_TSEG2_MASK GENMASK(14, 8)
#define MCP25XXFD_CAN_NBTCFG_SJW_MASK GENMASK(6, 0)

#define MCP25XXFD_CAN_DBTCFG 0x08
#define MCP25XXFD_CAN_DBTCFG_BRP_MASK GENMASK(31, 24)
#define MCP25XXFD_CAN_DBTCFG_TSEG1_MASK GENMASK(20, 16)
#define MCP25XXFD_CAN_DBTCFG_TSEG2_MASK GENMASK(11, 8)
#define MCP25XXFD_CAN_DBTCFG_SJW_MASK GENMASK(3, 0)

#define MCP25XXFD_CAN_TDC 0x0c
#define MCP25XXFD_CAN_TDC_EDGFLTEN BIT(25)
#define MCP25XXFD_CAN_TDC_SID11EN BIT(24)
#define MCP25XXFD_CAN_TDC_TDCMOD_MASK GENMASK(17, 6)
#define MCP25XXFD_CAN_TDC_TDCMOD_AUTO 2
#define MCP25XXFD_CAN_TDC_TDCMOD_MANUAL 1
#define MCP25XXFD_CAN_TDC_TDCMOD_DISABLED 0
#define MCP25XXFD_CAN_TDC_TDCO_MASK GENMASK(14, 8)
#define MCP25XXFD_CAN_TDC_TDCV_MASK GENMASK(5, 0)

#define MCP25XXFD_CAN_TBC 0x10

#define MCP25XXFD_CAN_TSCON 0x14
#define MCP25XXFD_CAN_TSCON_TSRES BIT(18)
#define MCP25XXFD_CAN_TSCON_TSEOF BIT(17)
#define MCP25XXFD_CAN_TSCON_TBCEN BIT(16)
#define MCP25XXFD_CAN_TSCON_TBCPRE_MASK GENMASK(9, 0)

#define MCP25XXFD_CAN_VEC 0x18
#define MCP25XXFD_CAN_VEC_RXCODE_MASK GENMASK(30, 24)
#define MCP25XXFD_CAN_VEC_TXCODE_MASK GENMASK(22, 16)
#define MCP25XXFD_CAN_VEC_FILHIT_MASK GENMASK(12, 8)
#define MCP25XXFD_CAN_VEC_ICODE_MASK GENMASK(6, 0)

#define MCP25XXFD_CAN_INT 0x1c
#define MCP25XXFD_CAN_INT_IF_MASK GENMASK(15, 0)
#define MCP25XXFD_CAN_INT_IE_MASK GENMASK(31, 16)
#define MCP25XXFD_CAN_INT_IVMIE BIT(31)
#define MCP25XXFD_CAN_INT_WAKIE BIT(30)
#define MCP25XXFD_CAN_INT_CERRIE BIT(29)
#define MCP25XXFD_CAN_INT_SERRIE BIT(28)
#define MCP25XXFD_CAN_INT_RXOVIE BIT(27)
#define MCP25XXFD_CAN_INT_TXATIE BIT(26)
#define MCP25XXFD_CAN_INT_SPICRCIE BIT(25)
#define MCP25XXFD_CAN_INT_ECCIE BIT(24)
#define MCP25XXFD_CAN_INT_TEFIE BIT(20)
#define MCP25XXFD_CAN_INT_MODIE BIT(19)
#define MCP25XXFD_CAN_INT_TBCIE BIT(18)
#define MCP25XXFD_CAN_INT_RXIE BIT(17)
#define MCP25XXFD_CAN_INT_TXIE BIT(16)
#define MCP25XXFD_CAN_INT_IVMIF BIT(15)
#define MCP25XXFD_CAN_INT_WAKIF BIT(14)
#define MCP25XXFD_CAN_INT_CERRIF BIT(13)
#define MCP25XXFD_CAN_INT_SERRIF BIT(12)
#define MCP25XXFD_CAN_INT_RXOVIF BIT(11)
#define MCP25XXFD_CAN_INT_TXATIF BIT(10)
#define MCP25XXFD_CAN_INT_SPICRCIF BIT(9)
#define MCP25XXFD_CAN_INT_ECCIF BIT(8)
#define MCP25XXFD_CAN_INT_TEFIF BIT(4)
#define MCP25XXFD_CAN_INT_MODIF BIT(3)
#define MCP25XXFD_CAN_INT_TBCIF BIT(2)
#define MCP25XXFD_CAN_INT_RXIF BIT(1)
#define MCP25XXFD_CAN_INT_TXIF BIT(0)
/* These IRQ flags must be cleared by SW in the CAN_INT register */
#define MCP25XXFD_CAN_INT_IF_CLEARABLE_MASK \
	(MCP25XXFD_CAN_INT_IVMIF | MCP25XXFD_CAN_INT_WAKIF | \
	 MCP25XXFD_CAN_INT_CERRIF |  MCP25XXFD_CAN_INT_SERRIF | \
	 MCP25XXFD_CAN_INT_MODIF)

#define MCP25XXFD_CAN_RXIF 0x20
#define MCP25XXFD_CAN_TXIF 0x24
#define MCP25XXFD_CAN_RXOVIF 0x28
#define MCP25XXFD_CAN_TXATIF 0x2c
#define MCP25XXFD_CAN_TXREQ 0x30

#define MCP25XXFD_CAN_TREC 0x34
#define MCP25XXFD_CAN_TREC_TXBO BIT(21)
#define MCP25XXFD_CAN_TREC_TXBP BIT(20)
#define MCP25XXFD_CAN_TREC_RXBP BIT(19)
#define MCP25XXFD_CAN_TREC_TXWARN BIT(18)
#define MCP25XXFD_CAN_TREC_RXWARN BIT(17)
#define MCP25XXFD_CAN_TREC_EWARN BIT(16)
#define MCP25XXFD_CAN_TREC_TEC_MASK GENMASK(15, 8)
#define MCP25XXFD_CAN_TREC_REC_MASK GENMASK(7, 0)

#define MCP25XXFD_CAN_BDIAG0 0x38
#define MCP25XXFD_CAN_BDIAG0_DTERRCNT_MASK GENMASK(31, 24)
#define MCP25XXFD_CAN_BDIAG0_DRERRCNT_MASK GENMASK(23, 16)
#define MCP25XXFD_CAN_BDIAG0_NTERRCNT_MASK GENMASK(15, 8)
#define MCP25XXFD_CAN_BDIAG0_NRERRCNT_MASK GENMASK(7, 0)

#define MCP25XXFD_CAN_BDIAG1 0x3c
#define MCP25XXFD_CAN_BDIAG1_DLCMM BIT(31)
#define MCP25XXFD_CAN_BDIAG1_ESI BIT(30)
#define MCP25XXFD_CAN_BDIAG1_DCRCERR BIT(29)
#define MCP25XXFD_CAN_BDIAG1_DSTUFERR BIT(28)
#define MCP25XXFD_CAN_BDIAG1_DFORMERR BIT(27)
#define MCP25XXFD_CAN_BDIAG1_DBIT1ERR BIT(25)
#define MCP25XXFD_CAN_BDIAG1_DBIT0ERR BIT(24)
#define MCP25XXFD_CAN_BDIAG1_TXBOERR BIT(23)
#define MCP25XXFD_CAN_BDIAG1_NCRCERR BIT(21)
#define MCP25XXFD_CAN_BDIAG1_NSTUFERR BIT(20)
#define MCP25XXFD_CAN_BDIAG1_NFORMERR BIT(19)
#define MCP25XXFD_CAN_BDIAG1_NACKERR BIT(18)
#define MCP25XXFD_CAN_BDIAG1_NBIT1ERR BIT(17)
#define MCP25XXFD_CAN_BDIAG1_NBIT0ERR BIT(16)
#define MCP25XXFD_CAN_BDIAG1_BERR_MASK \
	(MCP25XXFD_CAN_BDIAG1_DLCMM | MCP25XXFD_CAN_BDIAG1_ESI | \
	 MCP25XXFD_CAN_BDIAG1_DCRCERR | MCP25XXFD_CAN_BDIAG1_DSTUFERR | \
	 MCP25XXFD_CAN_BDIAG1_DFORMERR | MCP25XXFD_CAN_BDIAG1_DBIT1ERR | \
	 MCP25XXFD_CAN_BDIAG1_DBIT0ERR | MCP25XXFD_CAN_BDIAG1_TXBOERR | \
	 MCP25XXFD_CAN_BDIAG1_NCRCERR | MCP25XXFD_CAN_BDIAG1_NSTUFERR | \
	 MCP25XXFD_CAN_BDIAG1_NFORMERR | MCP25XXFD_CAN_BDIAG1_NACKERR | \
	 MCP25XXFD_CAN_BDIAG1_NBIT1ERR | MCP25XXFD_CAN_BDIAG1_NBIT0ERR)
#define MCP25XXFD_CAN_BDIAG1_EFMSGCNT_MASK GENMASK(15, 0)

#define MCP25XXFD_CAN_TEFCON 0x40
#define MCP25XXFD_CAN_TEFCON_FSIZE_MASK GENMASK(28, 24)
#define MCP25XXFD_CAN_TEFCON_FRESET BIT(10)
#define MCP25XXFD_CAN_TEFCON_UINC BIT(8)
#define MCP25XXFD_CAN_TEFCON_TEFTSEN BIT(5)
#define MCP25XXFD_CAN_TEFCON_TEFOVIE BIT(3)
#define MCP25XXFD_CAN_TEFCON_TEFFIE BIT(2)
#define MCP25XXFD_CAN_TEFCON_TEFHIE BIT(1)
#define MCP25XXFD_CAN_TEFCON_TEFNEIE BIT(0)

#define MCP25XXFD_CAN_TEFSTA 0x44
#define MCP25XXFD_CAN_TEFSTA_TEFOVIF BIT(3)
#define MCP25XXFD_CAN_TEFSTA_TEFFIF BIT(2)
#define MCP25XXFD_CAN_TEFSTA_TEFHIF BIT(1)
#define MCP25XXFD_CAN_TEFSTA_TEFNEIF BIT(0)

#define MCP25XXFD_CAN_TEFUA 0x48

#define MCP25XXFD_CAN_TXQCON 0x50
#define MCP25XXFD_CAN_TXQCON_PLSIZE_MASK GENMASK(31, 29)
#define MCP25XXFD_CAN_TXQCON_PLSIZE_8 0
#define MCP25XXFD_CAN_TXQCON_PLSIZE_12 1
#define MCP25XXFD_CAN_TXQCON_PLSIZE_16 2
#define MCP25XXFD_CAN_TXQCON_PLSIZE_20 3
#define MCP25XXFD_CAN_TXQCON_PLSIZE_24 4
#define MCP25XXFD_CAN_TXQCON_PLSIZE_32 5
#define MCP25XXFD_CAN_TXQCON_PLSIZE_48 6
#define MCP25XXFD_CAN_TXQCON_PLSIZE_64 7
#define MCP25XXFD_CAN_TXQCON_FSIZE_MASK GENMASK(28, 24)
#define MCP25XXFD_CAN_TXQCON_TXAT_UNLIMITED 3
#define MCP25XXFD_CAN_TXQCON_TXAT_THREE_SHOT 1
#define MCP25XXFD_CAN_TXQCON_TXAT_ONE_SHOT 0
#define MCP25XXFD_CAN_TXQCON_TXAT_MASK GENMASK(22, 21)
#define MCP25XXFD_CAN_TXQCON_TXPRI_MASK GENMASK(20, 16)
#define MCP25XXFD_CAN_TXQCON_FRESET BIT(10)
#define MCP25XXFD_CAN_TXQCON_TXREQ BIT(9)
#define MCP25XXFD_CAN_TXQCON_UINC BIT(8)
#define MCP25XXFD_CAN_TXQCON_TXEN BIT(7)
#define MCP25XXFD_CAN_TXQCON_TXATIE BIT(4)
#define MCP25XXFD_CAN_TXQCON_TXQEIE BIT(2)
#define MCP25XXFD_CAN_TXQCON_TXQNIE BIT(0)

#define MCP25XXFD_CAN_TXQSTA 0x54
#define MCP25XXFD_CAN_TXQSTA_TXQCI_MASK GENMASK(12, 8)
#define MCP25XXFD_CAN_TXQSTA_TXABT BIT(7)
#define MCP25XXFD_CAN_TXQSTA_TXLARB BIT(6)
#define MCP25XXFD_CAN_TXQSTA_TXERR BIT(5)
#define MCP25XXFD_CAN_TXQSTA_TXATIF BIT(4)
#define MCP25XXFD_CAN_TXQSTA_TXQEIF BIT(2)
#define MCP25XXFD_CAN_TXQSTA_TXQNIF BIT(0)

#define MCP25XXFD_CAN_TXQUA 0x58

#define MCP25XXFD_CAN_FIFOCON(x) (0x50 + 0xc * (x))
#define MCP25XXFD_CAN_FIFOCON_PLSIZE_MASK GENMASK(31, 29)
#define MCP25XXFD_CAN_FIFOCON_PLSIZE_8 0
#define MCP25XXFD_CAN_FIFOCON_PLSIZE_12 1
#define MCP25XXFD_CAN_FIFOCON_PLSIZE_16 2
#define MCP25XXFD_CAN_FIFOCON_PLSIZE_20 3
#define MCP25XXFD_CAN_FIFOCON_PLSIZE_24 4
#define MCP25XXFD_CAN_FIFOCON_PLSIZE_32 5
#define MCP25XXFD_CAN_FIFOCON_PLSIZE_48 6
#define MCP25XXFD_CAN_FIFOCON_PLSIZE_64 7
#define MCP25XXFD_CAN_FIFOCON_FSIZE_MASK GENMASK(28, 24)
#define MCP25XXFD_CAN_FIFOCON_TXAT_MASK GENMASK(22, 21)
#define MCP25XXFD_CAN_FIFOCON_TXAT_ONE_SHOT 0
#define MCP25XXFD_CAN_FIFOCON_TXAT_THREE_SHOT 1
#define MCP25XXFD_CAN_FIFOCON_TXAT_UNLIMITED 3
#define MCP25XXFD_CAN_FIFOCON_TXPRI_MASK GENMASK(20, 16)
#define MCP25XXFD_CAN_FIFOCON_FRESET BIT(10)
#define MCP25XXFD_CAN_FIFOCON_TXREQ BIT(9)
#define MCP25XXFD_CAN_FIFOCON_UINC BIT(8)
#define MCP25XXFD_CAN_FIFOCON_TXEN BIT(7)
#define MCP25XXFD_CAN_FIFOCON_RTREN BIT(6)
#define MCP25XXFD_CAN_FIFOCON_RXTSEN BIT(5)
#define MCP25XXFD_CAN_FIFOCON_TXATIE BIT(4)
#define MCP25XXFD_CAN_FIFOCON_RXOVIE BIT(3)
#define MCP25XXFD_CAN_FIFOCON_TFERFFIE BIT(2)
#define MCP25XXFD_CAN_FIFOCON_TFHRFHIE BIT(1)
#define MCP25XXFD_CAN_FIFOCON_TFNRFNIE BIT(0)

#define MCP25XXFD_CAN_FIFOSTA(x) (0x54 + 0xc * (x))
#define MCP25XXFD_CAN_FIFOSTA_FIFOCI_MASK GENMASK(12, 8)
#define MCP25XXFD_CAN_FIFOSTA_TXABT BIT(7)
#define MCP25XXFD_CAN_FIFOSTA_TXLARB BIT(6)
#define MCP25XXFD_CAN_FIFOSTA_TXERR BIT(5)
#define MCP25XXFD_CAN_FIFOSTA_TXATIF BIT(4)
#define MCP25XXFD_CAN_FIFOSTA_RXOVIF BIT(3)
#define MCP25XXFD_CAN_FIFOSTA_TFERFFIF BIT(2)
#define MCP25XXFD_CAN_FIFOSTA_TFHRFHIF BIT(1)
#define MCP25XXFD_CAN_FIFOSTA_TFNRFNIF BIT(0)

#define MCP25XXFD_CAN_FIFOUA(x) (0x58 + 0xc * (x))

#define MCP25XXFD_CAN_FLTCON(x) (0x1d0 + (x))
#define MCP25XXFD_CAN_FLTCON_FLTEN3 BIT(31)
#define MCP25XXFD_CAN_FLTCON_F3BP_MASK GENMASK(28, 24)
#define MCP25XXFD_CAN_FLTCON_FLTEN2 BIT(23)
#define MCP25XXFD_CAN_FLTCON_F2BP_MASK GENMASK(20, 16)
#define MCP25XXFD_CAN_FLTCON_FLTEN1 BIT(15)
#define MCP25XXFD_CAN_FLTCON_F1BP_MASK GENMASK(12, 8)
#define MCP25XXFD_CAN_FLTCON_FLTEN0 BIT(7)
#define MCP25XXFD_CAN_FLTCON_F0BP_MASK GENMASK(4, 0)

#define MCP25XXFD_CAN_FLTOBJ(x) (0x1f0 + 8 * (x))
#define MCP25XXFD_CAN_FLTOBJ_EXIDE BIT(30)
#define MCP25XXFD_CAN_FLTOBJ_SID11 BIT(29)
#define MCP25XXFD_CAN_FLTOBJ_EID_MASK GENMASK(28. 11)
#define MCP25XXFD_CAN_FLTOBJ_SID_MASK GENMASK(10, 0)

#define MCP25XXFD_CAN_FLTMASK(x) (0x1f4 + 8 * (x))
#define MCP25XXFD_CAN_MASK_MIDE BIT(30)
#define MCP25XXFD_CAN_MASK_MSID11 BIT(29)
#define MCP25XXFD_CAN_MASK_MEID_MASK GENMASK(28, 11)
#define MCP25XXFD_CAN_MASK_MSID_MASK GENMASK(10, 0)

/* RAM */
#define MCP25XXFD_RAM_SIZE 2048
#define MCP25XXFD_RAM_START 0x400

/* Message Object */
#define MCP25XXFD_OBJ_ID_SID11 BIT(29)
#define MCP25XXFD_OBJ_ID_EID_MASK GENMASK(28, 11)
#define MCP25XXFD_OBJ_ID_SID_MASK GENMASK(10, 0)
#define MCP25XXFD_OBJ_FLAGS_SEQ_MCP2518FD_MASK GENMASK(31, 9)
#define MCP25XXFD_OBJ_FLAGS_SEQ_MCP2517FD_MASK GENMASK(15, 9)
#define MCP25XXFD_OBJ_FLAGS_SEQ_MASK MCP25XXFD_OBJ_FLAGS_SEQ_MCP2518FD_MASK
#define MCP25XXFD_OBJ_FLAGS_ESI BIT(8)
#define MCP25XXFD_OBJ_FLAGS_FDF BIT(7)
#define MCP25XXFD_OBJ_FLAGS_BRS BIT(6)
#define MCP25XXFD_OBJ_FLAGS_RTR BIT(5)
#define MCP25XXFD_OBJ_FLAGS_IDE BIT(4)
#define MCP25XXFD_OBJ_FLAGS_DLC GENMASK(3, 0)

#define MCP25XXFD_CAN_FRAME_EFF_SID_MASK GENMASK(28, 18)
#define MCP25XXFD_CAN_FRAME_EFF_EID_MASK GENMASK(17, 0)

/* MCP2517/18FD SFR */
#define MCP25XXFD_OSC 0xe00
#define MCP25XXFD_OSC_SCLKRDY BIT(12)
#define MCP25XXFD_OSC_OSCRDY BIT(10)
#define MCP25XXFD_OSC_PLLRDY BIT(8)
#define MCP25XXFD_OSC_CLKODIV_10 3
#define MCP25XXFD_OSC_CLKODIV_4 2
#define MCP25XXFD_OSC_CLKODIV_2 1
#define MCP25XXFD_OSC_CLKODIV_1 0
#define MCP25XXFD_OSC_CLKODIV_MASK GENMASK(6, 5)
#define MCP25XXFD_OSC_SCLKDIV BIT(4)
#define MCP25XXFD_OSC_LPMEN BIT(3)	/* MCP2518FD only */
#define MCP25XXFD_OSC_OSCDIS BIT(2)
#define MCP25XXFD_OSC_PLLEN BIT(0)

#define MCP25XXFD_IOCON 0xe04
#define MCP25XXFD_IOCON_INTOD BIT(30)
#define MCP25XXFD_IOCON_SOF BIT(29)
#define MCP25XXFD_IOCON_TXCANOD BIT(28)
#define MCP25XXFD_IOCON_PM1 BIT(25)
#define MCP25XXFD_IOCON_PM0 BIT(24)
#define MCP25XXFD_IOCON_GPIO1 BIT(17)
#define MCP25XXFD_IOCON_GPIO0 BIT(16)
#define MCP25XXFD_IOCON_LAT1 BIT(9)
#define MCP25XXFD_IOCON_LAT0 BIT(8)
#define MCP25XXFD_IOCON_XSTBYEN BIT(6)
#define MCP25XXFD_IOCON_TRIS1 BIT(1)
#define MCP25XXFD_IOCON_TRIS0 BIT(0)

#define MCP25XXFD_CRC 0xe08
#define MCP25XXFD_CRC_FERRIE BIT(25)
#define MCP25XXFD_CRC_CRCERRIE BIT(24)
#define MCP25XXFD_CRC_FERRIF BIT(17)
#define MCP25XXFD_CRC_CRCERRIF BIT(16)
#define MCP25XXFD_CRC_IF_MASK GENMASK(17, 16)
#define MCP25XXFD_CRC_MASK GENMASK(15, 0)

#define MCP25XXFD_ECCCON 0xe0c
#define MCP25XXFD_ECCCON_PARITY_MASK GENMASK(14, 8)
#define MCP25XXFD_ECCCON_DEDIE BIT(2)
#define MCP25XXFD_ECCCON_SECIE BIT(1)
#define MCP25XXFD_ECCCON_ECCEN BIT(0)

#define MCP25XXFD_ECCSTAT 0xe10
#define MCP25XXFD_ECCSTAT_ERRADDR_MASK GENMASK(27, 16)
#define MCP25XXFD_ECCSTAT_IF_MASK GENMASK(2, 1)
#define MCP25XXFD_ECCSTAT_DEDIF BIT(2)
#define MCP25XXFD_ECCSTAT_SECIF BIT(1)

#define MCP25XXFD_DEVID 0xe14	/* MCP2518FD only */
#define MCP25XXFD_DEVID_ID_MASK GENMASK(7, 4)
#define MCP25XXFD_DEVID_REV_MASK GENMASK(3, 0)

/* number of TX FIFO objects, depending on CAN mode
 *
 * FIFO setup: tef: 8*12 bytes = 96 bytes, tx: 8*16 bytes = 128, rx: 32*20 bytes =  640 bytes, free: 1184 bytes.
 * FIFO setup: tef: 4*12 bytes = 48 bytes, tx: 4*72 bytes = 288, rx: 22*76 bytes = 1672 bytes, free:   40 bytes.
 */
#define MCP25XXFD_TX_OBJ_NUM_CAN 8
#define MCP25XXFD_TX_OBJ_NUM_CANFD 4

#if MCP25XXFD_TX_OBJ_NUM_CAN > MCP25XXFD_TX_OBJ_NUM_CANFD
#define MCP25XXFD_TX_OBJ_NUM_MAX MCP25XXFD_TX_OBJ_NUM_CAN
#else
#define MCP25XXFD_TX_OBJ_NUM_MAX MCP25XXFD_TX_OBJ_NUM_CANFD
#endif

/* The actual number of RX objects is calculated in
 * mcp25xxfd_chip_fifo_compute(), but we allocate memory
 * beforehand.
 */
#define MCP25XXFD_RX_OBJ_NUM_CAN 32
#define MCP25XXFD_RX_OBJ_NUM_CANFD 22

#define MCP25XXFD_NAPI_WEIGHT 32
#define MCP25XXFD_TX_FIFO 1
#define MCP25XXFD_RX_FIFO(x) (MCP25XXFD_TX_FIFO + 1 + (x))
#define MCP25XXFD_RX_FIFO_NUM (1)

/* SPI commands */
#define MCP25XXFD_INSTRUCTION_RESET 0x0000
#define MCP25XXFD_INSTRUCTION_WRITE 0x2000
#define MCP25XXFD_INSTRUCTION_READ 0x3000
#define MCP25XXFD_INSTRUCTION_WRITE_CRC 0xa000
#define MCP25XXFD_INSTRUCTION_READ_CRC 0xb000
#define MCP25XXFD_INSTRUCTION_WRITE_SAVE 0xc000

struct mcp25xxfd_dump_regs_fifo {
	u32 con;
	u32 sta;
	u32 ua;
};

struct mcp25xxfd_dump_regs {
	u32 con;
	u32 nbtcfg;
	u32 dbtcfg;
	u32 tdc;
	u32 tbc;
	u32 tscon;
	u32 vec;
	u32 intf;
	u32 rxif;
	u32 txif;
	u32 rxovif;
	u32 txatif;
	u32 txreq;
	u32 trec;
	u32 bdiag0;
	u32 bdiag1;
	union {
		struct {
			u32 tefcon;
			u32 tefsta;
			u32 tefua;
		};
		struct mcp25xxfd_dump_regs_fifo tef;
	};
	u32 reserved0;
	union {
		struct {
			struct mcp25xxfd_dump_regs_fifo txq;
			struct mcp25xxfd_dump_regs_fifo tx_fifo;
			struct mcp25xxfd_dump_regs_fifo rx_fifo;
		};
		struct mcp25xxfd_dump_regs_fifo fifo[32];
	};
};

struct mcp25xxfd_dump_ram {
	u8 ram[MCP25XXFD_RAM_SIZE];
};

struct mcp25xxfd_dump_regs_mcp25xxfd {
	u32 osc;
	u32 iocon;
	u32 crc;
	u32 ecccon;
	u32 eccstat;
	u32 devid;		/* MCP2518FD only */
};

struct mcp25xxfd_dump {
	struct mcp25xxfd_dump_regs regs;
	struct mcp25xxfd_dump_ram ram;
	struct mcp25xxfd_dump_regs_mcp25xxfd regs_mcp25xxfd;
};

struct mcp25xxfd_hw_tef_obj {
	u32 id;
	u32 flags;
	u32 ts;
};

/* The tx_obj_raw version is used in spi async, i.e. without
 * regmap. We have to take care of endianness ourselves.
 */
struct mcp25xxfd_hw_tx_obj_raw {
	__le32 id;
	__le32 flags;
	u8 data[FIELD_SIZEOF(struct canfd_frame, data)];
};

struct mcp25xxfd_hw_tx_obj_can {
	u32 id;
	u32 flags;
	u8 data[FIELD_SIZEOF(struct can_frame, data)];
};

struct mcp25xxfd_hw_tx_obj_canfd {
	u32 id;
	u32 flags;
	u8 data[FIELD_SIZEOF(struct canfd_frame, data)];
};

struct mcp25xxfd_hw_rx_obj_can {
	u32 id;
	u32 flags;
	u32 ts;
	u8 data[FIELD_SIZEOF(struct can_frame, data)];
};

struct mcp25xxfd_hw_rx_obj_canfd {
	u32 id;
	u32 flags;
	u32 ts;
	u8 data[FIELD_SIZEOF(struct canfd_frame, data)];
};

struct __packed mcp25xxfd_tx_obj_load_buf {
	__be16 cmd;
	struct mcp25xxfd_hw_tx_obj_raw hw_tx_obj;
} ____cacheline_aligned;

struct __packed mcp25xxfd_reg_write_buf {
	__be16 cmd;
	u8 data[4];
} ____cacheline_aligned;

struct mcp25xxfd_tef_ring {
	unsigned int head;
	unsigned int tail;

	/* u8 obj_num equals tx_ring->obj_num */
	/* u8 obj_size equals sizeof(struct mcp25xxfd_hw_tef_obj) */
};

struct mcp25xxfd_tx_obj {
	struct {
		struct spi_message msg;
		struct spi_transfer xfer;
		struct mcp25xxfd_tx_obj_load_buf buf;
	} load;

	struct {
		struct spi_message msg;
		struct spi_transfer xfer;
		struct mcp25xxfd_reg_write_buf buf;
	} trigger;
};

struct mcp25xxfd_tx_ring {
	unsigned int head;
	unsigned int tail;

	u8 obj_num;
	u8 obj_size;

	struct mcp25xxfd_tx_obj obj[MCP25XXFD_TX_OBJ_NUM_MAX];
};

struct mcp25xxfd_rx_ring {
	unsigned int head;
	unsigned int tail;

	u8 obj_num;
	u8 obj_size;

	struct mcp25xxfd_hw_rx_obj_canfd obj[MCP25XXFD_RX_OBJ_NUM_CANFD];
};

struct mcp25xxfd_regs_status {
	u32 intf;
};

enum mcp25xxfd_model {
	CAN_MCP2517FD = 0x2517,
	CAN_MCP2518FD = 0x2518,
	CAN_MCP25XXFD = 0xffff,		/* autodetect model */
};

struct mcp25xxfd_priv {
	struct can_priv can;
	struct can_rx_offload offload;
	struct net_device *ndev;

	struct regmap *map;
	struct spi_device *spi;

	struct mcp25xxfd_tef_ring tef;
	struct mcp25xxfd_tx_ring tx;
	struct mcp25xxfd_rx_ring rx;

	u32 intf;

	struct mcp25xxfd_reg_write_buf update_bits_buf;

	struct clk *clk;
	struct regulator *reg_vdd;
	struct regulator *reg_xceiver;

	enum mcp25xxfd_model model;
};

static inline u8 mcp25xxfd_first_byte_set(u32 mask)
{
	return (mask & 0x0000ffff) ?
		((mask & 0x000000ff) ? 0 : 1) :
		((mask & 0x00ff0000) ? 2 : 3);
}

static inline u8 mcp25xxfd_last_byte_set(u32 mask)
{
	return (mask & 0xffff0000) ?
		((mask & 0xff000000) ? 3 : 2) :
		((mask & 0x0000ff00) ? 1 : 0);
}

static inline __be16 mcp25xxfd_cmd_reset(void)
{
	return cpu_to_be16(MCP25XXFD_INSTRUCTION_RESET);
}

static inline __be16 mcp25xxfd_cmd_read(u16 addr)
{
	return cpu_to_be16(MCP25XXFD_INSTRUCTION_READ | addr);
}

static inline __be16 mcp25xxfd_cmd_write(u16 addr)
{
	return cpu_to_be16(MCP25XXFD_INSTRUCTION_WRITE | addr);
}

static inline u16
mcp25xxfd_get_tef_obj_rel_addr(const struct mcp25xxfd_priv *priv, u8 n)
{
	return sizeof(struct mcp25xxfd_hw_tef_obj) * n;
}

static inline u16
mcp25xxfd_get_tef_obj_addr(const struct mcp25xxfd_priv *priv, u8 n)
{
	return mcp25xxfd_get_tef_obj_rel_addr(priv, n) + MCP25XXFD_RAM_START;
}

static inline u16
mcp25xxfd_get_tx_obj_rel_addr(const struct mcp25xxfd_priv *priv, u8 n)
{
	return mcp25xxfd_get_tef_obj_rel_addr(priv, priv->tx.obj_num) +
		priv->tx.obj_size * n;
}

static inline u16
mcp25xxfd_get_tx_obj_addr(const struct mcp25xxfd_priv *priv, u8 n)
{
	return mcp25xxfd_get_tx_obj_rel_addr(priv, n) + MCP25XXFD_RAM_START;
}

static inline u16
mcp25xxfd_get_rx_obj_rel_addr(const struct mcp25xxfd_priv *priv, u8 n)
{
	return mcp25xxfd_get_tx_obj_rel_addr(priv, priv->tx.obj_num) +
		priv->rx.obj_size * n;
}

static inline u16
mcp25xxfd_get_rx_obj_addr(const struct mcp25xxfd_priv *priv, u8 n)
{
	return mcp25xxfd_get_rx_obj_rel_addr(priv, n) + MCP25XXFD_RAM_START;
}

static inline u8 mcp25xxfd_get_tef_head(const struct mcp25xxfd_priv *priv)
{
	return priv->tef.head & (priv->tx.obj_num - 1);
}

static inline u8 mcp25xxfd_get_tef_tail(const struct mcp25xxfd_priv *priv)
{
	return priv->tef.tail & (priv->tx.obj_num - 1);
}

static inline u8 mcp25xxfd_get_tef_len(const struct mcp25xxfd_priv *priv)
{
	return priv->tef.head - priv->tef.tail;
}

static inline u8 mcp25xxfd_get_tef_linear_len(const struct mcp25xxfd_priv *priv)
{
	u8 len;

	len = mcp25xxfd_get_tef_len(priv);

	return min_t(u8, len, priv->tx.obj_num - mcp25xxfd_get_tef_tail(priv));
}

static inline u8 mcp25xxfd_get_tx_head(const struct mcp25xxfd_priv *priv)
{
	return priv->tx.head & (priv->tx.obj_num - 1);
}

static inline u8 mcp25xxfd_get_tx_tail(const struct mcp25xxfd_priv *priv)
{
	return priv->tx.tail & (priv->tx.obj_num - 1);
}

static inline u8 mcp25xxfd_get_rx_head(const struct mcp25xxfd_priv *priv)
{
	return priv->rx.head & (priv->rx.obj_num - 1);
}

static inline u8 mcp25xxfd_get_rx_tail(const struct mcp25xxfd_priv *priv)
{
	return priv->rx.tail & (priv->rx.obj_num - 1);
}

static inline u8 mcp25xxfd_get_rx_len(const struct mcp25xxfd_priv *priv)
{
	return priv->rx.head - priv->rx.tail;
}

static inline u8 mcp25xxfd_get_rx_linear_len(const struct mcp25xxfd_priv *priv)
{
	u8 len;

	len = mcp25xxfd_get_rx_len(priv);

	return min_t(u8, len, priv->rx.obj_num - mcp25xxfd_get_rx_tail(priv));
}

int mcp25xxfd_regmap_init(struct mcp25xxfd_priv *priv);

#endif
