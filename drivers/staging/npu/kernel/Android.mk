##############################################################################
#
#    Copyright (c) 2005 - 2020 by Vivante Corp.  All rights reserved.
#
#    The material in this file is confidential and contains trade secrets
#    of Vivante Corporation. This is proprietary information owned by
#    Vivante Corporation. No part of this work may be disclosed,
#    reproduced, copied, transmitted, or used in any way for any purpose,
#    without the express written permission of Vivante Corporation.
#
##############################################################################


LOCAL_PATH := $(call my-dir)
include $(LOCAL_PATH)/../../Android.mk.def

ifeq ($(VIVANTE_ENABLE_VSIMULATOR),0)


#
# galcore.ko
#
include $(CLEAR_VARS)

GALCORE_OUT := $(TARGET_OUT_INTERMEDIATES)/GALCORE_OBJ
GALCORE := \
	$(GALCORE_OUT)/galcore.ko

KERNELENVSH := $(GALCORE_OUT)/kernelenv.sh
$(KERNELENVSH):
	rm -rf $(KERNELENVSH)
	echo 'export KERNEL_DIR=$(KERNEL_DIR)' > $(KERNELENVSH)
	echo 'export CROSS_COMPILE=$(CROSS_COMPILE)' >> $(KERNELENVSH)
	echo 'export ARCH_TYPE=$(ARCH_TYPE)' >> $(KERNELENVSH)

GALCORE_LOCAL_PATH := $(LOCAL_PATH)

$(GALCORE): $(KERNELENVSH)
	@cd $(AQROOT)
	source $(KERNELENVSH); $(MAKE) -f Kbuild -C $(AQROOT) \
		AQROOT=$(abspath $(AQROOT)) \
		AQARCH=$(abspath $(AQARCH)) \
		AQVGARCH=$(abspath $(AQVGARCH)) \
		ARCH_TYPE=$(ARCH_TYPE) \
		DEBUG=$(DEBUG) \
		VIVANTE_ENABLE_DRM=$(DRM_GRALLOC) \
		VIVANTE_ENABLE_2D=$(VIVANTE_ENABLE_2D) \
		VIVANTE_ENABLE_3D=$(VIVANTE_ENABLE_3D) \
		VIVANTE_ENABLE_VG=$(VIVANTE_ENABLE_VG); \
		cp $(GALCORE_LOCAL_PATH)/../../galcore.ko $(GALCORE)

LOCAL_PREBUILT_MODULE_FILE := \
	$(GALCORE)

LOCAL_GENERATED_SOURCES := \
	$(AQREG) \
	$(VG_AQREG)

LOCAL_GENERATED_SOURCES += \
	$(GALCORE)

ifeq ($(shell expr $(PLATFORM_SDK_VERSION) ">=" 21),1)
  LOCAL_MODULE_RELATIVE_PATH := modules
else
  LOCAL_MODULE_PATH          := $(TARGET_OUT_SHARED_LIBRARIES)/modules
endif

LOCAL_MODULE        := galcore
LOCAL_MODULE_SUFFIX := .ko
LOCAL_MODULE_TAGS   := optional
LOCAL_MODULE_CLASS  := SHARED_LIBRARIES
LOCAL_STRIP_MODULE  := false
include $(BUILD_PREBUILT)

include $(AQROOT)/copy_installed_module.mk

else

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
          gc_hal_kernel_command.c \
          gc_hal_kernel_db.c \
          gc_hal_kernel_debug.c \
          gc_hal_kernel_event.c \
          gc_hal_kernel_heap.c \
          gc_hal_kernel.c \
          gc_hal_kernel_mmu.c \
          gc_hal_kernel_video_memory.c

LOCAL_SRC_FILES += \
          gc_hal_kernel_security.c \
          gc_hal_kernel_security_v1.c \

LOCAL_CFLAGS := \
    $(CFLAGS) \
    -DNO_VDT_PROXY

LOCAL_C_INCLUDES := \
	$(AQROOT)/vsimulator/os/linux/inc \
	$(AQROOT)/vsimulator/os/linux/emulator \
    $(AQROOT)/hal/inc \
	$(AQROOT)/hal/user \
	$(AQROOT)/hal/kernel/arch \
	$(AQROOT)/hal/kernel \

LOCAL_MODULE         := libhalkernel

LOCAL_MODULE_TAGS    := optional

LOCAL_PRELINK_MODULE := false

ifeq ($(PLATFORM_VENDOR),1)
LOCAL_VENDOR_MODULE  := true
endif
include $(BUILD_STATIC_LIBRARY)

endif

