# kernel ubuntunizer v1.4

#message colors
COLOR_BLACK=30
COLOR_RED=31
COLOR_GREEN=32
COLOR_YELLOW=33
COLOR_BLUE=34
COLOR_MAGENTA=35
COLOR_CYAN=36
COLOR_WHITE=97
COLOR_DEFAULT=39
INVERSE=1
NONINVERSE=0

#usage: $(call message,<text>,<tabs>,<color>,<inverse>)
define message
	( if [ -z $(NOCOLOR) ] ; then echo -ne "\e[$(3)m" ; fi ; \
	if [ "$(4)" = "$(INVERSE)" ] ; then if [ -z $(NOCOLOR) ] ; then echo -ne "\e[7m" ; fi ; echo -n ">>>" ; fi ; \
	if [ "$(2)" -gt 0 ] && [ "$(2)" -lt 10 ] ; then for a in `seq "$(2)"` ; do echo -n "   " ; done ; fi ; \
 	echo -n " $(1) " ; \
	if [ "$(4)" = "$(INVERSE)" ] ; then echo -n "<<<" ; if [ -z $(NOCOLOR) ] ; then echo -ne "\e[27m" ; fi ; fi ; \
	echo ; \
	if [ -z $(NOCOLOR) ] ; then echo -ne "\e[39m" ; fi ; \
	)
endef

#usage: $(call patch_source,<patch_dir>,<source_dir>)
define patch_source
	(for PATCH_FILE in `find $(1) -maxdepth 1 -name '*.patch' | sort 2>/dev/null` ; do \
	    patch -p1 -d $(2) < $${PATCH_FILE} ; \
	  done \
	)
endef

#############

SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	else if [ -x /bin/bash ]; then echo /bin/bash; \
	else echo sh; fi; fi)
	
TOPDIR := $(shell pwd)

BUILD_DIR ?= $(TOPDIR)/..
IMAGE_DIR = $(TOPDIR)/linux-staging
BOOT_DIR  = $(IMAGE_DIR)/boot
CROSS_COMPILE ?= arm-linux-gnueabihf-
INSTALL_MOD_PATH ?= $(IMAGE_DIR)
LOADADDR ?= 10008000

export ARCH = arm
export CROSS_COMPILE
export INSTALL_MOD_PATH
export LOADADDR

JOBS := $(shell echo $$((`getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1`)))

.DEFAULT_GOAL := all

#############

$(BOOT_DIR):
	@mkdir -p $(IMAGE_DIR)/boot
	
kernel:
	@$(call message,"building kernel",0,$(COLOR_BLUE),$(INVERSE))
	@(if [ -d $(BUILD_DIR)/arch/arm/boot/dts ] ; then \
		$(MAKE) -C $(BUILD_DIR) zImage modules dtbs -j$(JOBS) ; \
	  else \
		$(MAKE) -C $(BUILD_DIR) zImage modules -j$(JOBS) ; \
	fi)

all: $(BOOT_DIR)
	@$(call message,"installing modules",0,$(COLOR_BLUE),$(INVERSE))
	@rm -rf $(IMAGE_DIR)/lib/modules/*
	@$(MAKE) -C $(BUILD_DIR) modules_install
	@(for symlink in `find ${INSTALL_MOD_PATH}/lib/modules/ -type l` ; do \
		rm -f $${symlink} ; \
	done)
	@(rm -rf $(BOOT_DIR)/*.dtb)
	@(if [ -d $(BUILD_DIR)/arch/arm/boot/dts ] ; then \
		(cd $(BUILD_DIR)/arch/arm/boot/dts && cp -t $(BOOT_DIR) imx6*.dtb) ; \
	fi)
	@(cd $(BUILD_DIR) && \
	  KR=`cat $(BUILD_DIR)/include/config/kernel.release` && \
	  cp $(BUILD_DIR)/arch/arm/boot/zImage $(BOOT_DIR)/vmlinuz-$${KR} && \
	  cp $(BUILD_DIR)/.config $(BOOT_DIR)/config-$${KR} && \
	  cd $(IMAGE_DIR) && rm -f vmlinuz initrd.img && \
	  ln -s boot/vmlinuz-$${KR} vmlinuz && ln -s boot/initrd.img-$${KR} initrd.img && \
	  $(call message,"Kernel image has been created in directory: $(IMAGE_DIR).",0,$(COLOR_GREEN),$(NONINVERSE)) ; \
	 )


tarball: all
	@(cd $(BUILD_DIR) && \
	  KR=`cat $(BUILD_DIR)/include/config/kernel.release` && \
	  cd $(IMAGE_DIR) && tar -czf ../linux-$${KR}.tar.gz * && \
	  $(call message,"Tarball linux-$${KR}.tar.gz has been created.",0,$(COLOR_GREEN),$(NONINVERSE)) ; \
	 )


clean:
	@$(call message,"cleaning...",0,$(COLOR_BLUE),$(INVERSE))
	@rm -rf $(IMAGE_DIR)
	@rm -f *.tar.gz

.PHONY: clean
