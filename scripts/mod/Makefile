OBJECT_FILES_NON_STANDARD := y

hostprogs-y	:= modpost mk_elfconfig
always		:= $(hostprogs-y) empty.o

modpost-objs	:= modpost.o file2alias.o sumversion.o

devicetable-offsets-file := devicetable-offsets.h

include $(srctree)/scripts/Makefile.asm-offsets

quiet_cmd_offsets = GEN     $@
define cmd_offsets
	$(call gen_header_from_asm_offsets,__DEVICETABLE_OFFSETS_H__) < $< > $@
endef

$(obj)/$(devicetable-offsets-file): $(obj)/devicetable-offsets.s
	$(call if_changed,offsets)

targets += $(devicetable-offsets-file) devicetable-offsets.s

# dependencies on generated files need to be listed explicitly

$(obj)/modpost.o $(obj)/file2alias.o $(obj)/sumversion.o: $(obj)/elfconfig.h
$(obj)/file2alias.o: $(obj)/$(devicetable-offsets-file)

quiet_cmd_elfconfig = MKELF   $@
      cmd_elfconfig = $(obj)/mk_elfconfig < $< > $@

$(obj)/elfconfig.h: $(obj)/empty.o $(obj)/mk_elfconfig FORCE
	$(call if_changed,elfconfig)

targets += elfconfig.h
