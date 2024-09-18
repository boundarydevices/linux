/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 ARM Ltd.
 */
#ifndef __ASM_MODULE_H
#define __ASM_MODULE_H

#include <asm-generic/module.h>

#ifdef CONFIG_KVM
struct pkvm_module_section {
	void *start;
	void *end;
};

typedef s32 kvm_nvhe_reloc_t;
struct pkvm_module_ops;

struct pkvm_el2_module {
	struct pkvm_module_section text;
	struct pkvm_module_section bss;
	struct pkvm_module_section rodata;
	struct pkvm_module_section data;
	struct pkvm_module_section event_ids;
	struct pkvm_module_section sections;
	struct hyp_event *hyp_events;
	unsigned int nr_hyp_events;
	kvm_nvhe_reloc_t *relocs;
	struct list_head node;
	unsigned long token;
	unsigned int nr_relocs;
	int (*init)(const struct pkvm_module_ops *ops);
};

void kvm_apply_hyp_module_relocations(struct pkvm_el2_module *mod,
				      kvm_nvhe_reloc_t *begin,
				      kvm_nvhe_reloc_t *end);

#define ARM64_MODULE_KVM_ARCHDATA					\
	/* For pKVM hypervisor modules */				\
	struct pkvm_el2_module	hyp;
#else
#define ARM64_MODULE_KVM_ARCHDATA
#endif

struct mod_plt_sec {
	int			plt_shndx;
	int			plt_num_entries;
	int			plt_max_entries;
};

struct mod_arch_specific {
	struct mod_plt_sec	core;
	struct mod_plt_sec	init;

	/* for CONFIG_DYNAMIC_FTRACE */
	struct plt_entry	*ftrace_trampolines;

	ARM64_MODULE_KVM_ARCHDATA
};

u64 module_emit_plt_entry(struct module *mod, Elf64_Shdr *sechdrs,
			  void *loc, const Elf64_Rela *rela,
			  Elf64_Sym *sym);

u64 module_emit_veneer_for_adrp(struct module *mod, Elf64_Shdr *sechdrs,
				void *loc, u64 val);

struct plt_entry {
	/*
	 * A program that conforms to the AArch64 Procedure Call Standard
	 * (AAPCS64) must assume that a veneer that alters IP0 (x16) and/or
	 * IP1 (x17) may be inserted at any branch instruction that is
	 * exposed to a relocation that supports long branches. Since that
	 * is exactly what we are dealing with here, we are free to use x16
	 * as a scratch register in the PLT veneers.
	 */
	__le32	adrp;	/* adrp	x16, ....			*/
	__le32	add;	/* add	x16, x16, #0x....		*/
	__le32	br;	/* br	x16				*/
};

static inline bool is_forbidden_offset_for_adrp(void *place)
{
	return IS_ENABLED(CONFIG_ARM64_ERRATUM_843419) &&
	       cpus_have_const_cap(ARM64_WORKAROUND_843419) &&
	       ((u64)place & 0xfff) >= 0xff8;
}

struct plt_entry get_plt_entry(u64 dst, void *pc);

static inline const Elf_Shdr *find_section(const Elf_Ehdr *hdr,
				    const Elf_Shdr *sechdrs,
				    const char *name)
{
	const Elf_Shdr *s, *se;
	const char *secstrs = (void *)hdr + sechdrs[hdr->e_shstrndx].sh_offset;

	for (s = sechdrs, se = sechdrs + hdr->e_shnum; s < se; s++) {
		if (strcmp(name, secstrs + s->sh_name) == 0)
			return s;
	}

	return NULL;
}

#endif /* __ASM_MODULE_H */
