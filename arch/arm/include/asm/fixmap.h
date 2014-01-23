#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H

/*
 * Nothing too fancy for now.
 *
 * On ARM we already have well known fixed virtual addresses imposed by
 * the architecture such as the vector page which is located at 0xffff0000,
 * therefore a second level page table is already allocated covering
 * 0xfff00000 upwards.
 *
 * The cache flushing code in proc-xscale.S uses the virtual area between
 * 0xfffe0000 and 0xfffeffff.
 */

#define FIXADDR_START		0xfff00000UL
#define FIXADDR_END		0xfffe0000UL
#define FIXADDR_TOP		(FIXADDR_END - PAGE_SIZE)

enum fixed_addresses {
	FIX_KMAP_BEGIN,
	FIX_KMAP_END = (FIXADDR_TOP - FIXADDR_START) >> PAGE_SHIFT,
	__end_of_fixed_addresses
};

#include <asm-generic/fixmap.h>

#endif
