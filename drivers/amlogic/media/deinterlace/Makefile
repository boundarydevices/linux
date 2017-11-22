# # Makefile for the Post Process Manager device #
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
ccflags-y := -D DEBUG_SUPPORT
endif
CFLAGS_deinterlace.o := -I$(src)
obj-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += film_mode_fmw/
obj-$(CONFIG_AMLOGIC_MEDIA_DEINTERLACE) += di.o
di-objs += deinterlace.o
di-objs += deinterlace_hw.o
di-objs += deinterlace_mtn.o
di-objs += deinterlace_dbg.o
di-objs += nr_drv.o
di-objs += pulldown_drv.o
di-objs += detect3d.o
di-objs += detect3d.o
ccflags-y += -Idrivers/amlogic/media/common/rdma/

