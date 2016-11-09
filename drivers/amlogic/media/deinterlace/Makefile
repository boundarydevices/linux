#
# Makefile for the Post Process Manager device
#
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y := -D DEBUG_SUPPORT
endif
obj-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di.o
di-objs += deinterlace.o
di-objs += deinterlace_hw.o
di-objs += deinterlace_mtn.o
di-objs += deinterlace_dbg.o
di-objs += nr.o
di-objs += vof_soft_top.o
di-objs += film_fw1.o
di-objs += flm_mod_xx.o
di-objs += detect3d.o
di-objs += tffbff_check.o
ccflags-y += -Idrivers/amlogic/media/common/rdma/

