#!/usr/bin/make -f
# -*- indent-tabs-mode: t; tab-width: 3 -*-

# kernel customizer v1.5

include common.mk

#############

$(BOOT_DIR):
	@mkdir -p $(IMAGE_DIR)/boot

arm-cross-toolchain:
	@$(call message,"installing arm-cross-toolchain",0,$(COLOR_BLUE),$(INVERSE))
	if [ -f /etc/alternatives/cpp ] ; then sudo update-alternatives --remove  cpp  /usr/bin/cpp ; fi
ifeq ($(DISTRO),trusty)
	sudo apt-get install g++-4.8-arm-linux-gnueabihf
	sudo apt-get install gcc-4.8-multilib-arm-linux-gnueabihf
	$(call upd-alternatives,4.8,90,$(CROSS_COMPILE))
else ifeq ($(DISTRO),xenial)
	sudo apt install g++-4.8-arm-linux-gnueabihf
	sudo apt install gcc-4.8-multilib-arm-linux-gnueabihf
	$(call upd-alternatives,4.8,90,$(CROSS_COMPILE))
	sudo apt install g++-4.9-arm-linux-gnueabihf
	sudo apt install gcc-4.9-multilib-arm-linux-gnueabihf
	$(call upd-alternatives,4.9,100,$(CROSS_COMPILE))
	sudo apt install g++-5-arm-linux-gnueabihf
	sudo apt install gcc-5-multilib-arm-linux-gnueabihf
	$(call upd-alternatives,5,110,$(CROSS_COMPILE))
else ifeq ($(DISTRO),jessie)
	sudo apt-get install libc6-dev-i386 lib32stdc++-4.9-dev
	sudo apt-get install g++-4.9-arm-linux-gnueabihf
	$(call upd-alternatives,4.9,100,$(CROSS_COMPILE))
else ifeq ($(DISTRO),stretch)
	sudo apt install libc6-dev-i386 lib32stdc++-6-dev
	sudo apt install g++-6-arm-linux-gnueabihf
	$(call upd-alternatives,6,110,$(CROSS_COMPILE))
else
	$(error unknown DISTRO=$(DISTRO))
endif



prerequisites: arm-cross-toolchain
	@$(call message,"installing prerequisites",0,$(COLOR_BLUE),$(INVERSE))
	@sudo apt-get -y install build-essential dpkg-cross git git-svn gzip pigz pixz
	@echo prerequisites installed


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


targz:
	@$(call message,"creating tarball",0,$(COLOR_BLUE),$(INVERSE))
	@(cd $(BUILD_DIR) && \
	  KR=`cat $(BUILD_DIR)/include/config/kernel.release` && \
	  cd $(IMAGE_DIR) && tar -czf ../linux-$${KR}.tar.gz * && \
	  $(call message,"tarball linux-$${KR}.tar.gz has been created.",0,$(COLOR_GREEN),$(NONINVERSE)) ; \
	 )


tarball: all targz


clean:
	@$(call message,"cleaning...",0,$(COLOR_BLUE),$(INVERSE))
	@rm -rf $(IMAGE_DIR)
	@rm -f *.tar.gz create-initrd.sh

.PHONY: clean
