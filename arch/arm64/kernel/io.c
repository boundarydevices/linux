/*
 * Based on arch/arm/kernel/io.c
 *
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/export.h>
#include <linux/types.h>
#define SKIP_IO_TRACE
#include <linux/io.h>
#undef SKIP_IO_TRACE
#ifdef CONFIG_AMLOGIC_DEBUG_FTRACE_PSTORE
#include <linux/amlogic/debug_ftrace_ramoops.h>
#endif

/*
 * Copy data from IO memory space to "real" memory space.
 */
void __memcpy_fromio(void *to, const volatile void __iomem *from, size_t count)
{
#ifdef CONFIG_AMLOGIC_DEBUG_FTRACE_PSTORE
	pstore_ftrace_io_rd((unsigned long)addr);
#endif
	while (count && !IS_ALIGNED((unsigned long)from, 8)) {
		*(u8 *)to = __raw_readb(from);
		from++;
		to++;
		count--;
	}

	while (count >= 8) {
		*(u64 *)to = __raw_readq(from);
		from += 8;
		to += 8;
		count -= 8;
	}

	while (count) {
		*(u8 *)to = __raw_readb(from);
		from++;
		to++;
		count--;
	}
#ifdef CONFIG_AMLOGIC_DEBUG_FTRACE_PSTORE
	pstore_ftrace_io_rd_end((unsigned long)addr);
#endif
}
EXPORT_SYMBOL(__memcpy_fromio);

/*
 * Copy data from "real" memory space to IO memory space.
 */
void __memcpy_toio(volatile void __iomem *to, const void *from, size_t count)
{
#ifdef CONFIG_AMLOGIC_DEBUG_FTRACE_PSTORE
	pstore_ftrace_io_wr((unsigned long)addr, 0x1234);
#endif
	while (count && !IS_ALIGNED((unsigned long)to, 8)) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}

	while (count >= 8) {
		__raw_writeq(*(u64 *)from, to);
		from += 8;
		to += 8;
		count -= 8;
	}

	while (count) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}
#ifdef CONFIG_AMLOGIC_DEBUG_FTRACE_PSTORE
	pstore_ftrace_io_wr_end((unsigned long)addr, 0x1234);
#endif
}
EXPORT_SYMBOL(__memcpy_toio);

/*
 * "memset" on IO memory space.
 */
void __memset_io(volatile void __iomem *dst, int c, size_t count)
{
	u64 qc = (u8)c;

#ifdef CONFIG_AMLOGIC_DEBUG_FTRACE_PSTORE
	pstore_ftrace_io_wr((unsigned long)addr, 0xabcd);
#endif
	qc |= qc << 8;
	qc |= qc << 16;
	qc |= qc << 32;

	while (count && !IS_ALIGNED((unsigned long)dst, 8)) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}

	while (count >= 8) {
		__raw_writeq(qc, dst);
		dst += 8;
		count -= 8;
	}

	while (count) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}
#ifdef CONFIG_AMLOGIC_DEBUG_FTRACE_PSTORE
	pstore_ftrace_io_wr_end((unsigned long)addr, 0xabcd);
#endif
}
EXPORT_SYMBOL(__memset_io);
