/*
 * include/linux/amlogic/media/utils/vdec_reg.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef VDEC_REG_H
#define VDEC_REG_H

#include <linux/kernel.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/registers/register_ops.h>

#define READ_DMCREG(r) codec_dmcbus_read(r)
#define WRITE_DMCREG(r, val) codec_dmcbus_write(r, val)

#define READ_AOREG(r) codec_aobus_read(r)
#define WRITE_AOREG(r, val) codec_aobus_write(r, val)

#define READ_VREG(r) codec_dosbus_read(r)
#define WRITE_VREG(r, val) codec_dosbus_write(r, val)

#define BASE_IRQ 32
#define AM_IRQ(reg)   (reg + BASE_IRQ)
#define INT_DOS_MAILBOX_0       AM_IRQ(43)
#define INT_DOS_MAILBOX_1       AM_IRQ(44)
#define INT_DOS_MAILBOX_2       AM_IRQ(45)
#define INT_VIU_VSYNC           AM_IRQ(3)
#define INT_DEMUX               AM_IRQ(23)
#define INT_DEMUX_1             AM_IRQ(5)
#define INT_DEMUX_2             AM_IRQ(53)
#define INT_ASYNC_FIFO_FILL     AM_IRQ(18)
#define INT_ASYNC_FIFO_FLUSH    AM_IRQ(19)
#define INT_ASYNC_FIFO2_FILL    AM_IRQ(24)
#define INT_ASYNC_FIFO2_FLUSH   AM_IRQ(25)

#define INT_PARSER              AM_IRQ(32)

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */

/*
 *#define READ_AOREG(r) (__raw_readl((volatile void __iomem *)\
 *AOBUS_REG_ADDR(r)))
 */

/*
 *#define WRITE_AOREG(r, val) __raw_writel(val,\
 *   (volatile void __iomem *)(AOBUS_REG_ADDR(r)))'
 */

/* aml_read_vcbus(unsigned int reg) */
#define INT_VDEC INT_DOS_MAILBOX_1
#define INT_VDEC2 INT_DOS_MAILBOX_0

/* #else */

/* /#define INT_VDEC INT_MAILBOX_1A */

/* /#endif */

/* static inline u32 READ_VREG(u32 r) */

/* { */

/* if (((r) > 0x2000) && ((r) < 0x3000) && !vdec_on(2)) dump_stack(); */

/* return __raw_readl((volatile void __iomem *)DOS_REG_ADDR(r)); */

/* } */

/* static inline void WRITE_VREG(u32 r, u32 val) */

/* { */

/* if (((r) > 0x2000) && ((r) < 0x3000) && !vdec_on(2)) dump_stack(); */

/* __raw_writel(val, (volatile void __iomem *)(DOS_REG_ADDR(r))); */

/* } */

#define WRITE_VREG_BITS(r, val, start, len) \
	WRITE_VREG(r, (READ_VREG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))
#define SET_VREG_MASK(r, mask) WRITE_VREG(r, READ_VREG(r) | (mask))
#define CLEAR_VREG_MASK(r, mask) WRITE_VREG(r, READ_VREG(r) & ~(mask))

#define READ_HREG(r) codec_dosbus_read(r|0x1000)
#define WRITE_HREG(r, val) codec_dosbus_write(r|0x1000, val)

#define WRITE_HREG_BITS(r, val, start, len) \
	WRITE_HREG(r, (READ_HREG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))
#define SET_HREG_MASK(r, mask) WRITE_HREG(r, READ_HREG(r) | (mask))
#define CLEAR_HREG_MASK(r, mask) WRITE_HREG(r, READ_HREG(r) & ~(mask))

/*TODO */
#define READ_SEC_REG(r)
#define WRITE_SEC_REG(r, val)
#define WRITE_SEC_REG_BITS(r, val, start, len) \
	WRITE_SEC_REG(r, (READ_SEC_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))

#define WRITE_MPEG_REG(r, val)      codec_cbus_write(r, val)
#define READ_MPEG_REG(r) codec_cbus_read(r)
#define WRITE_MPEG_REG_BITS(r, val, start, len) \
	WRITE_MPEG_REG(r, (READ_MPEG_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))

#define CLEAR_MPEG_REG_MASK(r, mask)\
	WRITE_MPEG_REG(r, READ_MPEG_REG(r) & ~(mask))
#define SET_MPEG_REG_MASK(r, mask)\
	WRITE_MPEG_REG(r, READ_MPEG_REG(r) | (mask))

#define WRITE_PARSER_REG(r, val) codec_parsbus_write(r, val)
#define READ_PARSER_REG(r) codec_parsbus_read(r)
#define WRITE_PARSER_REG_BITS(r, val, start, len) \
	WRITE_PARSER_REG(r, (READ_PARSER_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))

#define CLEAR_PARSER_REG_MASK(r, mask)\
	WRITE_PARSER_REG(r, READ_PARSER_REG(r) & ~(mask))
#define SET_PARSER_REG_MASK(r, mask)\
	WRITE_PARSER_REG(r, READ_PARSER_REG(r) | (mask))

#define WRITE_HHI_REG(r, val)      codec_hhibus_write(r, val)
#define READ_HHI_REG(r) codec_hhibus_read(r)
#define WRITE_HHI_REG_BITS(r, val, start, len) \
	WRITE_HHI_REG(r, (READ_HHI_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))

#define WRITE_AIU_REG(r, val) codec_aiubus_write(r, val)
#define READ_AIU_REG(r) codec_aiubus_read(r)
#define WRITE_AIU_REG_BITS(r, val, start, len) \
	WRITE_AIU_REG(r, (READ_AIU_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))

#define CLEAR_AIU_REG_MASK(r, mask)\
	WRITE_AIU_REG(r, READ_AIU_REG(r) & ~(mask))
#define SET_AIU_REG_MASK(r, mask)\
	WRITE_AIU_REG(r, READ_AIU_REG(r) | (mask))

#define WRITE_DEMUX_REG(r, val) codec_demuxbus_write(r, val)
#define READ_DEMUX_REG(r) codec_demuxbus_read(r)
#define WRITE_DEMUX_REG_BITS(r, val, start, len) \
	WRITE_DEMUX_REG(r, (READ_DEMUX_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))

#define CLEAR_DEMUX_REG_MASK(r, mask)\
	WRITE_DEMUX_REG(r, READ_DEMUX_REG(r) & ~(mask))
#define SET_DEMUX_REG_MASK(r, mask)\
	WRITE_DEMUX_REG(r, READ_DEMUX_REG(r) | (mask))

#define WRITE_RESET_REG(r, val) codec_resetbus_write(r, val)
#define READ_RESET_REG(r) codec_resetbus_read(r)
#define WRITE_RESET_REG_BITS(r, val, start, len) \
	WRITE_RESET_REG(r, (READ_RESET_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))

#define CLEAR_RESET_REG_MASK(r, mask)\
	WRITE_RESET_REG(r, READ_RESET_REG(r) & ~(mask))
#define SET_RESET_REG_MASK(r, mask)\
	WRITE_RESET_REG(r, READ_RESET_REG(r) | (mask))

#define ASSIST_MBOX1_CLR_REG VDEC_ASSIST_MBOX1_CLR_REG
#define ASSIST_MBOX1_MASK VDEC_ASSIST_MBOX1_MASK
#define ASSIST_AMR1_INT0 VDEC_ASSIST_AMR1_INT0
#define ASSIST_AMR1_INT1 VDEC_ASSIST_AMR1_INT1
#define ASSIST_AMR1_INT2 VDEC_ASSIST_AMR1_INT2
#define ASSIST_AMR1_INT3 VDEC_ASSIST_AMR1_INT3
#define ASSIST_AMR1_INT4 VDEC_ASSIST_AMR1_INT4
#define ASSIST_AMR1_INT5 VDEC_ASSIST_AMR1_INT5
#define ASSIST_AMR1_INT6 VDEC_ASSIST_AMR1_INT6
#define ASSIST_AMR1_INT7 VDEC_ASSIST_AMR1_INT7
#define ASSIST_AMR1_INT8 VDEC_ASSIST_AMR1_INT8
#define ASSIST_AMR1_INT9 VDEC_ASSIST_AMR1_INT9

/*TODO reg*/
#define READ_VCBUS_REG(r) codec_vcbus_read(r)
#define WRITE_VCBUS_REG(r, val) codec_vcbus_write(r, val)
#define WRITE_VCBUS_REG_BITS(r, val, start, len)\
	WRITE_VCBUS_REG(r, (READ_VCBUS_REG(r) & ~(((1L<<(len))-1)<<(start)))|\
		    ((unsigned int)((val)&((1L<<(len))-1)) << (start)))
#define CLEAR_VCBUS_REG_MASK(r, mask)\
	WRITE_VCBUS_REG(r, READ_VCBUS_REG(r) & ~(mask))
#define SET_VCBUS_REG_MASK(r, mask)\
	WRITE_VCBUS_REG(r, READ_VCBUS_REG(r) | (mask))

/****************logo relative part *****************************************/
#define ASSIST_MBOX1_CLR_REG VDEC_ASSIST_MBOX1_CLR_REG
#define ASSIST_MBOX1_MASK VDEC_ASSIST_MBOX1_MASK
#define RESET_PSCALE        (1<<4)
#define RESET_IQIDCT        (1<<2)
#define RESET_MC            (1<<3)
#define MEM_BUFCTRL_MANUAL      (1<<1)
#define MEM_BUFCTRL_INIT        (1<<0)
#define MEM_LEVEL_CNT_BIT       18
#define MEM_FIFO_CNT_BIT        16
#define MEM_FILL_ON_LEVEL       (1<<10)
#define MEM_CTRL_EMPTY_EN       (1<<2)
#define MEM_CTRL_FILL_EN        (1<<1)
#define MEM_CTRL_INIT           (1<<0)


#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)
#else
extern int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
extern u32 VSYNC_RD_MPEG_REG(u32 adr);
extern int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
#endif
#endif /* VDEC_REG_H */
