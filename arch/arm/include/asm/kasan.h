#ifndef __ASM_KASAN_H
#define __ASM_KASAN_H

#ifndef __ASSEMBLY__

#ifdef CONFIG_KASAN

#include <linux/linkage.h>
#include <asm/memory.h>

#define sym_to_pfn(x)		__phys_to_pfn(__pa_symbol(x))
#define PAGE_KERNEL_RO		_MOD_PROT(pgprot_kernel, L_PTE_RDONLY)

/*
 * KASAN_SHADOW_START: beginning of the kernel virtual addresses.
 * KASAN_SHADOW_END: KASAN_SHADOW_START + 1/8 of kernel virtual addresses.
 *
 * For 32bit KASAN, we using a fixed Memory map here
 *
 *  0x00000000 +--------+
 *             |        |
 *             |        |
 *             |        |  User space memory,         2944MB
 *             |        |
 *             |        |
 *  0xb8000000 +--------+
 *             |        |  Kasan shaddow memory,       128MB
 *  0xc0000000 +--------+
 *             |        |  Vmalloc address,            240MB
 *             |        |
 *  0xCF400000 +--------+
 *  0xCF600000 +--------+  PKmap, for kmap               2MB
 *  0xD0000000 +--------+  Module and pkmap,            10MB
 *             |        |
 *             |        |  Kernel linear mapped space, 762MB
 *  0xFFa00000 +--------+
 *  0xFFFc0000 +--------+  static map,                   2MB
 *  0xFFF00000 +--------+  Fixed map, for kmap_atomic,   3MB
 *  0xFFFF0000 +--------+  High vector,                  4KB
 *
 */
#define KADDR_SIZE		(SZ_1G)
#define KASAN_SHADOW_SIZE	(KADDR_SIZE >> 3)
#define KASAN_SHADOW_START	(TASK_SIZE)
#define KASAN_SHADOW_END	(KASAN_SHADOW_START + KASAN_SHADOW_SIZE)

/*
 * This value is used to map an address to the corresponding shadow
 * address by the following formula:
 *     shadow_addr = (address >> 3) + KASAN_SHADOW_OFFSET;
 *
 */
#define KASAN_SHADOW_OFFSET	(KASAN_SHADOW_START - (VMALLOC_START >> 3))
struct map_desc;
void kasan_init(void);
void kasan_copy_shadow(pgd_t *pgdir);
asmlinkage void kasan_early_init(void);
void cpu_v7_set_pte_ext(pte_t *ptep, pte_t pte, unsigned int ext);
void create_mapping(struct map_desc *md);
#else
static inline void kasan_init(void) { }
static inline void kasan_copy_shadow(pgd_t *pgdir) { }
#endif

#endif
#endif
