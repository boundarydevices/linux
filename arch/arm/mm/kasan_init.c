/*
 * This file contains kasan initialization code for ARM64.
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Author: Andrey Ryabinin <ryabinin.a.a@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) "kasan: " fmt
#include <linux/kasan.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/start_kernel.h>
#include <linux/mm.h>

#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/tlbflush.h>
#include <asm/mach/map.h>
#include <asm/fixmap.h>

#define PGD_SIZE	(PTRS_PER_PGD * sizeof(pgd_t))

static pgd_t tmp_pg_dir[PTRS_PER_PGD] __initdata __aligned(PGD_SIZE);

/*
 * The p*d_populate functions call virt_to_phys implicitly so they can't be used
 * directly on kernel symbols (bm_p*d). All the early functions are called too
 * early to use lm_alias so __p*d_populate functions must be used to populate
 * with the physical address from __pa_symbol.
 */

static void __init kasan_early_pte_populate(pmd_t *pmd, unsigned long addr,
					unsigned long end)
{
	pte_t *pte;
	unsigned long next;
	pgprot_t kernel_pte = __pgprot(L_PTE_PRESENT | L_PTE_YOUNG |
				       L_PTE_SHARED  | L_PTE_DIRTY |
				       L_PTE_MT_WRITEALLOC);

	if (pmd_none(*pmd))
		__pmd_populate(pmd, __pa_symbol(kasan_zero_pte),
			       _PAGE_KERNEL_TABLE);

	pte = pte_offset_kernel(pmd, addr);
	do {
		next = addr + PAGE_SIZE;
		cpu_v7_set_pte_ext(pte, pfn_pte(sym_to_pfn(kasan_zero_page),
				   kernel_pte),
				   0);
	} while (pte++, addr = next, addr != end && pte_none(*pte));
}

static void __init kasan_early_pmd_populate(pud_t *pud,
					unsigned long addr,
					unsigned long end)
{
	pmd_t *pmd;
	unsigned long next;

	if (pud_none(*pud))
		pud_populate(pud, __pa_symbol(kasan_zero_pmd), PMD_TYPE_TABLE);

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		kasan_early_pte_populate(pmd, addr, next);
	} while (pmd++, addr = next, addr != end && pmd_none(*pmd));
}

static void __init kasan_early_pud_populate(pgd_t *pgd,
					unsigned long addr,
					unsigned long end)
{
	pud_t *pud;
	unsigned long next;

	if (pgd_none(*pgd))
		pgd_populate(pgd, __pa_symbol(kasan_zero_pud), PUD_TYPE_TABLE);

	pud = pud_offset(pgd, addr);
	do {
		next = pud_addr_end(addr, end);
		kasan_early_pmd_populate(pud, addr, next);
	} while (pud++, addr = next, addr != end && pud_none(*pud));
}

static void __init kasan_map_early_shadow(unsigned long start,
					  unsigned long end)
{
	unsigned long addr = start;
	unsigned long next;
	pgd_t *pgd;

	pgd = pgd_offset_k(addr);
	do {
		next = pgd_addr_end(addr, end);
		kasan_early_pud_populate(pgd, addr, next);
	} while (pgd++, addr = next, addr != end);
}

asmlinkage void __init kasan_early_init(void)
{
	BUILD_BUG_ON(!IS_ALIGNED(KASAN_SHADOW_START, PGDIR_SIZE));
	BUILD_BUG_ON(!IS_ALIGNED(KASAN_SHADOW_END, PGDIR_SIZE));
	kasan_map_early_shadow(KASAN_SHADOW_START, KASAN_SHADOW_END);
}

static inline pmd_t *pmd_off_k(unsigned long virt)
{
	return pmd_offset(pud_offset(pgd_offset_k(virt), virt), virt);
}

static void __init clear_pmds(unsigned long start,
			unsigned long end)
{
	/*
	 * Remove references to kasan page tables from
	 * swapper_pg_dir. pmd_clear() can't be used
	 * here because it's nop on 2,3-level pagetable setups
	 */
	for (; start < end; start += PGDIR_SIZE)
		pmd_clear(pmd_off_k(start));
}

static void kasan_alloc_and_map_shadow(unsigned long start, unsigned long end)
{
	struct map_desc desc;
	unsigned long size;
	phys_addr_t l_shadow;

	size  = (end - start) >> KASAN_SHADOW_SCALE_SHIFT;
	l_shadow = memblock_alloc(size, SECTION_SIZE);
	WARN(!l_shadow, "%s, reserve %ld shadow memory failed",
		__func__, size);

	desc.virtual = (unsigned long)kasan_mem_to_shadow((void *)start);
	desc.pfn     = __phys_to_pfn(l_shadow);
	desc.length  = size;
	desc.type    = MT_MEMORY_RW;
	create_mapping(&desc);
	pr_info("KASAN shadow, virt:[%lx-%lx], phys:%x, size:%lx\n",
		start, end, l_shadow, size);
}

void __init kasan_init(void)
{
	unsigned long start, end;
	int i;

	/*
	 * We are going to perform proper setup of shadow memory.
	 * At first we should unmap early shadow (clear_pmds() call bellow).
	 * However, instrumented code couldn't execute without shadow memory.
	 * tmp_pg_dir used to keep early shadow mapped until full shadow
	 * setup will be finished.
	 */
	memcpy(tmp_pg_dir, swapper_pg_dir, sizeof(tmp_pg_dir));
	dsb(ishst);
	cpu_switch_mm(tmp_pg_dir, &init_mm);
	clear_pmds(KASAN_SHADOW_START, KASAN_SHADOW_END);

	kasan_alloc_and_map_shadow(PAGE_OFFSET, KMEM_END);
	kasan_alloc_and_map_shadow(FIXADDR_START, FIXADDR_END);
#ifdef CONFIG_HIGHMEM
	kasan_alloc_and_map_shadow(PKMAP_BASE,
				   PKMAP_BASE + LAST_PKMAP * PAGE_SIZE);
#endif

	/*
	 * populate zero page for vmalloc area and other gap area
	 * TODO:
	 *      Need check kasan for vmalloc?
	 */
	start = (ulong)kasan_mem_to_shadow((void *)MODULES_VADDR);
	kasan_map_early_shadow(KASAN_SHADOW_START, start);

	start = (ulong)kasan_mem_to_shadow((void *)KMEM_END);
	end   = (ulong)kasan_mem_to_shadow((void *)FIXADDR_START);
	kasan_map_early_shadow(start, end);

	/*
	 * KAsan may reuse the contents of kasan_zero_pte directly, so we
	 * should make sure that it maps the zero page read-only.
	 */
	for (i = 0; i < PTRS_PER_PTE; i++)
		set_pte_ext(&kasan_zero_pte[i],
			    pfn_pte(sym_to_pfn(kasan_zero_page),
				    PAGE_KERNEL_RO), 0);

	memset(kasan_zero_page, 0, PAGE_SIZE);
	cpu_switch_mm(swapper_pg_dir, &init_mm);
	local_flush_tlb_all();
	flush_cache_all();

	/* clear all shawdow memory before kasan running */
	memset(kasan_mem_to_shadow((void *)PAGE_OFFSET), 0,
	       (KMEM_END - PAGE_OFFSET) >> KASAN_SHADOW_SCALE_SHIFT);
	memset(kasan_mem_to_shadow((void *)FIXADDR_START), 0,
	       (FIXADDR_END - FIXADDR_START) >> KASAN_SHADOW_SCALE_SHIFT);
#ifdef CONFIG_HIGHMEM
	memset(kasan_mem_to_shadow((void *)PKMAP_BASE), 0,
	       (LAST_PKMAP * PAGE_SIZE) >> KASAN_SHADOW_SCALE_SHIFT);
#endif

	/* At this point kasan is fully initialized. Enable error messages */
	init_task.kasan_depth = 0;
	pr_info("KernelAddressSanitizer initialized\n");
}
