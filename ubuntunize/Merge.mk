#!/usr/bin/make -f
# -*- indent-tabs-mode: t; tab-width: 3 -*-

include common.mk


IMG_MOUNT  = /tmp/image-loop
LOOP_DEV   = $(shell sudo losetup -f)
OFFSET     = $(shell expr 2048 \* 512)
UBUNTUNIZE = $(TOPDIR)
KERNEL_REL = $(shell cat $(BUILD_DIR)/include/config/kernel.release)
PBUILDERSATISFYDEPENDSCMD = $(TOPDIR)/pbuilder-satisfydepends-gdebi
PB_CONFIG  = $(UBUNTUNIZE)/pbuilderrc
PB_EXEC    = sudo pbuilder --execute --configfile $(PB_CONFIG) --no-targz --buildplace $(IMG_MOUNT) --bindmounts "/tmp $(UBUNTUNIZE)"

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(ROOTFS_IMG),)
   $(error variable ROOTFS_IMG must be defined)
endif
endif


prerequisites:
	@$(call message,"installing prerequisites",0,$(COLOR_BLUE),$(INVERSE))
	@sudo apt-get -y install pbuilder devscripts debootstrap qemu qemu-user-static binfmt-support
	@echo prerequisites installed


$(ROOTFS_IMG):
	@(if [ ! -f "$(ROOTFS_IMG).gz" ] ; then  \
	    $(call message,"file not found: ROOTFS_IMG=$(ROOTFS_IMG).gz",0,$(COLOR_RED),$(NONINVERSE)) ; \
	    exit 3 ;                             \
	  else                                   \
	    $(call message,"extracting: $(ROOTFS_IMG).gz",0,$(COLOR_BLUE),$(INVERSE)) ; \
	    gunzip -k -f $(ROOTFS_IMG).gz ;      \
	  fi)


$(IMG_MOUNT):
	@$(call message,"creating directory $(IMG_MOUNT)",0,$(COLOR_BLUE),$(INVERSE))
	@(if [ -d $@ ] ; then rm -rf $@; fi)
	@mkdir -p $@


$(UBUNTUNIZE)/create-initrd.sh: $(UBUNTUNIZE)/create-initrd.sh.in
	@cp $< $@
	@sed "s,@KERNEL_REL@,$(KERNEL_REL),g" -i $@


gzip:
	@$(call message,"compressing $(ROOTFS_IMG)",0,$(COLOR_BLUE),$(INVERSE))
	@pigz $(ROOTFS_IMG)


all: prerequisites $(ROOTFS_IMG) $(IMG_MOUNT) $(UBUNTUNIZE)/create-initrd.sh $(PB_CONFIG)
	@$(call message,"using loop device: $(LOOP_DEV)",0,$(COLOR_BLUE),$(INVERSE))
	@sudo losetup -o $(OFFSET) $(LOOP_DEV) $(ROOTFS_IMG)
#
	@$(call message,"mounting image $(ROOTFS_IMG) to $(IMG_MOUNT)",0,$(COLOR_BLUE),$(INVERSE))
	@sudo mount -t ext4 $(LOOP_DEV) $(IMG_MOUNT)
#
	@$(call message,"executing script uninstall-kernel.sh",0,$(COLOR_BLUE),$(INVERSE))
	@($(PB_EXEC) -- $(UBUNTUNIZE)/uninstall-kernel.sh)
#
	@$(call message,"copying new kernel in place",0,$(COLOR_BLUE),$(INVERSE))
	@sudo cp -a $(IMAGE_DIR)/* $(IMG_MOUNT)
#
	@$(call message,"executing script create-initrd.sh",0,$(COLOR_BLUE),$(INVERSE))
	@($(PB_EXEC) -- $(UBUNTUNIZE)/create-initrd.sh)
#
	@$(call message,"executing script apt-upgrade.sh",0,$(COLOR_BLUE),$(INVERSE))
	@($(PB_EXEC) -- $(UBUNTUNIZE)/apt-upgrade.sh)
#
	@$(call message,"unmounting image",0,$(COLOR_BLUE),$(INVERSE))
	@sudo umount $(IMG_MOUNT)
#
	@$(call message,"detaching loop device:$(LOOP_DEV)",0,$(COLOR_BLUE),$(INVERSE))
	@sudo losetup --detach $(LOOP_DEV)


clean:
	@rm -f $(ROOTFS_IMG) $(UBUNTUNIZE)/create-initrd.sh
	@rm -rf $(IMG_MOUNT)


.PHONY: all prerequisites clean distclean $(IMG_MOUNT) $(UBUNTUNIZE)/create-initrd.sh
