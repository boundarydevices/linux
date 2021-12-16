
targets += dtbs

dtb-y += g12b_a311d_w400.dtb
dtb-y += g12b_a311d_w400_a.dtb

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

always		:= $(DTB_LIST)
subdir-y	:= $(dts-dirs)

clean-files := *.dtb
