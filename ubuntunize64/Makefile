#!/usr/bin/make -f
# -*- indent-tabs-mode: t; tab-width: 3 -*-

# kernel customizer v1.6

include common.mk

#############

$(BOOT_DIR):
	@mkdir -p $(IMAGE_DIR)/boot

arm64-cross-toolchain:
	@$(call message,"installing arm64-cross-toolchain",0,$(COLOR_BLUE),$(INVERSE))
	if [ -f /etc/alternatives/cpp ] ; then sudo update-alternatives --remove  cpp  /usr/bin/cpp ; fi
	sudo apt install -y fakeroot kernel-wedge quilt ccache flex
	sudo apt install -y bison libssl-dev crossbuild-essential-arm64


prerequisites: arm64-cross-toolchain
	@$(call message,"installing prerequisites",0,$(COLOR_BLUE),$(INVERSE))
	@sudo apt-get -y install build-essential dpkg-cross gzip pigz pixz
	@echo prerequisites installed


kernel:
	@$(call message,"building kernel",0,$(COLOR_BLUE),$(INVERSE))
	$(MAKE) -C $(BUILD_DIR) Image modules dtbs -j$(JOBS) ;


all: $(BOOT_DIR)
	@$(call message,"installing modules",0,$(COLOR_BLUE),$(INVERSE))
	@rm -rf $(IMAGE_DIR)/lib/modules/*
	@$(MAKE) -C $(BUILD_DIR) modules_install
	@(for symlink in `find ${INSTALL_MOD_PATH}/lib/modules/ -type l` ; do \
		rm -f $${symlink} ; \
	done)
	@(rm -rf $(BOOT_DIR)/*.dtb)
	@(cd $(BUILD_DIR)/arch/arm64/boot/dts/freescale && cp -t $(BOOT_DIR) imx8*.dtb) ;
	@(cd $(BUILD_DIR) && \
	  KR=`cat $(BUILD_DIR)/include/config/kernel.release` && \
	  cp $(BUILD_DIR)/arch/arm64/boot/Image $(BOOT_DIR)/vmlinuz-$${KR} && \
	  cp $(BUILD_DIR)/.config $(BOOT_DIR)/config-$${KR} && \
	  cd $(IMAGE_DIR) && rm -f vmlinuz initrd.img && \
	  ln -s boot/vmlinuz-$${KR} vmlinuz && \
	  ln -s initrd.img-$${KR} boot/initrd.img && \
	  ln -s vmlinuz-$${KR} boot/Image && \
	  $(call message,"Kernel image has been created in directory: $(IMAGE_DIR).",0,$(COLOR_GREEN),$(NONINVERSE)) ; \
	 )



targz:
	@$(call message,"creating tarball",0,$(COLOR_BLUE),$(INVERSE))
	@(cd $(BUILD_DIR) && \
	  KR=`cat $(BUILD_DIR)/include/config/kernel.release` && \
	  cd $(IMAGE_DIR) && tar -czf ../linux-$${KR}.tar.gz * && \
	  $(call message,"tarball ubuntunize64/linux-$${KR}.tar.gz has been created.",0,$(COLOR_GREEN),$(NONINVERSE)) ; \
	 )


tarball: all targz


clean:
	@$(call message,"cleaning...",0,$(COLOR_BLUE),$(INVERSE))
	@rm -rf $(IMAGE_DIR)
	@rm -f *.tar.gz create-initrd.sh

.PHONY: clean
